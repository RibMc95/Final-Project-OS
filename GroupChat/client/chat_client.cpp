#include "chat_client.h"
#include "audio_client.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"

#include <arpa/inet.h>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

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
        std::cerr << "Invalid address. Use something like 127.0.0.1\n";
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
    std::cout << "Commands: /join <group>, /list, /leave, /audio <file>, /quit\n";
    std::cout << "Example: /audio samples/hello.wav\n";

    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        if (line.rfind("/audio ", 0) == 0) {
            std::string path = utils::trim(line.substr(7));
            if (path.empty()) {
                std::cout << "Usage: /audio <path-to-audio-file>\n";
                continue;
            }
            send_audio_file(socket_fd_, path);
            continue;
        }

        if (!utils::send_text(socket_fd_, line)) {
            break;
        }

        if (line == "/quit") {
            break;
        }
    }

    running_ = false;
}

void ChatClient::receive_loop() {
    uint8_t type = 0;
    std::vector<char> payload;

    while (running_ && utils::recv_frame(socket_fd_, type, payload)) {
        if (type == protocol::FRAME_SERVER_TEXT) {
            handle_server_text(utils::bytes_to_string(payload));
        } else if (type == protocol::FRAME_AUDIO_BEGIN) {
            handle_audio_begin(utils::bytes_to_string(payload));
        } else if (type == protocol::FRAME_AUDIO_CHUNK) {
            handle_audio_chunk(payload);
        } else if (type == protocol::FRAME_AUDIO_END) {
            handle_audio_end();
        }
    }

    running_ = false;
}

void ChatClient::handle_server_text(const std::string& text) {
    std::cout << text << std::endl;
}

void ChatClient::handle_audio_begin(const std::string& filename) {
    std::filesystem::create_directories("downloads");

    incoming_audio_name_ = "downloads/received_" + protocol::safe_filename(filename);
    incoming_audio_.close();
    incoming_audio_.open(incoming_audio_name_, std::ios::binary);

    if (!incoming_audio_) {
        std::cerr << "Could not save incoming audio to " << incoming_audio_name_ << "\n";
        incoming_audio_name_.clear();
        return;
    }

    std::cout << "Receiving audio: " << incoming_audio_name_ << "\n";
}

void ChatClient::handle_audio_chunk(const std::vector<char>& chunk) {
    if (incoming_audio_) {
        incoming_audio_.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    }
}

void ChatClient::handle_audio_end() {
    if (incoming_audio_) {
        incoming_audio_.close();
        std::cout << "Audio saved as " << incoming_audio_name_ << "\n";
    }
    incoming_audio_name_.clear();
}
