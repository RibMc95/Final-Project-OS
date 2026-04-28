#include "../shared/utils.h"
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
using namespace std;

int connect_bot(const std::string& host, int port) 
{
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

void bot_pause() 
{
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

void drain_some_messages(int fd) 
{
    fd_set set;
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    FD_ZERO(&set);
    FD_SET(fd, &set);

    while (select(fd + 1, &set, nullptr, nullptr, &timeout) > 0) {
        uint8_t type = 0;
        std::vector<char> payload;
        if (!utils::recv_frame(fd, type, payload)) {
            return;
        }
        if (type == protocol::FRAME_SERVER_TEXT) {
            cout << utils::bytes_to_string(payload) << "\n";
        }

        FD_ZERO(&set);
        FD_SET(fd, &set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
    }
}

void bot(const std::string& name, const std::string& group) 
{
    int fd = connect_bot("127.0.0.1", 5555);
    if (fd < 0) 
    {
        cout << "Bot could not connect. Start groupchat_server first.\n";
        return;
    }

    utils::send_text(fd, "/join " + group);
    bot_pause();
    utils::send_text(fd, "hello from " + name);
    bot_pause();
    utils::send_text(fd, "/list");
    bot_pause();
    drain_some_messages(fd);
    utils::send_text(fd, "/quit");
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
