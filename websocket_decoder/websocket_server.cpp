//
// Created by admin on 2026/3/12.
//

#include "websocket_server.h"

#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#define PROTO_ERR(flag) \
    std::cerr << __FILE__ << ":" << __LINE__ << " websocket proto error" << std::endl;  \
    close_websocket_client(wb_client, flag);    \
    return false

#define WB_CLIENT(tcp_client) auto wb_client = reinterpret_cast<websocket_client *>(tcp_client);

Websocket_server::Websocket_server(const std::string &server_addr, int port_, bool start_heartbeat)
    : Tcp_server(server_addr, port_), heartbeat_(start_heartbeat) {
    time_wheel::init_time_wheel();
}

bool Websocket_server::decode_proto_data(tcp_client *tcp_client, const std::string &data) {
    Tcp_server::decode_proto_data(tcp_client, data);

    WB_CLIENT(tcp_client);
    if (!wb_client->is_connected_) {
        // 握手信息
        if (websocket_shake_hands(wb_client, data) && heartbeat_) {
            // 握手成功，且开启心跳检测
            add_to_time_wheel_heartbeat(wb_client);
        }
        return false;
    }
    const char *origin_data = wb_client->recv_buffer_.data();

    if (wb_client->recv_buffer_.size() < sizeof(websocket_header)) {
        wb_client->wait_close_ = true;
        PROTO_ERR(close_flag::PROTOCOL_ERROR);
    }
    auto header = const_cast<websocket_header *>(reinterpret_cast<const websocket_header *>(origin_data));
    if (header->opcode.opcode == opcode_flag::CLOSE) {
        if (!wb_client->wait_close_) {
            // 已经添加到时间轮中等待检测关闭的socket，删除时间轮中的任务
            if (wb_client->time_wheel_id_) {
                remove_from_time_wheel(wb_client);
            }
            // server 被动关闭方
            close_websocket_client(wb_client, close_flag::NORMAL_CLOSE);
            close_client(wb_client);
        } else {
            // 主动关闭方，添加到时间轮，10秒未收到回复自动关闭
            add_to_time_wheel_expire(wb_client, close_flag::NORMAL_CLOSE);
        }
        return false;
    }
    if (header->opcode.opcode == opcode_flag::PONG) {
        // 心跳回复
        if (wb_client->time_wheel_id_) {
            remove_from_time_wheel(wb_client);
        }
        return false;
    }
    if (header->opcode.opcode == opcode_flag::PING) {
        // 心跳检测回复心跳
        wb_client->send_without_frame(generate_websocket_pong_res(wb_client));
        return false;
    }

    const char *p = origin_data + sizeof(websocket_header);

    uint64_t payload_len = 0;
    if (header->payload.payload_len < PAYLOAD_2_FLAG) {
        payload_len = header->payload.payload_len;
    } else if (header->payload.payload_len == PAYLOAD_2_FLAG) {
        if (wb_client->recv_buffer_.size() < sizeof(websocket_header) + 2) {
            PROTO_ERR(close_flag::PROTOCOL_ERROR);
        }
        payload_len = *p << 8 | *(p + 1);
        p += 2;
    } else if (header->payload.payload_len == PAYLOAD_8_FLAG) {
        if (wb_client->recv_buffer_.size() < sizeof(websocket_header) + 6) {
            PROTO_ERR(close_flag::PROTOCOL_ERROR);
        }
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | *(p + i);
        }
        p += 8;
    } else {
        PROTO_ERR(close_flag::PROTOCOL_ERROR);
    }

    if (payload_len + sizeof(websocket_header) > wb_client->recv_buffer_.size()) {
        PROTO_ERR(close_flag::PROTOCOL_ERROR);
    }
    if (header->payload.mask) {
        char mask[4] {};
        for (int i = 0; i < 4; ++i) {
            mask[i] = *(p + i);
        }
        p += 4;
        uint32_t m = *reinterpret_cast<uint32_t *>(mask);
        auto *pt = const_cast<uint32_t *>(reinterpret_cast<const uint32_t *>(p));
        for (int i = 0; i <= payload_len / 4; ++i) {
            pt[i] ^= m;
        }
    }

    wb_client->decode_buffer_.append(p, payload_len);
    if (!header->opcode.fin) {
        wb_client->is_fin_ = false;
        // 此处数据没有接受完，防止外部callback调用
        return false;
    }

    wb_client->is_fin_ = true;
    return true;
}

tcp_client *Websocket_server::allocate_client() const {
    return new websocket_client;
}

void Websocket_server::close_websocket_client(tcp_client *client, close_flag flag) const {
    WB_CLIENT(client);
    websocket_header header {};
    header.opcode.fin = true;
    header.opcode.opcode = opcode_flag::CLOSE;

    header.payload.mask = false;
    header.payload.payload_len = 2;
    std::string send_buffer {};
    send_buffer += reinterpret_cast<char *>(&header);
    send_buffer += htobe16(static_cast<int16_t>(flag));

    wb_client->send_without_frame(send_buffer);

    if (heartbeat_ && wb_client->time_wheel_heartbeat_id_) {
        time_wheel::erase_task(wb_client->time_wheel_heartbeat_id_);
        wb_client->time_wheel_heartbeat_id_ = 0;
    }
}

