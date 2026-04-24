#include "../shared/utils.h"

#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

int connect_bot(const std::string& host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &address.sin_addr);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        return -1;
    }

    return fd;
}

void bot_pause() {
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

void bot(const std::string& name, const std::string& group) {
    int fd = connect_bot("127.0.0.1", 5555);
    if (fd < 0) {
        std::cerr << "Bot could not connect. Start groupchat_server first.\n";
        return;
    }

    utils::send_line(fd, "/join " + group);
    bot_pause();
    utils::send_line(fd, "hello from " + name);
    bot_pause();
    utils::send_line(fd, "/list");
    bot_pause();
    utils::send_line(fd, "/quit");
    bot_pause();

    utils::close_socket(fd);
}

int main() {
    std::thread a(bot, "botA", "general");
    std::thread b(bot, "botB", "general");
    std::thread c(bot, "botC", "sports");

    a.join();
    b.join();
    c.join();

    std::cout << "Bot test finished. Check server output and logs/chat_log.txt.\n";
    return 0;
}
