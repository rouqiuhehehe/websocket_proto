//
// Created by admin on 2026/3/16.
//

#include "http_server.h"

#define h_client(tcp_client) auto h_client = reinterpret_cast<http_client *>(tcp_client);
bool Http_server::decode_proto_data(tcp_client *tcp_client, const std::string &data) {
    if (decode_http_header(tcp_client, data)) {
        // 先不做http协议支持，该项目主要调研websocket，http只是用于继承
        return Tcp_server::decode_proto_data(tcp_client, data);
    }
    return false;
}

bool Http_server::decode_http_header(tcp_client *client, const std::string &data) {
    h_client(client);
    
    const char *p = data.data();
    const char *m = p;
    const char *n = p;
    // GET /chat HTTP/1.1
    int i = 0;
    while (*m != '\r' && *(m + 1) != '\n') {
        if (*m == ' ') {
            switch (i) {
                case 0:
                    h_client->http_header_.emplace("method", std::string(n, m - n));
                    break;
                case 1:
                    h_client->http_header_.emplace("path", std::string(n, m - n));
                    break;
                default: ;
            }
            n = m + 1;
            i++;
        }
        ++m;
    }
    m += 2;
    n = m;
    /*
        Host: example.com:8000\r\n
        Upgrade: websocket\r\n
        Connection: Upgrade\r\n
        Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n
        Sec-WebSocket-Version: 13\r\n
        Origin: http://example.com\r\n
     */
    while (true) {
        while (*m != ':') ++m;
        auto key = std::string_view(n, m - n);
        if (key.empty()) {
            return false;
        }
        m += 2;
        n = m;
        while (*m != '\r' && *(m + 1) != '\n') ++m;
        auto value = std::string_view(n, m - n);

        if (value.empty()) {
            return false;
        }
        h_client->http_header_.emplace(key, value);
        m += 2;
        n = m;
        if (*m == '\r' && *(m + 1) == '\n') break;
    }

    return true;
}
