#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Returns true if the bytes look like a valid PCM WAV file.
// Checks the RIFF/WAVE magic bytes only — does not validate the full header.
bool is_valid_wav(const std::vector<unsigned char> &data)
{
    // Minimum WAV header is 44 bytes.
    if (data.size() < 44)
    {
        return false;
    }
    // Bytes  0-3 : "RIFF" Bytes  8-11: "WAVE" 
    return data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' && data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E';
}

// Reads a .wav file from disk into a byte vector.
// Returns an empty vector on failure.
std::vector<unsigned char> load_wav_file(const std::string &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cout << "[audio] Cannot open: " << path << "\n";
        return {};
    }

    auto size = file.tellg();
    if (size <= 0)
    {
        std::cout << "[audio] File is empty: " << path << "\n";
        return {};
    }

    file.seekg(0);
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);

    if (!is_valid_wav(data))
    {
        std::cout << "[audio] File does not appear to be a WAV: " << path << "\n";
        return {};
    }

    return data;
}

// Saves received bytes to disk as a .wav file.
// Returns true on success.
bool save_wav_file(const std::string &path, const std::vector<unsigned char> &data)
{
    if (!is_valid_wav(data))
    {
        std::cout << "[audio] Received data is not a valid WAV file.\n";
        return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        std::cout << "[audio] Cannot write: " << path << "\n";
        return false;
    }

    out.write(reinterpret_cast<const char *>(data.data()),static_cast<std::streamsize>(data.size()));
    return out.good();
}
