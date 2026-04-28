cmake_minimum_required(VERSION 3.10)

project(GroupChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

include_directories(shared)
include_directories(client)
include_directories(server)

# Server executable
add_executable(groupchat_server
    server/main.cpp
    server/chat_server.cpp
    server/group_manager.cpp
    server/cache.cpp
    server/thread_pool.cpp
)

target_link_libraries(groupchat_server
    Threads::Threads
)

# Client executable
add_executable(groupchat_client
    client/main.cpp
    client/chat_client.cpp
    client/audio_client.cpp
)

target_link_libraries(groupchat_client
    Threads::Threads
)

if (WIN32)
    target_link_libraries(groupchat_client
        winmm
    )
endif()

# Optional bot test executable
add_executable(bot_test
    tests/bot_test.cpp
)

target_link_libraries(bot_test
    Threads::Threads
)