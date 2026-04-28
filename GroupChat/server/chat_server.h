#pragma once

#include "group_manager.h"
#include "thread_pool.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

class ChatServer {
public:
    ChatServer(int port, ScheduleMode mode, std::size_t worker_count = 4);
    ~ChatServer();

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;

    bool start();

private:
    void accept_loop();
    void client_read_loop(int client_fd, int client_id);
    void handle_text_line(int client_fd, const std::string& line);
    void relay_audio_begin(int client_fd, const std::string& filename);
    void relay_audio_chunk(int client_fd, const std::vector<char>& chunk);
    void relay_audio_end(int client_fd, const std::string& filename);
    void relay_video_begin(int client_fd, const std::string& filename);
    void relay_video_chunk(int client_fd, const std::vector<char>& chunk);
    void relay_video_end(int client_fd, const std::string& filename);

    int port_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::atomic<int> next_client_id_{1};

    GroupManager groups_;
    ThreadPool pool_;
    std::vector<std::thread> client_threads_;
};
