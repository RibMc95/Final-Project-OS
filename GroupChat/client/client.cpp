#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
using namespace std;


int main() 
{

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        cout << "Socket creation error" << endl;
        return -1;
    }


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);


    // Convert IPv4 and IPv6 addresses from text to binary form

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) 
    {
        cout << "Invalid address/ Address not supported" << endl;
        return -1;
    }


    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        cout << "Connection failed" << endl;
        return -1;
    }


    string message;
    cout << "Enter message: ";
    getline(cin, message);
    send(sock, message.c_str(), message.length(), 0);
    read(sock, buffer, 1024);
    cout << "Server response: " << buffer << endl;
    close(sock);
    return 0;

}
