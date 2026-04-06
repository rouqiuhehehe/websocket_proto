//
// Created by admin on 2026/3/11.
//

#include "tcp_server.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cerrno>
#include <ranges>

#include <memory>
#define PRINT_ERR(fn_name) std::cerr << std::string(#fn_name" failed: ") + strerror(errno) << std::endl

Tcp_server::Tcp_server(std::string server_addr, int port_) : port_(port_), server_addr_(std::move(server_addr)) {
    init_tcp_server();
    init_epoll();
}

Tcp_server::~Tcp_server() noexcept {
    close(listen_fd_);

    for (auto &tcp_client: clients_ | std::views::values) {
        Tcp_server::close_client(tcp_client);
    }
    close(epoll_fd_);
}

void Tcp_server::start(std::chrono::milliseconds timeout) {
    int ret = listen(listen_fd_, SOMAXCONN);
    if (ret != 0) {
        throw std::runtime_error("listen failed");
    }

    std::cout << "server start success on " << server_addr_ << ":" << port_ << std::endl;

    constexpr int BATCH_SIZE = 1024;
    constexpr int BUF_SIZE = 0xffff;
    char buf[BUF_SIZE] {};
    epoll_event event[BATCH_SIZE] {};
    epoll_event ev {};
    sockaddr client_addr {};
    socklen_t addr_len = sizeof(client_addr);
    while (!stop_) {
        int num = epoll_wait(
            epoll_fd_,
            reinterpret_cast<epoll_event *>(&event),
            BATCH_SIZE,
            static_cast<int>(timeout.count()));

        if (num < 0) {
            PRINT_ERR(epoll_wait);
            continue;
        }

        for (int i = 0; i < num; i++) {
            ev = event[i];
            if (ev.events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                auto client = clients_.find(ev.data.fd);
                if (client != clients_.end()) {
                    close_client(client->second);
                }
            } else if (ev.events & EPOLLIN) {
                if (ev.data.fd == listen_fd_) {
                    int remote_fd = accept(listen_fd_, &client_addr, &addr_len);
                    if (remote_fd == -1) {
                        PRINT_ERR(accept);
                        continue;
                    }
                    fcntl(remote_fd, F_SETFL, O_NONBLOCK);
                    auto client = decode_remote_client(&client_addr, remote_fd);
                    clients_.emplace(remote_fd, client);

                    if (connected_f) {
                        connected_f(client);
                    }
                    ev.data.fd = remote_fd;
                    ev.events = EPOLLIN | EPOLLET;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, remote_fd, &ev);
                } else {
                    auto client = clients_.find(ev.data.fd);
                    if (client == clients_.end()) {
                        PRINT_ERR(client find);
                        continue;
                    }
                    std::string m;
                    while (true) {
                        ret = recv(ev.data.fd, buf, BUF_SIZE, 0);
                        if (ret < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            }
                            PRINT_ERR(recv);
                        } else if (ret == 0) {
                            close_client(client->second);
                            break;
                        } else {
                            m.append(buf, ret);
                        }
                    }
                    // 特殊协议解析
                    if (!m.empty() && decode_proto_data(client->second, m)) {
                        // msg call back
                        recv_f_(client->second);
                    }
                }
            } else if (ev.events & EPOLLOUT) {
                auto client = clients_.find(ev.data.fd);
                if (client == clients_.end()) {
                    PRINT_ERR(client->find(ev.data.fd));
                    continue;
                }
                auto c = client->second;
                while (!c->send_buffer_queue_.empty()) {
                    auto send_buffer = c->send_buffer_queue_.front();
                    c->send_buffer_queue_.pop();
                    while (true) {
                        ret = send(ev.data.fd, send_buffer.c_str(), send_buffer.size(), 0);
                        if (ret < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                // 发送缓冲区满
                                // 等待 EPOLLOUT
                                break;
                            }
                            // 其他错误
                            close_client(c);
                            PRINT_ERR(send);
                            break;
                        }
                        if (ret == 0) {
                            close_client(c);
                            break;
                        }
                        if (ret >= send_buffer.size()) {
                            break;
                        }
                        send_buffer.erase(0, ret);
                    }
                }
                ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
                ev.data.fd = c->sockfd_;
                epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, c->sockfd_, &ev);
                if (sent_f_) {
                    sent_f_(client->second);
                }
            }
        }

        event_loop_once();
    }
}

void Tcp_server::stop() noexcept {
    stop_ = true;
}

void Tcp_server::register_recv_callback(Msg_callback_fn &&fn) {
    recv_f_ = std::forward<Msg_callback_fn>(fn);
}

void Tcp_server::register_connected_callback(Msg_callback_fn &&fn) {
    connected_f = std::forward<Msg_callback_fn>(fn);
}

void Tcp_server::register_disconnect_callback(Msg_callback_fn &&fn) {
    disconnect_f_ = std::forward<Msg_callback_fn>(fn);
}

void Tcp_server::register_sent_callback(Msg_callback_fn &&fn) {
    sent_f_ = std::forward<Msg_callback_fn>(fn);
}

void Tcp_server::send_buffer(tcp_client *client, const std::string &data) const {
    client->send_buffer_queue_.emplace(data);
    epoll_event ev {};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
    ev.data.fd = client->sockfd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client->sockfd_, &ev);
}

bool Tcp_server::encode_proto_data(tcp_client *, const std::string &data) { return true; }

bool Tcp_server::decode_proto_data(tcp_client *client, const std::string &data) {
    client->recv_buffer_ = data;
    return true;
}

tcp_client *Tcp_server::allocate_client() const {
    return new tcp_client;
}

void Tcp_server::init_tcp_server() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        throw std::runtime_error("socket create failed");
    }
    int opt = 1;
    // 设置bind 可重用socket
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 设置非阻塞socket
    fcntl(listen_fd_, F_SETFL, O_NONBLOCK);
    sockaddr_in addr {
        .sin_family = AF_INET,
        .sin_port = htons(port_)
    };
    inet_pton(AF_INET, server_addr_.c_str(), &addr.sin_addr);
    int ret = bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    if (ret != 0) {
        throw std::runtime_error("bind failed");
    }
}

void Tcp_server::init_epoll() {
    epoll_fd_ = epoll_create(SOMAXCONN);
    if (epoll_fd_ < 3) {
        throw std::runtime_error("epoll create failed");
    }
    epoll_event ev {};
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    ev.data.fd = listen_fd_;

    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
}

tcp_client *Tcp_server::decode_remote_client(sockaddr *sock, int fd) {
    sockaddr_in addr = *reinterpret_cast<sockaddr_in *>(sock);
    char ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    int port = ntohs(addr.sin_port);

    auto tcp_client = allocate_client();
    tcp_client->sockfd_ = fd;
    tcp_client->ip_ = ip;
    tcp_client->port_ = port;
    tcp_client->tcp_server_ = this;

    return tcp_client;
}

void Tcp_server::close_client(tcp_client *client) {
    close(client->sockfd_);

    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client->sockfd_, nullptr);
    clients_.erase(client->sockfd_);

    if (disconnect_f_) {
        disconnect_f_(client);
    }
    delete client;
}

void Tcp_server::event_loop_once() {}
