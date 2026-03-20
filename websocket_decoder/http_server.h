//
// Created by admin on 2026/3/16.
//

#ifndef WEBSOCKET_DECODER_HTTP_SERVER_H
#define WEBSOCKET_DECODER_HTTP_SERVER_H
#include "tcp_server.h"

class http_client : public tcp_client {
    friend class Http_server;
protected:
    using tcp_client::tcp_client;
    ~http_client() noexcept override = default;
    std::unordered_map<std::string, std::string> http_header_;

};
class Http_server : public Tcp_server {
public:
    using Tcp_server::Tcp_server;
    ~Http_server() noexcept override = default;

protected:
    bool decode_proto_data(tcp_client *, const std::string &data) override;

private:
    static bool decode_http_header(tcp_client *, const std::string &data);
};


#endif //WEBSOCKET_DECODER_HTTP_SERVER_H