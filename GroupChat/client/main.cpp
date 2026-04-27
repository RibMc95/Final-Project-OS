#include "chat_client.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) 
{
    using namespace std;
    string host = "127.0.0.1";
    int port = 5555;

    if (argc >= 2) 
    {
        host = argv[1];
    }
    if (argc >= 3) 
    {
        port = stoi(argv[2]);
    }

    ChatClient client(host, port);
    if (!client.connect_to_server()) 
    {
        return 1;
    }

    client.run();
    return 0;
}
