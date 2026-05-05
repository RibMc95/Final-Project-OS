#include "chat_client.h"
#include "audio_client.h"
#include "protocol.h"
#include "utils.h"
#include "video_client.h"

#include <arpa/inet.h>
#include <filesystem>
#include <cstdlib>
#include <cctype>
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

namespace
{
    string quote_for_shell(const std::string &path)
    {
        std::string quoted = "'";
        for (char ch : path)
        {
            if (ch == '\'')
            {
                quoted += "'\\''";
            }
            else
            {
                quoted += ch;
            }
        }
        quoted += "'";
        return quoted;
    }

    string lower_ascii(std::string value)
    {
        for (char &ch : value)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return value;
    }

    string quote_for_cmd(const std::string &path)
    {
        std::string quoted = "\"";
        for (char ch : path)
        {
            if (ch == '"')
            {
                quoted += "\"\"";
            }
            else
            {
                quoted += ch;
            }
        }
        quoted += "\"";
        return quoted;
    }
}

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
    receiver_ = std::thread([this]()
                            { receive_loop(); });
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
    cout << "  /play [file_path]  (play the last received audio or a local file)\n";
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

        if (line.rfind("/play", 0) == 0)
        {
            std::string path = utils::trim(line.substr(5));
            if (path.empty())
            {
                play_received_audio();
            }
            else
            {
                play_audio_file(path);
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
            cout << "Received unknown frame type." << endl;
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
        cout << "Audio transfer ended, but no file was open." << endl;
        return;
    }

    incoming_audio_.close();
    cout << "Audio saved to: " << incoming_audio_name_ << endl;
    cout << "Type /play to play it, or /play <file_path> for another audio file." << endl;
}

void ChatClient::play_received_audio() const
{
    if (incoming_audio_name_.empty())
    {
        cout << "No audio received yet." << endl;
        return;
    }
#ifdef _WIN32
    play_audio_file(incoming_audio_name_);
#else
    play_audio_file(incoming_audio_name_);
#endif
}

void ChatClient::play_audio_file(const std::string &path) const
{
    if (path.empty())
    {
        cout << "Usage: /play <file_path>" << endl;
        return;
    }

    if (!std::filesystem::exists(path))
    {
        cout << "ERROR: Audio file not found: " << path << endl;
        return;
    }

#ifdef _WIN32
    const std::string abs_path = std::filesystem::absolute(path).string();
    const std::string ext = lower_ascii(std::filesystem::path(abs_path).extension().string());

    if ((ext == ".wav" || ext == ".wave") && PlaySoundA(abs_path.c_str(), nullptr, SND_FILENAME | SND_ASYNC))
    {
        cout << "Played via PlaySoundA: " << abs_path << endl;
        return;
    }

    const std::string open_default = "cmd /C start \"\" " + quote_for_cmd(abs_path) + " >nul 2>&1";
    if (std::system(open_default.c_str()) == 0)
    {
        cout << "Opened in Windows media player: " << abs_path << endl;
        return;
    }

    cout << "Playback failed for: " << path << ". Install or configure a default Windows media player." << endl;
#else
    const std::string abs_path = std::filesystem::absolute(path).string();
    const std::string quoted_path = quote_for_shell(abs_path);

    // In WSL, the Windows media player is usually more reliable than Linux audio sinks.
    if (std::getenv("WSL_DISTRO_NAME") != nullptr)
    {
        const std::string wsl_fallback = "win_path=$(wslpath -w " + quoted_path + ") && cmd.exe /C start \"\" \"$win_path\" >/dev/null 2>&1";
        if (std::system(wsl_fallback.c_str()) == 0)
        {
            cout << "Opened in Windows media player: " << abs_path << endl;
            return;
        }
    }

    const std::string ext = lower_ascii(std::filesystem::path(abs_path).extension().string());
    std::vector<std::pair<std::string, std::string>> commands;
    if (ext == ".wav" || ext == ".wave")
    {
        commands = {
            {"ffplay", "ffplay -nodisp -autoexit " + quoted_path + " >/dev/null 2>&1"},
            {"aplay", "aplay " + quoted_path + " >/dev/null 2>&1"},
            {"paplay", "paplay " + quoted_path + " >/dev/null 2>&1"},
            {"mpg123", "mpg123 " + quoted_path + " >/dev/null 2>&1"}};
    }
    else
    {
        commands = {
            {"ffplay", "ffplay -nodisp -autoexit " + quoted_path + " >/dev/null 2>&1"},
            {"mpg123", "mpg123 " + quoted_path + " >/dev/null 2>&1"},
            {"paplay", "paplay " + quoted_path + " >/dev/null 2>&1"},
            {"aplay", "aplay " + quoted_path + " >/dev/null 2>&1"}};
    }

    for (const auto &candidate : commands)
    {
        if (std::system(candidate.second.c_str()) == 0)
        {
            cout << "Played via " << candidate.first << ": " << path << endl;
            return;
        }
    }

    cout << "Playback failed for: " << path << ". Install ffplay, aplay, paplay, or mpg123 in the terminal environment." << endl;
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
        cout << "Video transfer ended, but no file was open." << endl;
        return;
    }

    incoming_video_.close();
    cout << "Video saved to: " << incoming_video_name_ << endl;
}
