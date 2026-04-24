#pragma once

#include "../shared/cache.h"

#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class GroupManager {
public:
    explicit GroupManager(std::string log_file = "logs/chat_log.txt");

    void register_client(int client_fd, int client_id);
    void remove_client(int client_fd);

    void join_group(int client_fd, const std::string& group);
    void leave_current_group(int client_fd);
    void send_group_list(int client_fd);
    void broadcast_message(int client_fd, const std::string& message);

private:
    struct Group {
        std::set<int> members;
        MessageCache cache{20};
    };

    std::string format_message(int client_fd, const std::string& text) const;
    void append_log(const std::string& message);
    void send_history_unlocked(int client_fd, const Group& group);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Group> groups_;
    std::unordered_map<int, std::string> client_group_;
    std::unordered_map<int, int> client_ids_;
    std::string log_file_;
};
