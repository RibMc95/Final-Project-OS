#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

class ChatClient
{
public:
    ChatClient(std::string host, int port);
    ~ChatClient();

    bool connect_to_server();
    void run();

private:
    std::string host_;
    int port_;
    int socket_fd_;
    std::atomic<bool> running_;
    std::thread receiver_;

    std::ofstream incoming_audio_;
    std::string incoming_audio_name_;

    std::ofstream incoming_video_;
    std::string incoming_video_name_;

    /*
        Stores the most recent media file that this client can play/open.

        This is updated when:
        - this client sends audio with /audio
        - this client sends video with /video
        - this client receives audio and the transfer finishes
        - this client receives video and the transfer finishes

        This is what makes plain /play work.
    */
    std::string last_media_name_;

    void receive_loop();

    void handle_server_text(const std::string &text);

    void handle_audio_begin(const std::string &filename);
    void handle_audio_chunk(const std::vector<char> &chunk);
    void handle_audio_end();

    void handle_video_begin(const std::string &filename);
    void handle_video_chunk(const std::vector<char> &chunk);
    void handle_video_end();

    void play_last_media() const;
    void play_media_file(const std::string &path) const;
};