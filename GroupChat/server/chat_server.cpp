#include "chat_server.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace std;

ChatServer::ChatServer(int port, ScheduleMode mode, std::size_t worker_count) : port_(port), groups_("logs/chat_log.txt"), pool_(worker_count, mode) {}

ChatServer::~ChatServer()
{
    running_ = false;
    if (server_fd_ >= 0)
    {
        utils::close_socket(server_fd_);
    }

    for (auto &t : client_threads_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }
}

bool ChatServer::start()
{
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
    {
        cout << "Could not create socket.\n";
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::bind(server_fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        cout << "Bind failed. Is the port already in use?\n";
        return false;
    }

    if (::listen(server_fd_, 16) < 0)
    {
        cout << "Listen failed.\n";
        return false;
    }

    running_ = true;
    cout << "GroupChat server listening on port " << port_ << "\n";
    accept_loop();
    return true;
}

void ChatServer::accept_loop()
{
    while (running_)
    {
        sockaddr_in client_address{};
        socklen_t length = sizeof(client_address);

        int client_fd = ::accept(server_fd_, reinterpret_cast<sockaddr *>(&client_address), &length);
        if (client_fd < 0)
        {
            if (running_)
            {
                cout << "Accept failed.\n";
            }
            continue;
        }

        int client_id = next_client_id_++;
        groups_.register_client(client_fd, client_id);
        utils::send_server_text(client_fd, "INFO Welcome. Your sender ID is " + std::to_string(client_id) + ".");
        utils::send_server_text(client_fd, "INFO Commands: /join <group>, /list, /leave, /audio <file>, /video <file>, /play [file], /quit.");
        client_threads_.emplace_back([this, client_fd, client_id]
                                     { client_read_loop(client_fd, client_id); });
    }
}

void ChatServer::client_read_loop(int client_fd, int client_id)
{
    uint8_t type = 0;
    std::vector<char> payload;

    while (utils::recv_frame(client_fd, type, payload))
    {
        if (type == protocol::FRAME_TEXT)
        {
            std::string line = utils::bytes_to_string(payload);
            pool_.submit([this, client_fd, line]
                         { handle_text_line(client_fd, line); }, line.size());
        }
        else if (type == protocol::FRAME_AUDIO_BEGIN)
        {
            relay_audio_begin(client_fd, utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_AUDIO_CHUNK)
        {
            relay_audio_chunk(client_fd, payload);
        }
        else if (type == protocol::FRAME_AUDIO_END)
        {
            relay_audio_end(client_fd, utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_VIDEO_BEGIN)
        {
            relay_video_begin(client_fd, utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_VIDEO_CHUNK)
        {
            relay_video_chunk(client_fd, payload);
        }
        else if (type == protocol::FRAME_VIDEO_END)
        {
            relay_video_end(client_fd, utils::bytes_to_string(payload));
        }
        else
        {
            utils::send_server_text(client_fd, "ERROR Unknown frame type.");
        }
    }

    groups_.remove_client(client_fd);
    utils::close_socket(client_fd);
    std::cout << "Client " << client_id << " disconnected.\n";
}

void ChatServer::handle_text_line(int client_fd, const std::string &line)
{
    std::string clean = utils::trim(line);
    if (clean.empty())
    {
        return;
    }

    if (clean.rfind("/join ", 0) == 0)
    {
        groups_.join_group(client_fd, utils::trim(clean.substr(6)));
    }
    else if (clean == "/list")
    {
        groups_.send_group_list(client_fd);
    }
    else if (clean == "/leave")
    {
        groups_.leave_current_group(client_fd);
    }
    else if (clean == "/quit")
    {
        utils::send_server_text(client_fd, "INFO Bye.");
        groups_.remove_client(client_fd);
        utils::close_socket(client_fd);
    }
    else if (clean.rfind("/audio ", 0) == 0)
    {
        utils::send_server_text(client_fd, "ERROR Use /audio on the client terminal, not as a chat message.");
    }
    else if (clean.rfind("/video ", 0) == 0)
    {
        utils::send_server_text(client_fd, "ERROR Use /video on the client terminal, not as a chat message.");
    }
    else if (clean.rfind("/play", 0) == 0)
    {
        utils::send_server_text(client_fd, "ERROR Use /play [file] on the client terminal, not as a chat message.");
    }
    else
    {
        groups_.broadcast_message(client_fd, clean);
    }
}

void ChatServer::relay_audio_begin(int client_fd, const std::string &filename)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    if (targets.empty())
    {
        utils::send_server_text(client_fd, "ERROR Join a group before sending media: /join general");
        return;
    }

    std::string safe_name = protocol::safe_filename(filename);
    int sender_id = groups_.get_client_id(client_fd);
    std::string group = groups_.get_client_group(client_fd);

    for (int fd : targets)
    {
        utils::send_server_text(fd, "INFO sender=" + std::to_string(sender_id) + " is sharing audio '" + safe_name + "' in group=" + group);
        utils::send_frame(fd, protocol::FRAME_AUDIO_BEGIN, safe_name);
    }
}

void ChatServer::relay_audio_chunk(int client_fd, const std::vector<char> &chunk)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    for (int fd : targets)
    {
        utils::send_frame(fd, protocol::FRAME_AUDIO_CHUNK, chunk);
    }
}

void ChatServer::relay_audio_end(int client_fd, const std::string &filename)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    std::string safe_name = protocol::safe_filename(filename);
    int sender_id = groups_.get_client_id(client_fd);

    for (int fd : targets)
    {
        utils::send_frame(fd, protocol::FRAME_AUDIO_END, safe_name);
        utils::send_server_text(fd, "INFO audio transfer finished from sender=" + std::to_string(sender_id));
    }
}

void ChatServer::relay_video_begin(int client_fd, const std::string &filename)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    if (targets.empty())
    {
        utils::send_server_text(client_fd, "ERROR Join a group before sending media: /join general");
        return;
    }

    std::string safe_name = protocol::safe_filename(filename);
    int sender_id = groups_.get_client_id(client_fd);
    std::string group = groups_.get_client_group(client_fd);

    for (int fd : targets)
    {
        utils::send_server_text(fd, "INFO sender=" + std::to_string(sender_id) + " is sharing video '" + safe_name + "' in group=" + group);
        utils::send_frame(fd, protocol::FRAME_VIDEO_BEGIN, safe_name);
    }
}

void ChatServer::relay_video_chunk(int client_fd, const std::vector<char> &chunk)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    for (int fd : targets)
    {
        utils::send_frame(fd, protocol::FRAME_VIDEO_CHUNK, chunk);
    }
}

void ChatServer::relay_video_end(int client_fd, const std::string &filename)
{
    std::vector<int> targets = groups_.get_group_members_for_sender(client_fd);
    std::string safe_name = protocol::safe_filename(filename);
    int sender_id = groups_.get_client_id(client_fd);

    for (int fd : targets)
    {
        utils::send_frame(fd, protocol::FRAME_VIDEO_END, safe_name);
        utils::send_server_text(fd, "INFO video transfer finished from sender=" + std::to_string(sender_id));
    }
}
