#include "group_manager.h"
#include "../shared/utils.h"
#include <fstream>
#include <sstream>
using namespace std;

GroupManager::GroupManager(std::string log_file) : log_file_(std::move(log_file)) {}

void GroupManager::register_client(int client_fd, int client_id)
{
    lock_guard<mutex> lock(mutex_);
    client_ids_[client_fd] = client_id;
}

void GroupManager::remove_client(int client_fd)
{
    lock_guard<mutex> lock(mutex_);

    auto current = client_group_.find(client_fd);
    if (current != client_group_.end())
    {
        auto group = groups_.find(current->second);
        if (group != groups_.end())
        {
            group->second.members.erase(client_fd);
        }
        client_group_.erase(current);
    }

    client_ids_.erase(client_fd);
}

void GroupManager::join_group(int client_fd, const std::string &group_name)
{
    lock_guard<mutex> lock(mutex_);

    if (group_name.empty())
    {
        utils::send_line(client_fd, "ERROR Group name cannot be empty.");
        return;
    }

    auto old = client_group_.find(client_fd);
    if (old != client_group_.end())
    {
        groups_[old->second].members.erase(client_fd);
    }

    Group &group = groups_[group_name];
    group.members.insert(client_fd);
    client_group_[client_fd] = group_name;
    utils::send_line(client_fd, "INFO Joined group '" + group_name + "'.");
    send_history_unlocked(client_fd, group);
}

void GroupManager::leave_current_group(int client_fd)
{
    lock_guard<mutex> lock(mutex_);

    auto current = client_group_.find(client_fd);
    if (current == client_group_.end())
    {
        utils::send_line(client_fd, "INFO You are not in a group.");
        return;
    }

    groups_[current->second].members.erase(client_fd);
    utils::send_line(client_fd, "INFO Left group '" + current->second + "'.");
    client_group_.erase(current);
}

void GroupManager::send_group_list(int client_fd)
{
    lock_guard<mutex> lock(mutex_);
    ostringstream out;
    out << "GROUPS ";
    bool first = true;

    for (const auto &[name, group] : groups_)
    {
        if (!first)
        {
            out << ", ";
        }
        first = false;
        out << name << "(" << group.members.size() << ")";
    }

    if (first)
    {
        out << "<none>";
    }

    utils::send_line(client_fd, out.str());
}

void GroupManager::broadcast_message(int client_fd, const std::string &text)
{
    std::vector<int> targets;
    std::string formatted;

    {
        lock_guard<mutex> lock(mutex_);

        auto current = client_group_.find(client_fd);
        if (current == client_group_.end())
        {
            utils::send_line(client_fd, "ERROR Join a group first: /join general");
            return;
        }

        Group &group = groups_[current->second];
        formatted = format_message(client_fd, text);
        group.cache.add(formatted);
        append_log(formatted);

        targets.assign(group.members.begin(), group.members.end());
    }

    // Send outside the lock so one slow client does not block group state.
    for (int fd : targets)
    {
        utils::send_line(fd, "MSG " + formatted);
    }
}

std::string GroupManager::format_message(int client_fd, const std::string &text) const
{
    std::string group = "<none>";
    auto current = client_group_.find(client_fd);
    if (current != client_group_.end())
    {
        group = current->second;
    }

    int id = client_fd;
    auto found_id = client_ids_.find(client_fd);
    if (found_id != client_ids_.end())
    {
        id = found_id->second;
    }

    ostringstream out;
    out << "[" << utils::now_string() << "] " << "group=" << group << " " << "sender=" << id << " " << text;
    return out.str();
}

void GroupManager::append_log(const std::string &message)
{
    std::ofstream log(log_file_, std::ios::app);
    if (log)
    {
        log << message << '\n';
    }
}

void GroupManager::send_history_unlocked(int client_fd, const Group &group)
{
    auto history = group.cache.history();

    if (history.empty())
    {
        utils::send_line(client_fd, "INFO No recent history for this group yet.");
        return;
    }

    utils::send_line(client_fd, "INFO Recent history:");
    for (const auto &message : history)
    {
        utils::send_line(client_fd, "HISTORY " + message);
    }
}

void GroupManager::broadcast_file(int client_fd, const std::string &filename, const std::vector<unsigned char> &data) // help user to send file
{
    std::vector<int> targets;
    int sender_id = 0;

    {
        lock_guard<mutex> lock(mutex_);
        auto cur = client_group_.find(client_fd);
        if (cur == client_group_.end())
        {
            utils::send_line(client_fd, "ERROR Join a group first: /join general");
            return;
        }

        auto id_it = client_ids_.find(client_fd);
        if (id_it != client_ids_.end())
        {
            sender_id = id_it->second;
        }

        const Group &group = groups_[cur->second];
        targets.assign(group.members.begin(), group.members.end());
    }

    // Header line: FILE <sender_id> <filename> <byte_count>
    std::string header = "FILE " + std::to_string(sender_id) + " " + filename + " " + std::to_string(data.size()) + "\n";

    for (int fd : targets)
    {
        if (fd == client_fd)
        {
            continue; // do not echo back to the sender
        }
        utils::send_all(fd, header);
        utils::send_raw_bytes(fd, data);
    }

    utils::send_line(client_fd, "INFO File '" + filename + "' (" + std::to_string(data.size()) + " bytes) sent to group.");
}
