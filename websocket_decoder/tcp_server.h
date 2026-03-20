//
// Created by admin on 2026/3/11.
//

#ifndef WEBSOCKET_DECODER_TCP_SERVER_H
#define WEBSOCKET_DECODER_TCP_SERVER_H
#include <chrono>
#include <unordered_map>
#include "tcp_client.h"

using namespace std::chrono_literals;
struct sockaddr;

class Tcp_server {
protected:
    int listen_fd_ {};
    int port_;
    std::string server_addr_;
    int epoll_fd_ {};
    bool stop_ {};
    Msg_callback_fn connected_f;
    Msg_callback_fn recv_f_;
    Msg_callback_fn disconnect_f_;
    std::unordered_map<int, tcp_client *> clients_;

public:
    explicit Tcp_server(std::string server_addr = "0.0.0.0", int port_ = 8192);

    virtual ~Tcp_server() noexcept;

    void start(std::chrono::milliseconds timeout = -1ms);

    void stop() noexcept;

    void send_buffer(tcp_client *, const std::string &data) const;

    virtual void register_recv_callback(Msg_callback_fn &&fn);
    virtual void register_connected_callback(Msg_callback_fn &&fn);
    void register_disconnect_callback(Msg_callback_fn &&fn);

protected:
    virtual bool encode_proto_data(tcp_client *, const std::string &data);
    virtual bool decode_proto_data(tcp_client *, const std::string &data);
    virtual tcp_client* allocate_client() const;

    virtual void close_client(tcp_client *);
private:
    void init_tcp_server();
    void init_epoll();

    tcp_client *decode_remote_client(sockaddr *, int);
};
#endif //WEBSOCKET_DECODER_TCP_SERVER_H
