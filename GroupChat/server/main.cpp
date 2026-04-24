#include "chat_server.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = 5555;
    ScheduleMode mode = ScheduleMode::RoundRobin;

    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }

    if (argc >= 3) {
        std::string sched = argv[2];
        if (sched == "sjf") {
            mode = ScheduleMode::ShortestJobFirst;
        } else if (sched == "rr") {
            mode = ScheduleMode::RoundRobin;
        } else {
            std::cerr << "Unknown scheduler. Use rr or sjf.\n";
            return 1;
        }
    }

    std::cout << "Scheduler: "
              << (mode == ScheduleMode::ShortestJobFirst ? "SJF" : "Round Robin")
              << "\n";

    ChatServer server(port, mode, 4);
    return server.start() ? 0 : 1;
}
