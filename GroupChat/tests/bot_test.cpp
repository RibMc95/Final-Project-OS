#include "../shared/utils.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

namespace
{
    constexpr int kPort = 5555;
    constexpr int kShortWaitMs = 300;
    constexpr int kDefaultWaitMs = 2500;

    bool contains(const string &text, const string &needle)
    {
        return text.find(needle) != string::npos;
    }

    int connect_bot(const string &host, int port)
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return -1;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<uint16_t>(port));

        if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0)
        {
            utils::close_socket(fd);
            return -1;
        }

        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
        {
            utils::close_socket(fd);
            return -1;
        }

        return fd;
    }

    bool recv_one_with_timeout(int fd, int timeout_ms, uint8_t &type, vector<char> &payload)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);

        timeval timeout{};
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int ready = ::select(fd + 1, &set, nullptr, nullptr, &timeout);
        if (ready <= 0)
        {
            return false;
        }

        return utils::recv_frame(fd, type, payload);
    }

    void drain_messages(int fd)
    {
        uint8_t type = 0;
        vector<char> payload;
        while (recv_one_with_timeout(fd, 50, type, payload))
        {
        }
    }

    bool wait_for_server_text(int fd, const string &needle, int timeout_ms = kDefaultWaitMs)
    {
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeout_ms);
        uint8_t type = 0;
        vector<char> payload;

        while (chrono::steady_clock::now() < deadline)
        {
            int remaining = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count());
            if (remaining <= 0)
            {
                break;
            }

            if (!recv_one_with_timeout(fd, min(remaining, 250), type, payload))
            {
                continue;
            }

            if (type == protocol::FRAME_SERVER_TEXT && contains(utils::bytes_to_string(payload), needle))
            {
                return true;
            }
        }

        return false;
    }

    bool wait_for_groups_text(int fd, string &out_text, int timeout_ms = kDefaultWaitMs)
    {
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeout_ms);
        uint8_t type = 0;
        vector<char> payload;

        while (chrono::steady_clock::now() < deadline)
        {
            int remaining = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count());
            if (remaining <= 0)
            {
                break;
            }

            if (!recv_one_with_timeout(fd, min(remaining, 250), type, payload))
            {
                continue;
            }

            if (type == protocol::FRAME_SERVER_TEXT)
            {
                string text = utils::bytes_to_string(payload);
                if (contains(text, "GROUPS "))
                {
                    out_text = text;
                    return true;
                }
            }
        }

        return false;
    }

    bool expect_no_server_text(int fd, const string &needle, int timeout_ms)
    {
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeout_ms);
        uint8_t type = 0;
        vector<char> payload;

        while (chrono::steady_clock::now() < deadline)
        {
            int remaining = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count());
            if (remaining <= 0)
            {
                break;
            }

            if (!recv_one_with_timeout(fd, min(remaining, 150), type, payload))
            {
                continue;
            }

            if (type == protocol::FRAME_SERVER_TEXT && contains(utils::bytes_to_string(payload), needle))
            {
                return false;
            }
        }

        return true;
    }

    bool wait_for_frame_type(int fd, uint8_t expected_type, int timeout_ms = kDefaultWaitMs)
    {
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeout_ms);
        uint8_t type = 0;
        vector<char> payload;

        while (chrono::steady_clock::now() < deadline)
        {
            int remaining = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count());
            if (remaining <= 0)
            {
                break;
            }

            if (!recv_one_with_timeout(fd, min(remaining, 200), type, payload))
            {
                continue;
            }

            if (type == expected_type)
            {
                return true;
            }
        }

        return false;
    }

    bool expect_no_frame_type(int fd, uint8_t forbidden_type, int timeout_ms)
    {
        auto deadline = chrono::steady_clock::now() + chrono::milliseconds(timeout_ms);
        uint8_t type = 0;
        vector<char> payload;

        while (chrono::steady_clock::now() < deadline)
        {
            int remaining = static_cast<int>(chrono::duration_cast<chrono::milliseconds>(deadline - chrono::steady_clock::now()).count());
            if (remaining <= 0)
            {
                break;
            }

            if (!recv_one_with_timeout(fd, min(remaining, 120), type, payload))
            {
                continue;
            }

            if (type == forbidden_type)
            {
                return false;
            }
        }

        return true;
    }
}

