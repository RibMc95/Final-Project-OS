#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

class ChatClient {
public:
    ChatClient(std::string host, int port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    void receive_loop();
    void handle_server_text(const std::string& text);
    void handle_audio_begin(const std::string& filename);
    void handle_audio_chunk(const std::vector<char>& chunk);
    void handle_audio_end();

    std::string host_;
    int port_;
    int socket_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread receiver_;

    std::ofstream incoming_audio_;
    std::string incoming_audio_name_;
};
