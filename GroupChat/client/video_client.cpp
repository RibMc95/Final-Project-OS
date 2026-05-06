#include "video_client.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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
                quoted += "'\\''";
            else
                quoted += ch;
        }
        quoted += "'";
        return quoted;
    }

    string basename_of(const string &path)
    {
        size_t slash = path.find_last_of("/\\");
        if (slash == string::npos)
        {
            return path;
        }
        return path.substr(slash + 1);
    }

    bool command_exists(const string &name)
    {
        const string check = "command -v " + name + " >/dev/null 2>&1";
        return std::system(check.c_str()) == 0;
    }

    bool build_compatible_mp4(const string &input_path, string &output_path)
    {
#ifdef _WIN32
        (void)input_path;
        (void)output_path;
        return false;
#else
        if (!command_exists("ffmpeg"))
        {
            return false;
        }

        std::filesystem::create_directories("downloads");
        std::string stem = std::filesystem::path(input_path).stem().string();
        stem = protocol::safe_filename(stem);
        output_path = (std::filesystem::path("downloads") / (stem + "_compat.mp4")).string();

        const string cmd =
            "ffmpeg -y -i " + quote_for_shell(input_path) +
            " -c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 128k " +
            quote_for_shell(output_path) + " >/dev/null 2>&1";

        if (std::system(cmd.c_str()) != 0)
        {
            output_path.clear();
            return false;
        }

        return std::filesystem::exists(output_path);
#endif
    }
}

bool send_video_file(int socket_fd, const string &file_path)
{
    string path_to_send = file_path;
    string filename = protocol::safe_filename(basename_of(file_path));

    string converted_path;
    if (build_compatible_mp4(file_path, converted_path))
    {
        path_to_send = converted_path;
        filename = protocol::safe_filename(basename_of(path_to_send));
        cout << "Converted video to a Media Player compatible MP4: " << filename << "\n";
    }
#ifndef _WIN32
    else
    {
        cout << "Could not create a Media Player compatible MP4 from: " << file_path << "\n";
        cout << "Install ffmpeg to send compatible video files from Linux/WSL.\n";
        return false;
    }
#endif

    ifstream video(path_to_send, ios::binary);
    if (!video)
    {
        cout << "Could not open video file: " << path_to_send << "\n";
        return false;
    }

    if (!utils::send_frame(socket_fd, protocol::FRAME_VIDEO_BEGIN, filename))
    {
        cout << "Could not send video begin frame.\n";
        return false;
    }

    vector<char> buffer(protocol::VIDEO_CHUNK_SIZE);
    size_t total_bytes = 0;

    while (video)
    {
        video.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        streamsize count = video.gcount();

        if (count > 0)
        {
            vector<char> chunk(buffer.begin(), buffer.begin() + count);
            if (!utils::send_frame(socket_fd, protocol::FRAME_VIDEO_CHUNK, chunk))
            {
                cout << "Could not send video chunk.\n";
                return false;
            }
            total_bytes += static_cast<size_t>(count);
        }
    }

    if (!utils::send_frame(socket_fd, protocol::FRAME_VIDEO_END, filename))
    {
        cout << "Could not send video end frame.\n";
        return false;
    }

    cout << "Sent video file '" << filename << "' (" << total_bytes << " bytes).\n";
    return true;
}