int main()
{
    int alice = connect_bot("127.0.0.1", kPort);
    int bob = connect_bot("127.0.0.1", kPort);
    int charlie = connect_bot("127.0.0.1", kPort);

    if (alice < 0 || bob < 0 || charlie < 0)
    {
        cout << "FAIL: Bots could not connect. Start groupchat_server first.\n";
        if (alice >= 0)
            utils::close_socket(alice);
        if (bob >= 0)
            utils::close_socket(bob);
        if (charlie >= 0)
            utils::close_socket(charlie);
        return 1;
    }

    int failures = 0;
    auto check = [&](bool ok, const string &feature)
    {
        cout << (ok ? "PASS " : "FAIL ") << feature << "\n";
        if (!ok)
        {
            ++failures;
        }
    };

    drain_messages(alice);
    drain_messages(bob);
    drain_messages(charlie);

    utils::send_text(alice, "/join general");
    utils::send_text(bob, "/join general");
    utils::send_text(charlie, "/join sports");

    check(wait_for_server_text(alice, "INFO Joined group 'general'."), "join general (alice)");
    check(wait_for_server_text(bob, "INFO Joined group 'general'."), "join general (bob)");
    check(wait_for_server_text(charlie, "INFO Joined group 'sports'."), "join sports (charlie)");

    utils::send_text(alice, "/list");
    string groups_line;
    bool list_ok = wait_for_groups_text(alice, groups_line) && contains(groups_line, "general(") && contains(groups_line, "sports(");
    check(list_ok, "list groups");

    const string msg_one = "feature-msg-one";
    utils::send_text(alice, msg_one);
    check(wait_for_server_text(alice, msg_one), "message sender receives broadcast");
    check(wait_for_server_text(bob, msg_one), "message peer in same group receives broadcast");
    check(expect_no_server_text(charlie, msg_one, kDefaultWaitMs), "message isolated from other group");

    utils::send_text(bob, "/leave");
    check(wait_for_server_text(bob, "INFO Left group 'general'."), "leave group");

    const string msg_two = "feature-msg-after-leave";
    utils::send_text(alice, msg_two);
    check(wait_for_server_text(alice, msg_two), "message after leave still delivered to sender");
    check(expect_no_server_text(bob, msg_two, kDefaultWaitMs), "left member does not receive future message");

    utils::send_text(alice, "/audio fake.wav");
    check(wait_for_server_text(alice, "ERROR Use /audio on the client terminal"), "audio command guard");

    utils::send_text(alice, "/video fake.mp4");
    check(wait_for_server_text(alice, "ERROR Use /video on the client terminal"), "video command guard");

    utils::send_text(alice, "/play fake.mp3");
    check(wait_for_server_text(alice, "ERROR Use /play [file] on the client terminal"), "play command guard");

    utils::send_text(bob, "/join general");
    check(wait_for_server_text(bob, "INFO Joined group 'general'."), "rejoin general for relay checks");

    drain_messages(alice);
    drain_messages(bob);
    drain_messages(charlie);

    vector<char> audio_chunk{'a', 'u', 'd', 'i', 'o'};
    utils::send_frame(alice, protocol::FRAME_AUDIO_BEGIN, string("bot-audio.mp3"));
    utils::send_frame(alice, protocol::FRAME_AUDIO_CHUNK, audio_chunk);
    utils::send_frame(alice, protocol::FRAME_AUDIO_END, string("bot-audio.mp3"));

    check(wait_for_frame_type(bob, protocol::FRAME_AUDIO_BEGIN), "audio begin relayed to group member");
    check(wait_for_frame_type(bob, protocol::FRAME_AUDIO_CHUNK), "audio chunk relayed to group member");
    check(wait_for_frame_type(bob, protocol::FRAME_AUDIO_END), "audio end relayed to group member");
    check(expect_no_frame_type(charlie, protocol::FRAME_AUDIO_BEGIN, kDefaultWaitMs), "audio isolated from other group");

    vector<char> video_chunk{'v', 'i', 'd', 'e', 'o'};
    utils::send_frame(alice, protocol::FRAME_VIDEO_BEGIN, string("bot-video.mp4"));
    utils::send_frame(alice, protocol::FRAME_VIDEO_CHUNK, video_chunk);
    utils::send_frame(alice, protocol::FRAME_VIDEO_END, string("bot-video.mp4"));

    check(wait_for_frame_type(bob, protocol::FRAME_VIDEO_BEGIN), "video begin relayed to group member");
    check(wait_for_frame_type(bob, protocol::FRAME_VIDEO_CHUNK), "video chunk relayed to group member");
    check(wait_for_frame_type(bob, protocol::FRAME_VIDEO_END), "video end relayed to group member");
    check(expect_no_frame_type(charlie, protocol::FRAME_VIDEO_BEGIN, kDefaultWaitMs), "video isolated from other group");

    utils::send_text(alice, "/quit");
    utils::send_text(bob, "/quit");
    utils::send_text(charlie, "/quit");
    std::this_thread::sleep_for(std::chrono::milliseconds(kShortWaitMs));

    utils::close_socket(alice);
    utils::close_socket(bob);
    utils::close_socket(charlie);

    if (failures == 0)
    {
        cout << "PASS all bot feature checks\n";
        return 0;
    }

    cout << "FAIL total failures: " << failures << "\n";
    return 1;
}
