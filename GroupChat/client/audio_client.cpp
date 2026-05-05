#include "audio_client.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

namespace
{
    // Extracts the basename of a file path, e.g. "C:\Music\song.mp3" -> "song.mp3"
    string basename_of(const string &path)
    {
        size_t slash = path.find_last_of("/\\");
        if (slash == string::npos)
        {
            return path;
        }
        return path.substr(slash + 1);
    }

} // namespace

bool send_audio_file(int socket_fd, const string &file_path) // Returns true on success, false on failure.
{
    ifstream audio(file_path, ios::binary);
    if (!audio)
    {
        cout << "Could not open audio file: " << file_path << "\n";
        return false;
    }

    string filename = protocol::safe_filename(basename_of(file_path));

    if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_BEGIN, filename))
    {
        cout << "Could not send audio begin frame.\n";
        return false;
    }

    vector<char> buffer(protocol::AUDIO_CHUNK_SIZE);
    size_t total_bytes = 0;

    while (audio)
    {
        audio.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        streamsize count = audio.gcount();

        if (count > 0)
        {
            vector<char> chunk(buffer.begin(), buffer.begin() + count);
            if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_CHUNK, chunk))
            {
                cout << "Could not send audio chunk.\n";
                return false;
            }
            total_bytes += static_cast<size_t>(count);
        }
    }

    if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_END, filename))
    {
        cout << "Could not send audio end frame.\n";
        return false;
    }

    cout << "Sent audio file '" << filename << "' (" << total_bytes << " bytes).\n";
    return true;
}
