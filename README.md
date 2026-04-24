# GroupChat Simplified

This is a simplified C++ Group Chat System for CSC-375 Operating Systems.  
The goal is to keep the project easy to compile, explain, and defend while still showing the required OS ideas.

## Folder Layout

```text
GroupChat/
├── client/
│   ├── main.cpp
│   ├── chat_client.cpp
│   ├── chat_client.h
│   └── audio_client.cpp
├── server/
│   ├── main.cpp
│   ├── chat_server.cpp
│   ├── chat_server.h
│   ├── group_manager.cpp
│   ├── group_manager.h
│   ├── cache.cpp
│   ├── thread_pool.cpp
│   └── thread_pool.h
├── shared/
│   ├── protocol.h
│   ├── cache.h
│   └── utils.h
├── tests/
│   └── bot_test.cpp
├── logs/
│   └── chat_log.txt
├── diagrams/
├── README.md
├── CMakeLists.txt
└── final_report.docx
```

## What Works

- Clients connect to one server over TCP.
- Clients can join or create a group using `/join groupName`.
- Messages are broadcast to everyone in the same group.
- Messages include timestamp, sender ID, and group name.
- Clients can switch groups by running `/join anotherGroup`.
- Clients can list active groups using `/list`.
- Each group has recent message history using a fixed-size circular cache.
- The server has a thread pool and supports:
  - `rr` = Round Robin/FIFO task queue
  - `sjf` = Shortest Job First based on message length
- Optional audio/video is left as a separate simulation stub in `client/audio_client.cpp`.

## Build

```bash
mkdir build
cd build
cmake ..
make
```

## Run

Terminal 1:

```bash
./groupchat_server 5555 rr
```

or:

```bash
./groupchat_server 5555 sjf
```

Terminal 2:

```bash
./groupchat_client 127.0.0.1 5555
```

Terminal 3:

```bash
./groupchat_client 127.0.0.1 5555
```

## Client Commands

```text
/join general
/list
/leave
/quit
```

Any other text is treated as a group message.

## Test Harness

Start the server first, then run:

```bash
./bot_test
```

## OS Concepts to Explain

### Threads and Concurrency

The server accepts multiple clients and creates lightweight reader threads for connected clients. Actual message work is submitted into a fixed thread pool.

### Scheduling

The thread pool simulates scheduling at the application level. The real operating system still schedules the actual CPU threads, but this project chooses which chat task gets handled next.

- Round Robin mode uses FIFO order.
- SJF mode handles shorter messages first.

### Synchronization

Shared group state is protected with `std::mutex`. The thread pool uses `std::condition_variable` so worker threads sleep when no work is available and wake when new tasks arrive.

### Caching

Each group owns a fixed-size circular cache. When the cache is full, the oldest message is evicted. This simulates memory limits and connects to caching/demand paging concepts.

### Logging

Messages are appended to `logs/chat_log.txt` so the system has a simple persistence layer.
