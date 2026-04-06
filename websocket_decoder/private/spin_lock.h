//
// Created by admin on 2026/3/14.
//

#ifndef WEBSOCKET_DECODER_SPIN_LOCK_H
#define WEBSOCKET_DECODER_SPIN_LOCK_H

#include <atomic>
#include <thread>

class spin_lock
{
public:
    spin_lock() = default;
    spin_lock(const spin_lock &) = delete;
    spin_lock &operator=(const spin_lock &) = delete;

    void lock() {
        while (lock_.test_and_set(std::memory_order_acquire))
            std::this_thread::yield();
    }

    void unlock() {
        lock_.clear(std::memory_order_release);
    }

    bool try_lock() {
        return !lock_.test_and_set(std::memory_order_acquire);
    }

private:
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};
#endif //WEBSOCKET_DECODER_SPIN_LOCK_H