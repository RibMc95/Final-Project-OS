#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace protocol {

// Every network message is sent as:
// 1 byte  = frame type
// 4 bytes = payload size
// N bytes = payload data
//
// This lets the chat send both normal text and binary audio files.

constexpr int DEFAULT_PORT = 5555;
constexpr std::size_t MAX_LINE = 1024;
constexpr std::size_t HISTORY_LIMIT = 20;
constexpr std::size_t AUDIO_CHUNK_SIZE = 4096;
constexpr std::size_t MAX_FRAME_SIZE = 1024 * 1024;

constexpr uint8_t FRAME_TEXT = 1;
constexpr uint8_t FRAME_AUDIO_BEGIN = 2;
constexpr uint8_t FRAME_AUDIO_CHUNK = 3;
constexpr uint8_t FRAME_AUDIO_END = 4;
constexpr uint8_t FRAME_SERVER_TEXT = 5;

inline std::string safe_filename(std::string name) {
    if (name.empty()) {
        return "audio.bin";
    }

    for (char& ch : name) {
        bool ok = (ch >= 'a' && ch <= 'z') ||
                  (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') ||
                  ch == '.' || ch == '_' || ch == '-';

        if (!ok) {
            ch = '_';
        }
    }

    return name;
}

} // namespace protocol