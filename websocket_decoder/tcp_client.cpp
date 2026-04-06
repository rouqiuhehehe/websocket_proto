//
// Created by admin on 2026/3/12.
//
#include "tcp_client.h"
#include "tcp_server.h"
#include <fmt/format.h>
#include <openssl/rand.h>

tcp_client::tcp_client() {
    uint8_t buf[16];
    RAND_bytes(buf, sizeof(buf));

    id_ = fmt::format(
        "{:02x}{:02x}{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}-"
        "{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        buf[0], buf[1], buf[2], buf[3],
        buf[4], buf[5],
        buf[6], buf[7],
        buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]
    );
}

void tcp_client::send(const std::string &send_buffer) {
    tcp_server_->send_buffer(this, send_buffer);
}

void tcp_client::close() {
    tcp_server_->close_client(this);
}
