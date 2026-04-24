#pragma once

#include <deque>
#include <string>
#include <vector>

class MessageCache {
public:
    explicit MessageCache(std::size_t capacity = 20)
        : capacity_(capacity) {}

    void add(const std::string& message) {
        if (messages_.size() >= capacity_) {
            messages_.pop_front(); // circular-buffer style eviction
        }
        messages_.push_back(message);
    }

    std::vector<std::string> history() const {
        return std::vector<std::string>(messages_.begin(), messages_.end());
    }

    std::size_t size() const {
        return messages_.size();
    }

private:
    std::size_t capacity_;
    std::deque<std::string> messages_;
};
