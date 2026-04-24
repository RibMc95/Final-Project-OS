#include "audio_client.h"
#include "../shared/protocol.h"
#include "../shared/utils.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::string basename_of(const std::string& path) {
    std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

} // namespace

bool send_audio_file(int socket_fd, const std::string& file_path) {
    std::ifstream audio(file_path, std::ios::binary);
    if (!audio) {
        std::cerr << "Could not open audio file: " << file_path << "\n";
        return false;
    }

    std::string filename = protocol::safe_filename(basename_of(file_path));

    if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_BEGIN, filename)) {
        std::cerr << "Could not send audio begin frame.\n";
        return false;
    }

    std::vector<char> buffer(protocol::AUDIO_CHUNK_SIZE);
    std::size_t total_bytes = 0;

    while (audio) {
        audio.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize count = audio.gcount();

        if (count > 0) {
            std::vector<char> chunk(buffer.begin(), buffer.begin() + count);
            if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_CHUNK, chunk)) {
                std::cerr << "Could not send audio chunk.\n";
                return false;
            }
            total_bytes += static_cast<std::size_t>(count);
        }
    }

    if (!utils::send_frame(socket_fd, protocol::FRAME_AUDIO_END, filename)) {
        std::cerr << "Could not send audio end frame.\n";
        return false;
    }

    std::cout << "Sent audio file '" << filename << "' (" << total_bytes << " bytes).\n";
    return true;
}
