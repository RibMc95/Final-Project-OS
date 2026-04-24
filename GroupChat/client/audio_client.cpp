#include <iostream>
#include <vector>

// Optional extra-credit placeholder.
// This keeps audio/video separate from the required text chat.
// A real upgrade could read .wav chunks and send them as MSG_MEDIA packets.
void demo_audio_chunk() {
    std::vector<unsigned char> fake_audio_chunk(256, 0);
    std::cout << "Simulated audio chunk size: "
              << fake_audio_chunk.size()
              << " bytes\n";
}
