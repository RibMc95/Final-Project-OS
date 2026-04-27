#pragma once

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

bool send_video_file(int socket_fd, const std::string& file_path);
