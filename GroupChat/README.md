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
│   ├── audio_client.cpp
│   └── video_client.cpp
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
└── CMakeLists.txt
```

## What Works

- Clients connect to one server over TCP.
- Clients can join or create a group using `/join groupName`.
- Messages are broadcast to everyone in the same group.
- Messages include timestamp, sender ID, and group name.

# GroupChat Simplified

This is a simplified C++ Group Chat System for CSC-375 Operating Systems.
The goal is to keep the project easy to compile, explain, and defend while still showing the required OS ideas.

Design document for submission: see [DESIGN_DOCUMENT.md](DESIGN_DOCUMENT.md).

## Folder Layout

```text
GroupChat/
├── client/
│   ├── main.cpp
│   ├── chat_client.cpp
│   ├── chat_client.h
│   ├── audio_client.cpp
│   └── video_client.cpp
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
├── DESIGN_DOCUMENT.md
└── CMakeLists.txt
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
- Clients can send audio files with `/audio <file_path>`.
- Clients can send video files with `/video <file_path>`.
- Clients can open the last sent/received media with `/play` or open a specific local media file with `/play <file_path>`.

## Build

```bash
cmake -S . -B build-local
cmake --build build-local --target groupchat_server groupchat_client bot_test
```

## Run

Terminal 1:

```bash
./build-local/groupchat_server
```

Terminal 2:

```bash
./build-local/groupchat_client
```

Terminal 3:

```bash
./build-local/groupchat_client
```

## Quick Start

```bash
/join general
```

Join a group before sending audio or video. If you do not join first, the server will reject the media transfer.

## Media Commands

```text
/audio sample/test.mp3
/video sample/test4.mp4
/play
/play downloads/received_test.mp3
/play downloads/received_test4_compat.mp4
```

These sample paths assume you launched the client from the `GroupChat` folder.

## Playback Behavior

- On Windows, `/play` opens the file with the default Windows app.
- On WSL, `/play` opens the file with the Windows default app.
- On native Linux, `/play` uses `xdg-open` to open the default Linux app.
- In headless environments such as some Codespaces setups, the media file may save correctly but not open directly.

## Video Compatibility (Linux/WSL)

On Linux/WSL, `/video` converts outgoing video to a Windows Media Player compatible MP4 before sending.

Install `ffmpeg` once:

```bash
sudo apt install -y ffmpeg
```

If `Isreal.mp4` is the only file that still shows an unsupported encoding error in Windows Media Player, convert it once manually:

```bash
ffmpeg -i sample/Isreal.mp4 -c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 128k sample/Isreal_compat.mp4
```

Then send:

```text
/video sample/Isreal_compat.mp4
```

## Client Commands

```text
/join general
/list
/leave
/audio sample/test.mp3
/video sample/test4.mp4
/play
/play downloads/received_test.mp3
/play downloads/received_test4_compat.mp4
/quit
```

Any other text is treated as a group message.

## Test Harness

Start the server first, then run:

```bash
./build-local/bot_test
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

Each group owns a fixed-size circular cache. When the cache is full, the oldest message is evicted. This simulates memory limits and connects to caching and demand paging concepts.

### Logging

Messages are appended to `logs/chat_log.txt` so the system has a simple persistence layer.
