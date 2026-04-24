#include "chat_server.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ChatServer::ChatServer(int port, ScheduleMode mode, std::size_t worker_count)
    : port_(port), groups_("logs/chat_log.txt"), pool_(worker_count, mode) {}

ChatServer::~ChatServer() {
    running_ = false;
    if (server_fd_ >= 0) {
        utils::close_socket(server_fd_);
    }

    for (auto& t : client_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

bool ChatServer::start() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Could not create socket.\n";
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Bind failed. Is the port already in use?\n";
        return false;
    }

    if (::listen(server_fd_, 16) < 0) {
        std::cerr << "Listen failed.\n";
        return false;
    }

    running_ = true;
    std::cout << "GroupChat server listening on port " << port_ << "\n";
    accept_loop();
    return true;
}

void ChatServer::accept_loop() {
    while (running_) {
        sockaddr_in client_address{};
        socklen_t length = sizeof(client_address);

        int client_fd = ::accept(server_fd_, reinterpret_cast<sockaddr*>(&client_address), &length);
        if (client_fd < 0) {
            if (running_) {
                std::cerr << "Accept failed.\n";
            }
            continue;
        }

        int client_id = next_client_id_++;
        groups_.register_client(client_fd, client_id);

        utils::send_line(client_fd, "INFO Welcome. Your sender ID is " + std::to_string(client_id) + ".");
        utils::send_line(client_fd, "INFO Commands: /join <group>, /list, /quit. Type text to chat.");

        client_threads_.emplace_back([this, client_fd, client_id] {
            client_read_loop(client_fd, client_id);
        });
    }
}

void ChatServer::client_read_loop(int client_fd, int client_id) {
    std::string line;

    while (utils::recv_line(client_fd, line)) {
        std::string copy = line;
        pool_.submit([this, client_fd, copy] {
            handle_line(client_fd, copy);
        }, copy.size());
    }

    groups_.remove_client(client_fd);
    utils::close_socket(client_fd);
    std::cout << "Client " << client_id << " disconnected.\n";
}

void ChatServer::handle_line(int client_fd, const std::string& line) {
    std::string clean = utils::trim(line);
    if (clean.empty()) {
        return;
    }

    if (clean.rfind("/join ", 0) == 0) {
        groups_.join_group(client_fd, utils::trim(clean.substr(6)));
    } else if (clean == "/list") {
        groups_.send_group_list(client_fd);
    } else if (clean == "/leave") {
        groups_.leave_current_group(client_fd);
    } else if (clean == "/quit") {
        utils::send_line(client_fd, "INFO Bye.");
        groups_.remove_client(client_fd);
        utils::close_socket(client_fd);
    } else {
        groups_.broadcast_message(client_fd, clean);
    }
}
