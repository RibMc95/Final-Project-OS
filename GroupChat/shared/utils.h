#pragma once

#include "protocol.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace utils {

inline std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

inline std::string now_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&t, &local);

    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

inline bool send_all(int fd, const void* data, std::size_t length) {
    const char* buffer = static_cast<const char*>(data);
    std::size_t total = 0;

    while (total < length) {
        ssize_t sent = ::send(fd, buffer + total, length - total, 0);
        if (sent <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(sent);
    }
    return true;
}

inline bool recv_all(int fd, void* data, std::size_t length) {
    char* buffer = static_cast<char*>(data);
    std::size_t total = 0;

    while (total < length) {
        ssize_t received = ::recv(fd, buffer + total, length - total, 0);
        if (received <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(received);
    }
    return true;
}

inline uint32_t host_to_network_u32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0xFF000000u) >> 24);
}

inline uint32_t network_to_host_u32(uint32_t value) {
    return host_to_network_u32(value);
}

inline bool send_frame(int fd, uint8_t type, const std::vector<char>& payload) {
    if (payload.size() > protocol::MAX_FRAME_SIZE) {
        return false;
    }

    uint32_t network_length = host_to_network_u32(static_cast<uint32_t>(payload.size()));

    if (!send_all(fd, &type, sizeof(type))) {
        return false;
    }
    if (!send_all(fd, &network_length, sizeof(network_length))) {
        return false;
    }
    if (!payload.empty() && !send_all(fd, payload.data(), payload.size())) {
        return false;
    }

    return true;
}

inline bool send_frame(int fd, uint8_t type, const std::string& payload) {
    std::vector<char> bytes(payload.begin(), payload.end());
    return send_frame(fd, type, bytes);
}

inline bool recv_frame(int fd, uint8_t& type, std::vector<char>& payload) {
    payload.clear();

    if (!recv_all(fd, &type, sizeof(type))) {
        return false;
    }

    uint32_t network_length = 0;
    if (!recv_all(fd, &network_length, sizeof(network_length))) {
        return false;
    }

    uint32_t length = network_to_host_u32(network_length);
    if (length > protocol::MAX_FRAME_SIZE) {
        std::cerr << "Frame too large. Closing connection.\n";
        return false;
    }

    payload.resize(length);
    if (length > 0 && !recv_all(fd, payload.data(), payload.size())) {
        return false;
    }

    return true;
}

inline bool send_text(int fd, const std::string& text) {
    return send_frame(fd, protocol::FRAME_TEXT, text);
}

inline bool send_server_text(int fd, const std::string& text) {
    return send_frame(fd, protocol::FRAME_SERVER_TEXT, text);
}

inline std::string bytes_to_string(const std::vector<char>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

inline void close_socket(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

} // namespace utils
