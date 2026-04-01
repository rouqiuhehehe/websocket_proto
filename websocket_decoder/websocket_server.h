//
// Created by admin on 2026/3/12.
//

#ifndef WEBSOCKET_DECODER_WEBSOCKET_SERVER_H
#define WEBSOCKET_DECODER_WEBSOCKET_SERVER_H
#include "http_server.h"
#include "tcp_server.h"

/**
 *
*   0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |      (16/64 bits)              |
 |N|V|V|V|       |S|             |   (if payload len==126/127)    |
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |     Masking-key (32 bits)     |   Payload Data                |
 +-------------------------------+-------------------------------+
 |                     Payload Data (continued)                  |
 +---------------------------------------------------------------+
 */

#define PAYLOAD_2_FLAG 0x7e
#define PAYLOAD_8_FLAG 0x7f

enum class opcode_flag : uint8_t {
    CONTINUATION_FRAME = 0,
    TEXT_FRAME = 0x1,
    BINARY_FRAME = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
};
enum class close_flag : uint16_t {
    SUCCESS = 0,
    NORMAL_CLOSE = 1000,
    GOING_AWAY = 1001,
    PROTOCOL_ERROR = 1002,
    UNSUPPORTED_DATA = 1003,
    INTERNAL_UTF8 = 1007,
    SERVER_ERROR = 1011,
};

struct opcode_bit {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    opcode_flag opcode: 4;
    bool rsv3: 1;
    bool rsv2: 1;
    bool rsv1: 1;
    bool fin: 1;
#else
    bool fin: 1;
    bool rsv1: 1;
    bool rsv2: 1;
    bool rsv3: 1;

    opcode_flag opcode: 4;
#endif
};

struct payload_mask {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t payload_len : 7;
    bool mask : 1;
#else
    bool mask : 1;
    uint8_t payload_len : 7;
#endif
};

// __attribute__((packed)) 禁用对其
struct __attribute__((packed)) websocket_header {
    opcode_bit opcode;
    payload_mask payload;
};

class websocket_client final : public http_client {
    friend class Websocket_server;

    bool is_streaming_ = false;
    bool is_connected_ = false;
    bool wait_close_ = false;
    size_t time_wheel_id_ {};
    size_t time_wheel_heartbeat_id_ {};
    std::string decode_buffer_;
    websocket_header wb_header_;
public:
    using http_client::http_client;

    ~websocket_client() noexcept override;

    [[nodiscard]] std::string get_message() const override {
        return decode_buffer_;
    }

    [[nodiscard]] std::string_view get_path() const {
        auto it = http_header_.find("path");
        if (it != http_header_.end()) {
            return it->second;
        }
        return {};
    }

    void send(const std::string &send_buffer) override;
    void send_without_frame(const std::string &send_buffer);
};
class Websocket_server final : public Http_server {
public:
    explicit Websocket_server(const std::string &server_addr = "0.0.0.0", int port_ = 8192, bool start_heartbeat = true);
    ~Websocket_server() override;
    void register_recv_callback(Msg_callback_fn &&fn) override;
    void register_connected_callback(Msg_callback_fn &&fn) override;

private:
    bool decode_proto_data(tcp_client *, const std::string &data) override;
    tcp_client *allocate_client() const override;

    void close_websocket_client(tcp_client *, close_flag) const;

    bool websocket_shake_hands(websocket_client *, const std::string &data) const;

    static std::string websocket_accept_key(const std::string &wb_key);
    static std::string generate_websocket_shake_hands_res(const std::string &accept_key);

    void add_to_time_wheel_expire(tcp_client *, close_flag);

    static void add_to_time_wheel_heartbeat(tcp_client *);

    static void remove_from_time_wheel(tcp_client *);

    static std::string generate_websocket_pong_res(tcp_client *);
    static std::string generate_websocket_ping_res(tcp_client *);
    static close_flag decode_websocket_data_frame(tcp_client *);

    static bool validate_utf8(std::string_view);

    static constexpr auto default_expire_time = 10s;
    static constexpr auto default_heartbeat_time = 3s;

    // 是否开启心跳检测
    bool heartbeat_;
    Msg_callback_fn wb_connected_f_;
};


#endif //WEBSOCKET_DECODER_WEBSOCKET_SERVER_H
