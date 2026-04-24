#include "chat_client.h"
#include "../shared/utils.h"

#include <arpa/inet.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

ChatClient::ChatClient(std::string host, int port)
    : host_(std::move(host)), port_(port) {}

ChatClient::~ChatClient()
{
    running_ = false;
    utils::close_socket(socket_fd_);

    if (receiver_.joinable())
    {
        receiver_.join();
    }
}

bool ChatClient::connect_to_server()
{
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0)
    {
        std::cerr << "Could not create socket.\n";
        return false;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::inet_pton(AF_INET, host_.c_str(), &server_address.sin_addr) <= 0)
    {
        std::cerr << "Invalid address.\n";
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr *>(&server_address), sizeof(server_address)) < 0)
    {
        std::cerr << "Could not connect to server.\n";
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this]
                            { receive_loop(); });
    return true;
}

void ChatClient::run() // edit some commands can be handled client-side (like sending files)
{
    std::cout << "Type /join general to enter a group.\n";
    std::cout << "Commands: /join <group>, /list, /leave, /quit, /sendfile <path.wav>\n";

    std::string line;
    while (running_ && std::getline(std::cin, line))
    {
        if (line.rfind("/sendfile ", 0) == 0)
        {
            send_wav_file(utils::trim(line.substr(10)));
            continue;
        }

        if (!utils::send_line(socket_fd_, line))
        {
            break;
        }

        if (line == "/quit")
        {
            break;
        }
    }

    running_ = false;
}

void ChatClient::receive_loop() // continuously read lines from the server and print them to the console
{
    std::string line;

    while (running_ && utils::recv_line(socket_fd_, line))
    {
        if (line.rfind("FILE ", 0) == 0)
        {
            receive_wav_file(line);
            continue;
        }
        std::cout << line << std::endl;
    }

    running_ = false;
}

void ChatClient::send_wav_file(const std::string &path) // help user to send file
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Cannot open file: " << path << "\n";
        return;
    }

    auto raw_size = file.tellg();
    if (raw_size <= 0)
    {
        std::cerr << "File is empty or unreadable: " << path << "\n";
        return;
    }
    auto size = static_cast<std::size_t>(raw_size);

    file.seekg(0);
    std::vector<unsigned char> data(size);
    file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

    // Use only the base filename (strip directories).
    std::string filename = path;
    auto slash = path.rfind('/');
    if (slash != std::string::npos)
        filename = path.substr(slash + 1);
    auto backslash = filename.rfind('\\');
    if (backslash != std::string::npos)
        filename = filename.substr(backslash + 1);

    std::string header = "/sendfile " + filename + " " + std::to_string(size);
    if (!utils::send_line(socket_fd_, header))
    {
        std::cerr << "Failed to send file header.\n";
        return;
    }
    if (!utils::send_raw_bytes(socket_fd_, data))
    {
        std::cerr << "Failed to send file data.\n";
        return;
    }
    std::cout << "[you] Sending file: " << filename << " (" << size << " bytes)\n";
}

void ChatClient::receive_wav_file(const std::string &header) // help user to receive file
{
    // Format: FILE <sender_id> <filename> <byte_count>
    std::istringstream ss(header.substr(5)); // skip "FILE "
    int sender_id = 0;
    std::string filename;
    std::size_t file_size = 0;

    if (!(ss >> sender_id >> filename >> file_size))
    {
        std::cerr << "[warn] Malformed FILE header: " << header << "\n";
        return;
    }

    std::vector<unsigned char> data;
    if (!utils::recv_n_bytes(socket_fd_, file_size, data))
    {
        std::cerr << "[warn] Failed to receive file data from sender " << sender_id << ".\n";
        return;
    }

    std::string out_path = "received_" + filename;
    std::ofstream out(out_path, std::ios::binary);
    if (!out)
    {
        std::cerr << "[warn] Cannot save file: " << out_path << "\n";
        return;
    }
    out.write(reinterpret_cast<const char *>(data.data()),
              static_cast<std::streamsize>(data.size()));

    std::cout << "[Client " << sender_id << "] sent file '" << filename
              << "' (" << file_size << " bytes) -> saved as " << out_path << "\n";
}
