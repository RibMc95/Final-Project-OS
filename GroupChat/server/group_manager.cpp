#include "group_manager.h"
#include "../shared/utils.h"

#include <fstream>
#include <sstream>

GroupManager::GroupManager(std::string log_file)
    : log_file_(std::move(log_file)) {}

void GroupManager::register_client(int client_fd, int client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    client_ids_[client_fd] = client_id;
}

void GroupManager::remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto current = client_group_.find(client_fd);
    if (current != client_group_.end()) {
        auto group = groups_.find(current->second);
        if (group != groups_.end()) {
            group->second.members.erase(client_fd);
        }
        client_group_.erase(current);
    }

    client_ids_.erase(client_fd);
}

void GroupManager::join_group(int client_fd, const std::string& group_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (group_name.empty()) {
        utils::send_line(client_fd, "ERROR Group name cannot be empty.");
        return;
    }

    auto old = client_group_.find(client_fd);
    if (old != client_group_.end()) {
        groups_[old->second].members.erase(client_fd);
    }

    Group& group = groups_[group_name];
    group.members.insert(client_fd);
    client_group_[client_fd] = group_name;

    utils::send_line(client_fd, "INFO Joined group '" + group_name + "'.");
    send_history_unlocked(client_fd, group);
}

void GroupManager::leave_current_group(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto current = client_group_.find(client_fd);
    if (current == client_group_.end()) {
        utils::send_line(client_fd, "INFO You are not in a group.");
        return;
    }

    groups_[current->second].members.erase(client_fd);
    utils::send_line(client_fd, "INFO Left group '" + current->second + "'.");
    client_group_.erase(current);
}

void GroupManager::send_group_list(int client_fd) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream out;
    out << "GROUPS ";
    bool first = true;

    for (const auto& [name, group] : groups_) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << name << "(" << group.members.size() << ")";
    }

    if (first) {
        out << "<none>";
    }

    utils::send_line(client_fd, out.str());
}

void GroupManager::broadcast_message(int client_fd, const std::string& text) {
    std::vector<int> targets;
    std::string formatted;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto current = client_group_.find(client_fd);
        if (current == client_group_.end()) {
            utils::send_line(client_fd, "ERROR Join a group first: /join general");
            return;
        }

        Group& group = groups_[current->second];
        formatted = format_message(client_fd, text);
        group.cache.add(formatted);
        append_log(formatted);

        targets.assign(group.members.begin(), group.members.end());
    }

    // Send outside the lock so one slow client does not block group state.
    for (int fd : targets) {
        utils::send_line(fd, "MSG " + formatted);
    }
}

std::string GroupManager::format_message(int client_fd, const std::string& text) const {
    std::string group = "<none>";
    auto current = client_group_.find(client_fd);
    if (current != client_group_.end()) {
        group = current->second;
    }

    int id = client_fd;
    auto found_id = client_ids_.find(client_fd);
    if (found_id != client_ids_.end()) {
        id = found_id->second;
    }

    std::ostringstream out;
    out << "[" << utils::now_string() << "] "
        << "group=" << group << " "
        << "sender=" << id << " "
        << text;
    return out.str();
}

void GroupManager::append_log(const std::string& message) {
    std::ofstream log(log_file_, std::ios::app);
    if (log) {
        log << message << '\n';
    }
}

void GroupManager::send_history_unlocked(int client_fd, const Group& group) {
    auto history = group.cache.history();

    if (history.empty()) {
        utils::send_line(client_fd, "INFO No recent history for this group yet.");
        return;
    }

    utils::send_line(client_fd, "INFO Recent history:");
    for (const auto& message : history) {
        utils::send_line(client_fd, "HISTORY " + message);
    }
}
