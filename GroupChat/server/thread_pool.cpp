#include "thread_pool.h"

ThreadPool::ThreadPool(std::size_t workers, ScheduleMode mode)
    : mode_(mode) {
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::submit(std::function<void()> work, std::size_t cost) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Task task{std::move(work), cost, next_sequence_++};

        if (mode_ == ScheduleMode::ShortestJobFirst) {
            sjf_queue_.push(std::move(task));
        } else {
            rr_queue_.push(std::move(task));
        }
    }
    cv_.notify_one();
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }

    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool ThreadPool::has_work() const {
    return mode_ == ScheduleMode::ShortestJobFirst ? !sjf_queue_.empty() : !rr_queue_.empty();
}

Task ThreadPool::pop_task() {
    if (mode_ == ScheduleMode::ShortestJobFirst) {
        Task task = std::move(const_cast<Task&>(sjf_queue_.top()));
        sjf_queue_.pop();
        return task;
    }

    Task task = std::move(rr_queue_.front());
    rr_queue_.pop();
    return task;
}

void ThreadPool::worker_loop() {
    while (true) {
        Task task;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || has_work(); });

            if (stopping_ && !has_work()) {
                return;
            }

            task = pop_task();
        }

        if (task.work) {
            task.work();
        }
    }
}
