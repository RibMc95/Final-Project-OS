#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

enum class ScheduleMode {
    RoundRobin,
    ShortestJobFirst
};

struct Task {
    std::function<void()> work;
    std::size_t cost{0}; // SJF uses smaller message length first
    std::uint64_t sequence{0};
};

class ThreadPool {
public:
    ThreadPool(std::size_t workers, ScheduleMode mode);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> work, std::size_t cost);
    void stop();

private:
    struct SJFCompare {
        bool operator()(const Task& a, const Task& b) const {
            if (a.cost == b.cost) {
                return a.sequence > b.sequence;
            }
            return a.cost > b.cost;
        }
    };

    void worker_loop();
    bool has_work() const;
    Task pop_task();

    ScheduleMode mode_;
    bool stopping_{false};
    std::uint64_t next_sequence_{0};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;

    std::queue<Task> rr_queue_;
    std::priority_queue<Task, std::vector<Task>, SJFCompare> sjf_queue_;
};
