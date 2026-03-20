//
// Created by admin on 2026/3/12.
//

#ifndef WEBSOCKET_DECODER_TCP_CLIENT_H
#define WEBSOCKET_DECODER_TCP_CLIENT_H
#include <string>
#include <functional>

class tcp_client;
using Msg_callback_fn = std::function<void(tcp_client *)>;

class Tcp_server;
class tcp_client {
protected:
    friend class Tcp_server;
    Tcp_server *tcp_server_{};
    std::string send_buffer_;
    std::string recv_buffer_;
public:
    std::string ip_;
    int port_{};
    int sockfd_{};

    tcp_client() = default;
    tcp_client(Tcp_server *,const std::string &ip, int port, int sockfd);

    virtual ~tcp_client() noexcept = default;

    [[nodiscard]] std::string get_ip() const {
        return ip_;
    }

    [[nodiscard]] int get_port() const {
        return port_;
    }

    [[nodiscard]] virtual std::string get_message() const {
        return recv_buffer_;
    }

    virtual void send(const std::string &send_buffer);
};

#endif //WEBSOCKET_DECODER_TCP_CLIENT_H
