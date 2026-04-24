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
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace utils
{

    inline std::string trim(std::string s)
    {
        auto not_space = [](unsigned char c)
        { return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        return s;
    }

    inline std::string now_string()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
        localtime_r(&t, &local);

        std::ostringstream out;
        out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
        return out.str();
    }

inline bool send_all(int fd, const std::string& data) {
    const char* buffer = data.c_str();
    std::size_t total = 0;
    std::size_t length = data.size();

        while (total < length)
        {
            ssize_t sent = ::send(fd, buffer + total, length - total, 0);
            if (sent <= 0)
            {
                return false;
            }
            total += static_cast<std::size_t>(sent);
        }
        return true;
    }

inline bool send_line(int fd, const std::string& line) {
    return send_all(fd, line + "\n");
}

inline bool recv_line(int fd, std::string& line) {
    line.clear();
    char ch = '\0';

    while (line.size() < 1024) {
        ssize_t received = ::recv(fd, &ch, 1, 0);
        if (received <= 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            line.push_back(ch);
        }
    }

    return true;
}

    // Send exactly n raw bytes (no framing added).
    inline bool send_raw_bytes(int fd, const std::vector<unsigned char> &data)
    {
        const char *buf = reinterpret_cast<const char *>(data.data());
        std::size_t total = 0;
        std::size_t length = data.size();

        while (total < length)
        {
            ssize_t sent = ::send(fd, buf + total, length - total, 0);
            if (sent <= 0)
            {
                return false;
            }
            total += static_cast<std::size_t>(sent);
        }
        return true;
    }

    // Read exactly n bytes from the socket into out.
    inline bool recv_n_bytes(int fd, std::size_t n, std::vector<unsigned char> &out)
    {
        out.resize(n);
        std::size_t total = 0;

        while (total < n)
        {
            ssize_t received = ::recv(fd,
                                      reinterpret_cast<char *>(out.data()) + total,
                                      n - total, 0);
            if (received <= 0)
            {
                return false;
            }
            total += static_cast<std::size_t>(received);
        }
        return true;
    }

    inline void close_socket(int fd)
    {
        if (fd >= 0)
        {
            ::close(fd);
        }
    }

} // namespace utils
