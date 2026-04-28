#include "chat_client.h"
#include "audio_client.h"
#include "protocol.h"
#include "utils.h"
#include "video_client.h"

#include <arpa/inet.h>
#include <filesystem>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#ifdef _WIN32
#include <mmsystem.h>
#include <windows.h>
#endif

using namespace std;

ChatClient::ChatClient(std::string host, int port) : host_(std::move(host)), port_(port), socket_fd_(-1), running_(false) {}

ChatClient::~ChatClient()
{
    running_ = false;

    if (socket_fd_ >= 0)
    {
        utils::close_socket(socket_fd_);
        socket_fd_ = -1;
    }

    if (receiver_.joinable())
    {
        receiver_.join();
    }

    if (incoming_audio_.is_open())
    {
        incoming_audio_.close();
    }

    if (incoming_video_.is_open())
    {
        incoming_video_.close();
    }
}

bool ChatClient::connect_to_server()
{
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0)
    {
        cout << "ERROR: Could not create socket.\n";
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);

    if (::inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0)
    {
        cout << "ERROR: Invalid server address.\n";
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) < 0)
    {
        cout << "ERROR: Could not connect to server.\n";
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this](){ receive_loop(); });
    return true;
}

void ChatClient::run()
{
    cout << "Connected to server.\n";
    cout << "Commands:\n";
    cout << "  /join <group>\n";
    cout << "  /list\n";
    cout << "  /leave\n";
    cout << "  /audio <file_path>\n";
    cout << "  /video <file_path>\n";
    cout << "  /quit\n\n";

    std::string line;
    while (running_ && std::getline(std::cin, line))
    {
        line = utils::trim(line);
        if (line.empty())
        {
            continue;
        }

        if (line.rfind("/audio ", 0) == 0)
        {
            std::string path = utils::trim(line.substr(7));
            if (path.empty())
            {
                cout << "Usage: /audio <file_path>\n";
                continue;
            }

            if (!send_audio_file(socket_fd_, path))
            {
                cout << "ERROR: Could not send audio file.\n";
            }
            continue;
        }

        if (line.rfind("/video ", 0) == 0)
        {
            std::string path = utils::trim(line.substr(7));
            if (path.empty())
            {
                cout << "Usage: /video <file_path>\n";
                continue;
            }

            if (!send_video_file(socket_fd_, path))
            {
                cout << "ERROR: Could not send video file.\n";
            }
            continue;
        }

        if (!utils::send_text(socket_fd_, line))
        {
            cout << "Disconnected from server.\n";
            running_ = false;
            break;
        }

        if (line == "/quit")
        {
            running_ = false;
            break;
        }
    }
}

void ChatClient::receive_loop()
{
    uint8_t type = 0;
    std::vector<char> payload;

    while (running_ && utils::recv_frame(socket_fd_, type, payload))
    {
        if (type == protocol::FRAME_TEXT || type == protocol::FRAME_SERVER_TEXT)
        {
            handle_server_text(utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_AUDIO_BEGIN)
        {
            handle_audio_begin(utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_AUDIO_CHUNK)
        {
            handle_audio_chunk(payload);
        }
        else if (type == protocol::FRAME_AUDIO_END)
        {
            handle_audio_end();
        }
        else if (type == protocol::FRAME_VIDEO_BEGIN)
        {
            handle_video_begin(utils::bytes_to_string(payload));
        }
        else if (type == protocol::FRAME_VIDEO_CHUNK)
        {
            handle_video_chunk(payload);
        }
        else if (type == protocol::FRAME_VIDEO_END)
        {
            handle_video_end();
        }
        else
        {
            cout << "Received unknown frame type.\n";
        }
    }

    running_ = false;
}

void ChatClient::handle_server_text(const std::string &text)
{
    cout << text << endl;
}

void ChatClient::handle_audio_begin(const std::string &filename)
{
    std::filesystem::create_directories("downloads");
    incoming_audio_name_ = "downloads/received_" + filename;

    if (incoming_audio_.is_open())
    {
        incoming_audio_.close();
    }

    incoming_audio_.open(incoming_audio_name_, std::ios::binary);
    if (!incoming_audio_)
    {
        cout << "ERROR: Could not open file for incoming audio: " << incoming_audio_name_ << endl;
        return;
    }

    cout << "Receiving audio file: " << incoming_audio_name_ << endl;
}

void ChatClient::handle_audio_chunk(const std::vector<char> &chunk)
{
    if (incoming_audio_.is_open())
    {
        incoming_audio_.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    }
}

void ChatClient::handle_audio_end()
{
    if (!incoming_audio_.is_open())
    {
        cout << "Audio transfer ended, but no file was open.\n";
        return;
    }

    incoming_audio_.close();
    cout << "Audio saved to: " << incoming_audio_name_ << endl;
    play_received_audio();
}

void ChatClient::play_received_audio() const
{
#ifdef _WIN32
    if (!PlaySoundA(incoming_audio_name_.c_str(), nullptr, SND_FILENAME | SND_ASYNC))
    {
        cout << "Playback failed for: " << incoming_audio_name_ << ". PlaySoundA typically expects a WAV file.\n";
    }
#else
    cout << "Audio saved, but automatic playback is only enabled for Windows builds.\n";
#endif
}

void ChatClient::handle_video_begin(const std::string &filename)
{
    std::filesystem::create_directories("downloads");
    incoming_video_name_ = "downloads/received_" + filename;

    if (incoming_video_.is_open())
    {
        incoming_video_.close();
    }

    incoming_video_.open(incoming_video_name_, std::ios::binary);
    if (!incoming_video_)
    {
        cout << "ERROR: Could not open file for incoming video: " << incoming_video_name_ << endl;
        return;
    }

    cout << "Receiving video file: " << incoming_video_name_ << endl;
}

void ChatClient::handle_video_chunk(const std::vector<char> &chunk)
{
    if (incoming_video_.is_open())
    {
        incoming_video_.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    }
}

void ChatClient::handle_video_end()
{
    if (!incoming_video_.is_open())
    {
        cout << "Video transfer ended, but no file was open.\n";
        return;
    }

    incoming_video_.close();
    cout << "Video saved to: " << incoming_video_name_ << endl;
}