void websocket_client::send(const std::string &send_buffer) {
    websocket_header header {};
    // 先不处理分包
    header.opcode.fin = true;
    header.opcode.opcode = opcode_flag::TEXT_FRAME;
    header.payload.mask = false;

    auto size = send_buffer.size();
    char buffer[10] {};
    char *p = buffer + sizeof(header);
    if (size < PAYLOAD_2_FLAG) {
        header.payload.payload_len = size;
    } else if (size <= std::numeric_limits<uint16_t>::max()) {
        header.payload.payload_len = PAYLOAD_2_FLAG;
        *reinterpret_cast<uint16_t *>(p) = htobe16(size);
        p += sizeof(uint16_t);
    } else {
        header.payload.payload_len = PAYLOAD_8_FLAG;
        *reinterpret_cast<uint64_t *>(p) = htobe64(size);
        p += sizeof(uint64_t);
    }

    *reinterpret_cast<websocket_header *>(buffer) = header;
    std::string send_buffer_(buffer, p - buffer);
    send_buffer_ += send_buffer;

    tcp_server_->send_buffer(this, send_buffer_);
}

void websocket_client::send_without_frame(const std::string &send_buffer) {
    tcp_client::send(send_buffer);
}

void Websocket_server::register_recv_callback(Msg_callback_fn &&fn) {
    Tcp_server::register_recv_callback([&, fn](tcp_client *tcp_client) {
        WB_CLIENT(tcp_client);

        if (wb_client->is_connected_) {
            fn(wb_client);
            wb_client->is_fin_ = false;
            wb_client->decode_buffer_.clear();
        }
    });
}

bool Websocket_server::websocket_shake_hands(websocket_client *wb_client, const std::string &data) {
    const char *p = data.data();
    const char *m = p;
    const char *n = p;
    // GET /chat HTTP/1.1
    int i = 0;
    while (*m != '\r' && *(m + 1) != '\n') {
        if (*m == ' ') {
            switch (i) {
                case 0:
                    wb_client->header_.emplace("method", std::string(n, m - n));
                    break;
                case 1:
                    wb_client->header_.emplace("path", std::string(n, m - n));
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
        m += 2;
        n = m;
        while (*m != '\r' && *(m + 1) != '\n') ++m;
        auto value = std::string_view(n, m - n);

        wb_client->header_.emplace(key, value);
        m += 2;
        n = m;
        if (*m == '\r' && *(m + 1) == '\n') break;
    }

    auto upgrade = wb_client->header_.find("Upgrade");
    auto connection = wb_client->header_.find("Connection");
    if (upgrade != wb_client->header_.end()
        && upgrade->second == "websocket"
        && connection != wb_client->header_.end()
        && connection->second == "Upgrade") {
        // 协议正确
        auto wb_key = wb_client->header_.find("Sec-WebSocket-Key");
        if (wb_key != wb_client->header_.end()) {
            auto res = generate_websocket_shake_hands_res(websocket_accept_key(wb_key->second));
            wb_client->send_without_frame(res);
            wb_client->is_connected_ = true;
            return true;
        }
    }

    std::cerr << "websocket proto error\n" << data << std::endl;
    close(wb_client->sockfd_);
    return false;
}

std::string Websocket_server::websocket_accept_key(const std::string &wb_key) {
    constexpr char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    auto data = wb_key + guid;

    uint8_t hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(data.c_str()), data.size(), hash);
    char base64[32];
    EVP_EncodeBlock(reinterpret_cast<uint8_t *>(base64), hash, SHA_DIGEST_LENGTH);

    return base64;
}

std::string Websocket_server::generate_websocket_shake_hands_res(const std::string &accept_key) {
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + accept_key + "\r\n"
           "\r\n";
}

void Websocket_server::add_to_time_wheel_expire(tcp_client *client, close_flag flag) {
    WB_CLIENT(client);
    wb_client->time_wheel_id_ = time_wheel::add_task([&, wb_client](bool is_done, size_t id) {
        close_websocket_client(wb_client, flag);
        close_client(wb_client);
    }, default_expire_time, 1);
}

void Websocket_server::add_to_time_wheel_heartbeat(tcp_client *client) {
    WB_CLIENT(client);
    wb_client->time_wheel_heartbeat_id_ = time_wheel::add_task([&, wb_client](bool is_done, size_t id) {
        wb_client->send_without_frame(generate_websocket_ping_res(wb_client));
    }, default_heartbeat_time);
}

void Websocket_server::remove_from_time_wheel(tcp_client *client) {
    WB_CLIENT(client);
    time_wheel::erase_task(wb_client->time_wheel_id_);
    wb_client->time_wheel_id_ = 0;
}

std::string Websocket_server::generate_websocket_pong_res(tcp_client *client) {
    WB_CLIENT(client);
    websocket_header header {};
    header.opcode.fin = true;
    header.opcode.opcode = opcode_flag::PONG;

    return { reinterpret_cast<char *>(&header), sizeof(header) };
}

std::string Websocket_server::generate_websocket_ping_res(tcp_client *client) {
    WB_CLIENT(client);
    websocket_header header {};
    header.opcode.fin = true;
    header.opcode.opcode = opcode_flag::PING;

    return { reinterpret_cast<char *>(&header), sizeof(header) };
}
