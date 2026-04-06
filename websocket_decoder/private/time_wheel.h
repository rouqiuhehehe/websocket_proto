//
// Created by admin on 2026/3/14.
//

#ifndef WEBSOCKET_DECODER_TIME_WHEEL_H
#define WEBSOCKET_DECODER_TIME_WHEEL_H


// #include "spinlock.h"
#include <list>
#include <functional>
#include <chrono>
#include <thread>
#include <iostream>
#include "spin_lock.h"

#ifdef __linux__
#include <pthread.h>
#elif defined(_WIN32)

#include <windows.h>

#endif

#define LEVEL 4
#define TIME_NEAR_SHIFT 8
#define TIME_NEAR (1 << TIME_NEAR_SHIFT)
#define TIME_LEVEL_SHIFT 6
#define TIME_LEVEL (1 << TIME_LEVEL_SHIFT)
#define TIME_NEAR_MASK (TIME_NEAR - 1)
#define TIME_LEVEL_MASK (TIME_LEVEL - 1)

#define OFFSET(N) (TIME_NEAR + (N) * TIME_LEVEL)
#define INDEX(V, N) ((V >> (TIME_NEAR_SHIFT + (N) * TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)

using namespace std::chrono_literals;

class time_wheel
{
    struct time_node;

public:
    using Function_type = std::function<void(bool, size_t)>;
    using Time_type = std::chrono::milliseconds;
    using Time_list = std::list<time_node>;
    static constexpr uint32_t FULL_CIRCLE = std::numeric_limits<uint32_t>::max();
    time_wheel() = delete;
    time_wheel(const time_wheel&) = delete;
    time_wheel& operator=(const time_wheel&) = delete;
    ~time_wheel() = default;

    // 一个刻度的时间，默认100ms
    static void init_time_wheel(Time_type scale = 100ms) {
        if (!init_) {
            spin_lock_.lock();
            if (!init_) {
                init_ = true;
                scale_ = scale;
                jthread_ = std::jthread(&time_wheel::run_time_wheel);
                // setThreadAffinity();
                current_ = get_now();
            }
            spin_lock_.unlock();
        }
    }

    static void stop_time_wheel() {
        if (spin_lock_.try_lock()) {
            jthread_.request_stop();
            jthread_.join();
            init_ = false;
            id_map_.clear();
            for (auto& v : tv_) {
                v.clear();
            }
            spin_lock_.unlock();
        }
    }

    template <class F>
    static size_t add_task(F&& fn, const Time_type expire, const uint32_t circle_count = FULL_CIRCLE,
                          const bool immediate = false, const Time_type delay = Time_type(0)) {
        time_node time_node;
        time_node.fn_ = std::forward<F>(fn);
        time_node.expire_ = get_now() + (immediate ? delay : (expire + delay));
        time_node.circle_count_ = circle_count;
        time_node.interval_ = expire;

        add_node(time_node);

        return time_node.id_;
    }

    static bool erase_task(const uint32_t id) {
        spin_lock_.lock();
        auto ret = id_map_.find(id);
        if (ret == id_map_.end()) {
            std::cerr << "error: cant find erase task id\n";
            return false;
        }
        ret->second->list_->erase(ret->second);
        id_map_.erase(ret);
        spin_lock_.unlock();
        return true;
    }

private:
    static void set_thread_affinity() {
#ifdef __linux__
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(0, &cpu_set);
        if (pthread_setaffinity_np(jthread_.native_handle(), sizeof(cpu_set_t), &cpu_set) != 0) {
            std::cerr << "set thread affinity failed";
        }
#elif defined(_WIN32)
        SetThreadAffinityMask(jthread_.native_handle(), 0x1);
#endif
    }

    static void run_time_wheel(const std::stop_token& stopToken) {
        current_ = get_now();
        while (!stopToken.stop_requested()) {
            check();
            if (scale_.count()) {
                std::this_thread::sleep_for(scale_);
            } else {
                std::this_thread::yield();
            }
        }
    }

