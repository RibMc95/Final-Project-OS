#include "chat_client.h"
#include "audio_client.h"
#include "protocol.h"
#include "utils.h"
#include "video_client.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <arpa/inet.h>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

namespace
{
    string quote_for_shell(const string &path)
    {
        string quoted = "'";

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

#ifndef _WIN32
    bool command_exists(const string &command)
    {
        string check_command = "command -v " + command + " >/dev/null 2>&1";
        return system(check_command.c_str()) == 0;
    }
#endif

#ifdef _WIN32
    string quote_for_cmd(const string &path)
    {
        string quoted = "\"";

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
#endif

    string lowercase_extension(const string &path)
    {
        string ext = filesystem::path(path).extension().string();
        string lowered;

        for (char c : ext)
        {
            lowered += static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }

        return lowered;
    }

    bool is_audio_extension(const string &ext)
    {
        return ext == ".mp3" ||
               ext == ".wav" ||
               ext == ".wave" ||
               ext == ".ogg" ||
               ext == ".flac" ||
               ext == ".m4a" ||
               ext == ".aac";
    }

    bool is_video_extension(const string &ext)
    {
        return ext == ".mp4" ||
               ext == ".mov" ||
               ext == ".avi" ||
               ext == ".mkv" ||
               ext == ".webm" ||
               ext == ".wmv" ||
               ext == ".m4v";
    }
}

ChatClient::ChatClient(string host, int port)
    : host_(move(host)), port_(port), socket_fd_(-1), running_(false)
{
}

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

    receiver_ = thread([this]()
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
    cout << "  /play [file_path]  (open the last sent/received audio/video or a local media file)\n";
    cout << "  /quit\n\n";

    string line;

    while (running_ && getline(cin, line))
    {
        line = utils::trim(line);

        if (line.empty())
        {
            continue;
        }

        if (line.rfind("/audio ", 0) == 0)
        {
            string path = utils::trim(line.substr(7));

            if (path.empty())
            {
                cout << "Usage: /audio <file_path>\n";
                continue;
            }

            if (!filesystem::exists(path))
            {
                cout << "ERROR: Audio file not found: " << path << endl;
                continue;
            }

            if (!send_audio_file(socket_fd_, path))
            {
                cout << "ERROR: Could not send audio file.\n";
            }
            else
            {
                /*
                    This makes /play work on the sender side too.
                    Without this, /play only works on the receiver side.
                */
                last_media_name_ = path;
            }

            continue;
        }

        if (line.rfind("/video ", 0) == 0)
        {
            string path = utils::trim(line.substr(7));

            if (path.empty())
            {
                cout << "Usage: /video <file_path>\n";
                continue;
            }

            if (!filesystem::exists(path))
            {
                cout << "ERROR: Video file not found: " << path << endl;
                continue;
            }

            if (!send_video_file(socket_fd_, path))
            {
                cout << "ERROR: Could not send video file.\n";
            }
            else
            {
                /*
                    This makes /play work immediately after you send a video.
                    Example:
                        /video test4.mp4
                        /play
                    Now /play opens test4.mp4 on the sender side.
                */
                last_media_name_ = path;
            }

            continue;
        }

        if (line == "/play" || line.rfind("/play ", 0) == 0)
        {
            string path;

            if (line.size() > 5)
            {
                path = utils::trim(line.substr(5));
            }

            if (path.empty())
            {
                play_last_media();
            }
            else
            {
                play_media_file(path);
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
    vector<char> payload;

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

void ChatClient::handle_server_text(const string &text)
{
    cout << text << endl;
}

void ChatClient::handle_audio_begin(const string &filename)
{
    filesystem::create_directories("downloads");

    incoming_audio_name_ = "downloads/received_" + protocol::safe_filename(filename);

    if (incoming_audio_.is_open())
    {
        incoming_audio_.close();
    }

    incoming_audio_.open(incoming_audio_name_, ios::binary);

    if (!incoming_audio_)
    {
        cout << "ERROR: Could not open file for incoming audio: " << incoming_audio_name_ << endl;
        return;
    }

    cout << "Receiving audio file: " << incoming_audio_name_ << endl;
}

void ChatClient::handle_audio_chunk(const vector<char> &chunk)
{
    if (incoming_audio_.is_open())
    {
        incoming_audio_.write(chunk.data(), static_cast<streamsize>(chunk.size()));
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

    /*
        This makes plain /play open the most recently received audio.
    */
    last_media_name_ = incoming_audio_name_;

    cout << "Audio saved to: " << incoming_audio_name_ << endl;
    cout << "Type /play to open it, or /play <file_path> for another audio/video file." << endl;
}

void ChatClient::handle_video_begin(const string &filename)
{
    filesystem::create_directories("downloads");

    incoming_video_name_ = "downloads/received_" + protocol::safe_filename(filename);

    if (incoming_video_.is_open())
    {
        incoming_video_.close();
    }

    incoming_video_.open(incoming_video_name_, ios::binary);

    if (!incoming_video_)
    {
        cout << "ERROR: Could not open file for incoming video: " << incoming_video_name_ << endl;
        return;
    }

    cout << "Receiving video file: " << incoming_video_name_ << endl;
}

void ChatClient::handle_video_chunk(const vector<char> &chunk)
{
    if (incoming_video_.is_open())
    {
        incoming_video_.write(chunk.data(), static_cast<streamsize>(chunk.size()));
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

    /*
        This is the main fix for your issue.

        After the receiver finishes saving:
            downloads/received_test4.mp4

        plain /play will open:
            downloads/received_test4.mp4
    */
    last_media_name_ = incoming_video_name_;

    cout << "Video saved to: " << incoming_video_name_ << endl;
    cout << "Type /play to open it, or /play <file_path> for another audio/video file." << endl;
}

void ChatClient::play_last_media() const
{
    if (last_media_name_.empty())
    {
        cout << "No audio or video has been sent or received yet." << endl;
        cout << "Use /audio <file_path> or /video <file_path> first." << endl;
        return;
    }

    play_media_file(last_media_name_);
}

void ChatClient::play_media_file(const string &path) const
{
    if (path.empty())
    {
        cout << "Usage: /play <file_path>" << endl;
        return;
    }

    if (!filesystem::exists(path))
    {
        cout << "ERROR: Media file not found: " << path << endl;
        return;
    }

    if (!filesystem::is_regular_file(path))
    {
        cout << "ERROR: This path is not a regular media file: " << path << endl;
        return;
    }

    const string abs_path = filesystem::absolute(path).string();
    const string ext = lowercase_extension(abs_path);

    bool is_audio = is_audio_extension(ext);
    bool is_video = is_video_extension(ext);

    if (!is_audio && !is_video)
    {
        cout << "WARNING: Unknown media extension '" << ext << "'." << endl;
        cout << "Trying to open it anyway: " << abs_path << endl;
    }

#ifdef _WIN32
    /*
        Native Windows:
        WAV audio can be played directly with PlaySound.
        MP3, MP4, MOV, AVI, and other files open in the default Windows app.
    */
    if (is_audio && (ext == ".wav" || ext == ".wave"))
    {
        if (PlaySoundA(abs_path.c_str(), NULL, SND_FILENAME | SND_ASYNC))
        {
            cout << "Playing WAV file: " << path << endl;
        }
        else
        {
            cout << "Playback failed for: " << path << ". Could not play WAV file." << endl;
        }

        return;
    }

    const string open_cmd = "cmd /C start \"\" " + quote_for_cmd(abs_path) + " >nul 2>&1";

    if (system(open_cmd.c_str()) == 0)
    {
        if (is_video)
        {
            cout << "Opened video in Windows default media player: " << path << endl;
        }
        else if (is_audio)
        {
            cout << "Opened audio in Windows default media player: " << path << endl;
        }
        else
        {
            cout << "Opened file in Windows default app: " << path << endl;
        }
    }
    else
    {
        cout << "Playback failed for: " << path << "." << endl;
        cout << "Make sure Windows has a default app for this file type." << endl;
    }
#else
    const string quoted_path = quote_for_shell(abs_path);

    auto env_exists = [](const char *name) -> bool
    {
        const char *value = getenv(name);
        return value != nullptr && string(value).size() > 0;
    };

    bool is_codespaces = env_exists("CODESPACES");
    bool is_wsl = env_exists("WSL_DISTRO_NAME") || env_exists("WSL_INTEROP");
    bool has_linux_audio_device = filesystem::exists("/dev/snd");

    /*
        GitHub Codespaces:
        Codespaces usually has no audio device and no GUI video player.
        So do not force ffplay/xdg-open. Instead, show where the file is saved.
    */
    if (is_codespaces)
    {
        cout << "Media file is ready:" << endl;
        cout << "  " << abs_path << endl;
        cout << endl;
        cout << "This is a GitHub Codespace." << endl;
        cout << "Codespaces usually cannot play audio/video directly from the terminal." << endl;
        cout << "There is no Linux sound device such as /dev/snd, and there may be no GUI video player." << endl;
        cout << endl;

        if (command_exists("ffprobe"))
        {
            cout << "Checking media file information with ffprobe:" << endl;
            string probe_cmd = "ffprobe -hide_banner " + quoted_path;
            system(probe_cmd.c_str());
            cout << endl;
        }

        cout << "To watch or hear it, download/open this file from VS Code Explorer:" << endl;
        cout << "  " << abs_path << endl;
        cout << endl;
        cout << "Right-click the file, then choose Download." << endl;
        return;
    }

    /*
        WSL:
        Open audio/video with the Windows default app.
    */
    if (is_wsl && command_exists("wslpath"))
    {
        string command =
            "powershell.exe -NoProfile -Command \"Start-Process -FilePath \\\"$(wslpath -w " +
            quoted_path +
            ")\\\"\"";

        cout << "Opening media file with the Windows default app:" << endl;
        cout << "  " << abs_path << endl;

        if (system(command.c_str()) == 0)
        {
            return;
        }

        cout << "Could not open the file with Windows default app." << endl;
        cout << "You can still manually open this file:" << endl;
        cout << "  " << abs_path << endl;
        return;
    }

    /*
        Normal Linux desktop:
        For videos, xdg-open is usually best because it opens VLC, Videos, etc.
        For audio, try terminal players first if there is an audio device.
    */
    if (is_video && command_exists("xdg-open"))
    {
        const string cmd = "xdg-open " + quoted_path + " >/dev/null 2>&1 &";

        if (system(cmd.c_str()) == 0)
        {
            cout << "Opened video with the default Linux app:" << endl;
            cout << "  " << abs_path << endl;
            return;
        }
    }

    if (is_audio && has_linux_audio_device)
    {
        if (command_exists("ffplay"))
        {
            const string cmd = "ffplay -nodisp -autoexit -loglevel error " + quoted_path;
            cout << "Playing audio with ffplay: " << path << endl;

            if (system(cmd.c_str()) == 0)
            {
                return;
            }
        }

        if (command_exists("mpg123"))
        {
            const string cmd = "mpg123 -q " + quoted_path;
            cout << "Playing audio with mpg123: " << path << endl;

            if (system(cmd.c_str()) == 0)
            {
                return;
            }
        }

        if ((ext == ".wav" || ext == ".wave") && command_exists("paplay"))
        {
            const string cmd = "paplay " + quoted_path;
            cout << "Playing audio with paplay: " << path << endl;

            if (system(cmd.c_str()) == 0)
            {
                return;
            }
        }

        if ((ext == ".wav" || ext == ".wave") && command_exists("aplay"))
        {
            const string cmd = "aplay -q " + quoted_path;
            cout << "Playing audio with aplay: " << path << endl;

            if (system(cmd.c_str()) == 0)
            {
                return;
            }
        }
    }

    /*
        Linux fallback:
        Try xdg-open for any media file if available.
    */
    if (command_exists("xdg-open"))
    {
        const string cmd = "xdg-open " + quoted_path + " >/dev/null 2>&1 &";

        if (system(cmd.c_str()) == 0)
        {
            cout << "Opened media file with the default Linux app:" << endl;
            cout << "  " << abs_path << endl;
            return;
        }
    }

    /*
        Final fallback:
        The transfer still worked; this machine just cannot open/play it.
    */
    cout << "Media file is ready, but this terminal cannot open/play it directly." << endl;
    cout << "Saved file:" << endl;
    cout << "  " << abs_path << endl;
    cout << endl;

    if (is_audio && !has_linux_audio_device)
    {
        cout << "Reason: Linux cannot find an audio device such as /dev/snd." << endl;
    }
    else if (is_video)
    {
        cout << "Reason: Linux could not find a GUI/default video player." << endl;
    }
    else
    {
        cout << "Reason: No suitable media opener/player was found." << endl;
    }

    cout << "Download or open the file locally to hear/watch it." << endl;
    return;
#endif
}