//
// Created by admin on 2026/3/12.
//
#include "tcp_client.h"
#include "tcp_server.h"

tcp_client::tcp_client(Tcp_server *server, const std::string &ip, int port, int sockfd)
    : tcp_server_(server),
      ip_(ip), port_(port),
      sockfd_(sockfd) {}


void tcp_client::send(const std::string &send_buffer) {
    tcp_server_->send_buffer(this, send_buffer);
}