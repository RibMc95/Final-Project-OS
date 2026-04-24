#include "chat_client.h"
#include "../shared/utils.h"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ChatClient::ChatClient(std::string host, int port)
    : host_(std::move(host)), port_(port) {}

ChatClient::~ChatClient() {
    running_ = false;
    utils::close_socket(socket_fd_);

    if (receiver_.joinable()) {
        receiver_.join();
    }
}

bool ChatClient::connect_to_server() {
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "Could not create socket.\n";
        return false;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::inet_pton(AF_INET, host_.c_str(), &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address.\n";
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) < 0) {
        std::cerr << "Could not connect to server.\n";
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this] { receive_loop(); });
    return true;
}

void ChatClient::run() {
    std::cout << "Type /join general to enter a group.\n";
    std::cout << "Commands: /join <group>, /list, /leave, /quit\n";

    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        if (!utils::send_line(socket_fd_, line)) {
            break;
        }

        if (line == "/quit") {
            break;
        }
    }

    running_ = false;
}

void ChatClient::receive_loop() {
    std::string line;

    while (running_ && utils::recv_line(socket_fd_, line)) {
        std::cout << line << std::endl;
    }

    running_ = false;
}
