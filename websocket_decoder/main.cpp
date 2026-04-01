#include <future>
#include <iostream>
#include <shared_mutex>
#include <unordered_set>

#include "tcp_server.h"
#include "websocket_server.h"
#include <fmt/format.h>

int main() {
    Websocket_server wb_server { "0.0.0.0", 8192, false };

    std::unordered_set<websocket_client *> wb_clients {};
    std::mutex mutex;
    std::thread wb_ping_thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            mutex.lock();
            for (auto wb_client: wb_clients) {
                wb_client->send(fmt::format("hello {}", wb_client->get_ip()));
            }
            mutex.unlock();
        }
    });

    wb_server.register_connected_callback([&](tcp_client *tcp_client) {
        auto wb_client = reinterpret_cast<websocket_client *>(tcp_client);
        std::cout << fmt::format("[{}:{}] connect success", wb_client->get_ip(), wb_client->get_port()) << std::endl;

        std::lock_guard lock(mutex);
        wb_clients.emplace(wb_client);
    });

    wb_server.register_recv_callback([&](tcp_client *tcp_client) {
        auto wb_client = reinterpret_cast<websocket_client *>(tcp_client);
        std::cout << fmt::format("[{}:{}] recv message : {}", wb_client->get_ip(), wb_client->get_port(),
                                 wb_client->get_message()) << std::endl;

        wb_client->send(wb_client->get_message());
    });

    wb_server.register_disconnect_callback([&](tcp_client *tcp_client) {
        auto wb_client = reinterpret_cast<websocket_client *>(tcp_client);

        std::cout << fmt::format("[{}:{}] disconnect", wb_client->get_ip(), wb_client->get_port()) << std::endl;
        std::lock_guard lock(mutex);
        wb_clients.erase(wb_client);
    });

    wb_server.start();

    return 0;
}
