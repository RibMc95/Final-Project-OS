# GroupChat

A C++ group chat system for CSC-375 Operating Systems demonstrating real concurrency,
thread-pool scheduling (RR and SJF), a bounded circular cache, and a binary packet
protocol — with optional audio/video file transfer.

**Design document:** [GroupChat/DESIGN_DOCUMENT.md](GroupChat/DESIGN_DOCUMENT.md) · [design_document.pdf](GroupChat/design_document.pdf)

---

## Folder Layout

```text
GroupChat/
├── client/
│   ├── main.cpp
│   ├── chat_client.cpp / .h
│   ├── audio_client.cpp / .h
│   └── video_client.cpp / .h
├── server/
│   ├── main.cpp
│   ├── chat_server.cpp / .h
│   ├── group_manager.cpp / .h
│   ├── cache.cpp
│   ├── thread_pool.cpp / .h
│   └── (cache.cpp mirrors shared/cache.h)
├── shared/
│   ├── protocol.h
│   ├── cache.h
│   └── utils.h
├── tests/
│   └── bot_test.cpp
├── logs/
│   └── chat_log.txt
├── sample/           ← put test audio/video files here
├── downloads/        ← received media files saved here
├── diagrams/
├── CMakeLists.txt
├── DESIGN_DOCUMENT.md
└── design_document.pdf
```

---

## What Works

- Clients connect to one server over TCP.
- `/join <group>` — join or create a group; joining replays recent message history.
- `/list` — list all active groups.
- `/leave` — leave the current group.
- `/quit` — disconnect.
- Messages are broadcast to all members of the same group with timestamp and sender ID.
- Each group keeps a **circular cache of 20 messages** replayed to new joiners.
- All messages are appended to `logs/chat_log.txt`.
- Server thread pool supports two scheduling modes:
  - `rr` — Round Robin / FIFO (default)
  - `sjf` — Shortest Job First (keyed on message length)
- `/audio <path>` — stream an audio file to the group.
- `/video <path>` — stream a video file to the group (auto-converted to H.264 MP4 via ffmpeg on Linux/WSL).
- `/play [path]` — open the last received media (or a specific file) with the system default player.

---

## Dependencies

| Tool | Purpose |
|------|---------|
| CMake ≥ 3.10 | build system |
| g++ with C++17 | compiler |
| ffmpeg *(optional)* | video re-encode for Windows compatibility |

Install ffmpeg on WSL/Linux:

```bash
sudo apt install -y ffmpeg
```

---

## Build

```bash
cd GroupChat
cmake -S . -B build-local
cmake --build build-local --target groupchat_server groupchat_client bot_test
```

---

## Run

**Terminal 1 — start the server:**

```bash
./build-local/groupchat_server        # default: port 5555, RR mode
# or specify port and mode explicitly:
./build-local/groupchat_server 5555 sjf
```

**Terminal 2 — client A:**

```bash
./build-local/groupchat_client        # default: 127.0.0.1:5555
```

**Terminal 3 — client B:**

```bash
./build-local/groupchat_client
```

---

## Client Commands

```text
/join <group>          join or create a group
/list                  list active groups
/leave                 leave current group
/audio <file_path>     send an audio file  (e.g. /audio sample/test.mp3)
/video <file_path>     send a video file   (e.g. /video sample/test.mp4)
/play                  open last received media
/play <file_path>      open a specific local file
/quit                  disconnect
```

Any other text is sent as a chat message to the current group.

> **Note:** you must `/join` a group before sending audio or video.

---

## Media Notes

- Received files are saved to `downloads/received_<filename>`.
- `/play` uses Windows Media Player when running under WSL, or `xdg-open` on native Linux.
- If a video shows an unsupported-encoding error, convert it once with ffmpeg:

  ```bash
  ffmpeg -i sample/file.mp4 -c:v libx264 -pix_fmt yuv420p -c:a aac -b:a 128k sample/file_compat.mp4
  ```

---

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
