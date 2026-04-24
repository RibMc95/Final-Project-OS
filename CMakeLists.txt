cmake_minimum_required(VERSION 3.10)
project(GroupChatSimplified)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(groupchat_server
    server/main.cpp
    server/chat_server.cpp
    server/group_manager.cpp
    server/thread_pool.cpp
)

add_executable(groupchat_client
    client/main.cpp
    client/chat_client.cpp
    client/audio_client.cpp
)

add_executable(bot_test
    tests/bot_test.cpp
)

target_include_directories(groupchat_server PRIVATE shared server)
target_include_directories(groupchat_client PRIVATE shared client)
target_include_directories(bot_test PRIVATE shared)

find_package(Threads REQUIRED)
target_link_libraries(groupchat_server PRIVATE Threads::Threads)
target_link_libraries(groupchat_client PRIVATE Threads::Threads)
target_link_libraries(bot_test PRIVATE Threads::Threads)
