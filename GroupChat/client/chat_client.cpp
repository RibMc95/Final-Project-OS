#include "chat_client.h"

#include "audio_client.h"
#include "protocol.h"
#include "utils.h"

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

ChatClient::ChatClient(std::string host, int port)
    : host_(std::move(host)), port_(port), socket_fd_(-1), running_(false) {
}

ChatClient::~ChatClient() {
    running_ = false;

    if (socket_fd_ >= 0) {
        utils::close_socket(socket_fd_);
        socket_fd_ = -1;
    }

    if (receiver_.joinable()) {
        receiver_.join();
    }

    if (incoming_audio_.is_open()) {
        incoming_audio_.close();
    }
}

bool ChatClient::connect_to_server() {
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "ERROR: Could not create socket.\n";
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (::inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "ERROR: Invalid server address.\n";
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "ERROR: Could not connect to server.\n";
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this]() {
        receive_loop();
    });

    return true;
}

void ChatClient::run() {
    std::cout << "Connected to server.\n";
    std::cout << "Commands:\n";
    std::cout << "  /join <group>\n";
    std::cout << "  /list\n";
    std::cout << "  /leave\n";
    std::cout << "  /audio <file_path>\n";
    std::cout << "  /quit\n\n";

    std::string line;

    while (running_ && std::getline(std::cin, line)) {
        line = utils::trim(line);

        if (line.empty()) {
            continue;
        }

        if (line.rfind("/audio ", 0) == 0) {
            std::string path = utils::trim(line.substr(7));
            if (path.empty()) {
                std::cout << "Usage: /audio <file_path>\n";
                continue;
            }

            if (!send_audio_file(socket_fd_, path)) {
                std::cout << "ERROR: Could not send audio file.\n";
            }

            continue;
        }

        if (!utils::send_text(socket_fd_, line)) {
            std::cout << "Disconnected from server.\n";
            running_ = false;
            break;
        }

        if (line == "/quit") {
            running_ = false;
            break;
        }
    }
}

void ChatClient::receive_loop() {
    uint8_t type = 0;
    std::vector<char> payload;

    while (running_ && utils::recv_frame(socket_fd_, type, payload)) {
        if (type == protocol::FRAME_TEXT || type == protocol::FRAME_SERVER_TEXT) {
            handle_server_text(utils::bytes_to_string(payload));
        } else if (type == protocol::FRAME_AUDIO_BEGIN) {
            handle_audio_begin(utils::bytes_to_string(payload));
        } else if (type == protocol::FRAME_AUDIO_CHUNK) {
            handle_audio_chunk(payload);
        } else if (type == protocol::FRAME_AUDIO_END) {
            handle_audio_end();
        } else {
            std::cout << "Received unknown frame type.\n";
        }
    }

    running_ = false;
}

void ChatClient::handle_server_text(const std::string& text) {
    std::cout << text << std::endl;
}

void ChatClient::handle_audio_begin(const std::string& filename) {
    std::string safe_name = protocol::safe_filename(filename);
    incoming_audio_name_ = "downloads/received_" + safe_name;

    system("mkdir -p downloads");

    if (incoming_audio_.is_open()) {
        incoming_audio_.close();
    }

    incoming_audio_.open(incoming_audio_name_, std::ios::binary);

    if (!incoming_audio_) {
        std::cout << "ERROR: Could not open file for incoming audio: "
                  << incoming_audio_name_ << std::endl;
        return;
    }

    std::cout << "Receiving audio file: " << incoming_audio_name_ << std::endl;
}

void ChatClient::handle_audio_chunk(const std::vector<char>& chunk) {
    if (incoming_audio_.is_open()) {
        incoming_audio_.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    }
}

void ChatClient::handle_audio_end() {
    if (incoming_audio_.is_open()) {
        incoming_audio_.close();
        std::cout << "Audio saved to: " << incoming_audio_name_ << std::endl;
    } else {
        std::cout << "Audio transfer ended, but no file was open.\n";
    }
}