    static void add_node(time_node& node) {
        size_t idx = (node.expire_ - current_).count();
        if (idx < TIME_NEAR) {
            idx = node.expire_.count() & TIME_NEAR_MASK;
        } else if (idx < (1 << (TIME_NEAR_SHIFT + TIME_LEVEL_SHIFT))) {
            idx = OFFSET(0) + INDEX(node.expire_.count(), 0);
        } else if (idx < (1 << (TIME_NEAR_SHIFT + 2 * TIME_LEVEL_SHIFT))) {
            idx = OFFSET(1) + INDEX(node.expire_.count(), 1);
        } else if (idx < (1 << (TIME_NEAR_SHIFT + 3 * TIME_LEVEL_SHIFT))) {
            idx = OFFSET(2) + INDEX(node.expire_.count(), 2);
        } else if (static_cast<long long>(idx) < 0) {
            // 用于处理定时器调度中因为时间误差导致的过期任务
            // 以及scale_精度导致的过期任务
            idx = current_.count() & TIME_NEAR_MASK;
        } else {
            if (idx > std::numeric_limits<uint32_t>::max()) {
                idx = std::numeric_limits<uint32_t>::max();
                node.expire_ = std::chrono::milliseconds(idx) + current_;
            }
            idx = OFFSET(3) + INDEX(node.expire_.count(), 3);
        }

        spin_lock_.lock();
        Time_list& time_list = tv_[idx];
        node.list_ = &time_list;
        time_list.emplace_back(node);
        id_map_[node.id_] = std::prev(time_list.end());
        spin_lock_.unlock();
    }

    static void check() {
        // spin_lock_.lock();
        auto now = get_now();
        while (current_ <= now) {
            size_t idx = current_.count() & TIME_NEAR_MASK;

            if (!idx && !cascade(OFFSET(0), INDEX(current_.count(), 0)) &&
                !cascade(OFFSET(1), INDEX(current_.count(), 1)) &&
                !cascade(OFFSET(2), INDEX(current_.count(), 2)))
                cascade(OFFSET(3), INDEX(current_.count(), 3));

            ++current_;

            spin_lock_.lock();
            auto temp = std::move(tv_[idx]);
            spin_lock_.unlock();
            bool is_done;
            for (auto& v : temp) {
                // 如果还有需要执行的次数，重新计算过期时间并添加进时间轮调度
                if (v.circle_count_ == FULL_CIRCLE || --v.circle_count_) {
                    v.expire_ = now + v.interval_;
                    is_done = false;
                    add_node(v);
                } else {
                    is_done = true;
                    id_map_.erase(v.id_);
                }
                v.fn_(is_done, v.id_);
            }
        }
    }

    static bool cascade(int offset, int idx) {
        Time_list& list = tv_[offset + idx];
        Time_list temp = std::move(list);

        for (auto& v : temp) {
            add_node(v);
        }

        return idx;
    }

    static Time_type get_now() {
        return std::chrono::duration_cast<Time_type>(std::chrono::system_clock::now().time_since_epoch());
    }

    struct time_node
    {
        Function_type fn_;
        Time_type expire_;
        Time_type interval_;
        uint32_t circle_count_;
        Time_list* list_;
        const size_t id_ = get_id();

    private:
        static size_t get_id() {
            static size_t id = 1;
            if (id == 0) {
                id = 1;
            }
            return id++;
        };
    };

    static inline std::unordered_map<size_t, Time_list::iterator> id_map_;
    static inline Time_type current_;
    static inline Time_type scale_;
    static inline bool init_ = false;
    static inline std::jthread jthread_;
    static inline spin_lock spin_lock_;
    static inline Time_list tv_[TIME_NEAR + LEVEL * TIME_LEVEL];
};
#endif //WEBSOCKET_DECODER_TIME_WHEEL_H