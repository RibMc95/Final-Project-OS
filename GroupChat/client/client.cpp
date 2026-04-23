#include <iostream>

#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>

#include <string.h>


int main() {

    int sock = 0;

    struct sockaddr_in serv_addr;

    char buffer[1024] = {0};


    // Create socket

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

        std::cout << "Socket creation error" << std::endl;

        return -1;

    }


    serv_addr.sin_family = AF_INET;

    serv_addr.sin_port = htons(8080);


    // Convert IPv4 and IPv6 addresses from text to binary form

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {

        std::cout << "Invalid address/ Address not supported" << std::endl;

        return -1;

    }


    // Connect to server

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {

        std::cout << "Connection failed" << std::endl;

        return -1;

    }


    std::string message;

    std::cout << "Enter message: ";

    std::getline(std::cin, message);

    send(sock, message.c_str(), message.length(), 0);

    read(sock, buffer, 1024);

    std::cout << "Server response: " << buffer << std::endl;


    close(sock);

    return 0;

}
