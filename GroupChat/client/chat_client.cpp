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
using namespace std;

ChatClient::ChatClient(std::string host, int port) : host_(std::move(host)), port_(port) {}

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
        cout << "Could not create socket.\n";
        return false;
    }

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(static_cast<uint16_t>(port_));

    if (::inet_pton(AF_INET, host_.c_str(), &server_address.sin_addr) <= 0)
    {
        cout << "Invalid address.\n";
        return false;
    }

    if (::connect(socket_fd_, reinterpret_cast<sockaddr *>(&server_address), sizeof(server_address)) < 0)
    {
        cout << "Could not connect to server.\n";
        return false;
    }

    running_ = true;
    receiver_ = std::thread([this] { receive_loop(); });
    return true;
}

void ChatClient::run() // edit some commands can be handled client-side (like sending files)
{
    cout << "Type /join general to enter a group.\n";
    cout << "Commands: /join <group>, /list, /leave, /quit, /sendfile <path.wav>\n";

    string line;
    while (running_ && getline(cin, line))
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
    string line;

    while (running_ && utils::recv_line(socket_fd_, line))
    {
        if (line.rfind("FILE ", 0) == 0)
        {
            receive_wav_file(line);
            continue;
        }
        cout << line << endl;
    }

    running_ = false;
}

void ChatClient::send_wav_file(const std::string &path) // help user to send file
{
    ifstream file(path, ios::binary | ios::ate);
    if (!file)
    {
        cout << "Cannot open file: " << path << "\n";
        return;
    }

    auto raw_size = file.tellg();
    if (raw_size <= 0)
    {
        cout << "File is empty or unreadable: " << path << "\n";
        return;
    }
    auto size = static_cast<std::size_t>(raw_size);

    file.seekg(0);
    vector<unsigned char> data(size);
    file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

    // Use only the base filename (strip directories).
    string filename = path;
    auto slash = path.rfind('/');
    if (slash != string::npos)
    {
        filename = path.substr(slash + 1);
    }
    auto backslash = filename.rfind('\\');
    if (backslash != string::npos)
        filename = filename.substr(backslash + 1);

    string header = "/sendfile " + filename + " " + to_string(size);
    if (!utils::send_line(socket_fd_, header))
    {
        cout << "Failed to send file header.\n";
        return;
    }
    if (!utils::send_raw_bytes(socket_fd_, data))
    {
        cout << "Failed to send file data.\n";
        return;
    }
    cout << "[you] Sending file: " << filename << " (" << size << " bytes)\n";
}

void ChatClient::receive_wav_file(const std::string &header) // help user to receive file
{
    // Format: FILE <sender_id> <filename> <byte_count>
    istringstream ss(header.substr(5)); // skip "FILE "
    int sender_id = 0;
    string filename;
    size_t file_size = 0;

    if (!(ss >> sender_id >> filename >> file_size))
    {
        cout << "[warn] Malformed FILE header: " << header << "\n";
        return;
    }

    vector<unsigned char> data;
    if (!utils::recv_n_bytes(socket_fd_, file_size, data))
    {
        cout << "[warn] Failed to receive file data from sender " << sender_id << ".\n";
        return;
    }

    string out_path = "received_" + filename;
    ofstream out(out_path, ios::binary);
    if (!out)
    {
        cout << "[warn] Cannot save file: " << out_path << "\n";
        return;
    }
    out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    cout << "[Client " << sender_id << "] sent file '" << filename << "' (" << file_size << " bytes) -> saved as " << out_path << "\n";
}
