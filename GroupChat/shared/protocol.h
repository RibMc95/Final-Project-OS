#pragma once

#include <cstdint>
#include <string>

namespace protocol {

// Simple text protocol used to keep the project readable.
// Client -> Server:
//   JOIN <group>
//   LIST
//   MSG <message text>
//   QUIT
//
// Server -> Client:
//   INFO <text>
//   HISTORY <formatted old message>
//   MSG <formatted live message>
//   GROUPS <comma-separated group list>
//   ERROR <text>

constexpr int DEFAULT_PORT = 5555;
constexpr std::size_t MAX_LINE = 1024;
constexpr std::size_t HISTORY_LIMIT = 20;

enum class ClientCommand {
    Join,
    List,
    Message,
    Quit,
    Unknown
};

struct ParsedCommand {
    ClientCommand type{ClientCommand::Unknown};
    std::string payload;
};

} // namespace protocol
