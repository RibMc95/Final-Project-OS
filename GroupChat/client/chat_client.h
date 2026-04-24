#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

class ChatClient
{
public:
    ChatClient(std::string host, int port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    void receive_loop();

    std::string host_;
    int port_;
    int socket_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread receiver_;

    std::ofstream incoming_audio_;
    std::string incoming_audio_name_;
};
