//
// Created by admin on 2026/3/12.
//

#ifndef WEBSOCKET_DECODER_TCP_CLIENT_H
#define WEBSOCKET_DECODER_TCP_CLIENT_H
#include <string>
#include <functional>
#include <queue>

class tcp_client;
using Msg_callback_fn = std::function<void(tcp_client *)>;

class Tcp_server;
class tcp_client {
protected:
    friend class Tcp_server;
    Tcp_server *tcp_server_{};
    std::queue<std::string> send_buffer_queue_;   // 做发送队列，用于在一次epoll_wait中做多次send操作
    std::string recv_buffer_;
    std::string ip_;
    int port_{};
    int sockfd_{};
    std::string id_;

public:
    tcp_client();

    virtual ~tcp_client() noexcept = default;

    [[nodiscard]] std::string get_ip() const {
        return ip_;
    }

    [[nodiscard]] int get_port() const {
        return port_;
    }

    [[nodiscard]] std::string get_id() const {
        return id_;
    }

    [[nodiscard]] virtual std::string get_message() const {
        return recv_buffer_;
    }

    virtual void send(const std::string &send_buffer);
    virtual void close();
};

#endif //WEBSOCKET_DECODER_TCP_CLIENT_H
