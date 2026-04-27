#include "video_client.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

namespace  
{
    string basename_of(const string& path) 
    {
        size_t slash = path.find_last_of("/\\");
        if (slash == string::npos) 
        {
            return path;
        }
        return path.substr(slash + 1);
    }
}

bool send_video_file(int socket_fd, const string& file_path) 
{
    ifstream video(file_path, ios::binary);
    if (!video) 
    {
        cout << "Could not open video file: " << file_path << "\n";
        return false;
    }

    string filename = protocol::safe_filename(basename_of(file_path));

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

    cout << "Finished sending video file: " << filename << " (" << total_bytes << " bytes)\n";
    return true;
}
