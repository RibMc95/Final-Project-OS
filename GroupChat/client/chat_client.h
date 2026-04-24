#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

class ChatClient {
public:
    ChatClient(std::string host, int port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    std::string host_;
    int port_;
    int socket_fd_;
    std::atomic<bool> running_;
    std::thread receiver_;

    std::ofstream incoming_audio_;
    std::string incoming_audio_name_;

    void receive_loop();
    void handle_server_text(const std::string& text);
    void handle_audio_begin(const std::string& filename);
    void handle_audio_chunk(const std::vector<char>& chunk);
    void handle_audio_end();
};