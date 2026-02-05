# Lurk — Multiplayer Game Server (C++)

A **Linux**-oriented, multi-threaded game server written in **C++** with a custom binary protocol over TCP. Built for a text-based multiplayer game with secure protocol handling and concurrent client support.

## Relevance to embedded systems & networking

- **C/C++**: Core server implemented in C++ with POSIX sockets (`sys/socket.h`, `netinet/in.h`).
- **Networking**: TCP server with strict **protocol compliance** — validates all incoming messages and rejects malformed or unauthorized packets to prevent network-based attacks.
- **Linux**: Uses POSIX APIs; developed and run on Linux. Includes a **Bash** script for process management and auto-restart.
- **Concurrency**: Multi-threaded design with **mutexes** for shared state (players, rooms, game entities); safe handling of concurrent connections.
- **Source control**: Project maintained with **Git**; suitable as a portfolio piece for roles involving networking and secure software.

## Features

- Binary protocol with typed messages (characters, rooms, errors, accept/reject).
- Thread-safe player and room state with fine-grained locking.
- Error codes and structured error messages for protocol violations.
- Bash helper script for keeping the server running (e.g. `keep_server_alive.sh`).

## Build & run (Linux)

```bash
# Compile (GCC/Clang on Linux or macOS)
g++ -std=c++17 -pthread -o lurk_ok lurk_ok.cpp

# Run on port 5005
./lurk_ok 5005
```

Optional: use `./keep_server_alive.sh` to run with auto-restart (Bash).

## Project structure

| File                   | Description                             |
|------------------------|-----------------------------------------|
| `lurk_ok.cpp`          | Main server: sockets, protocol, threads |
| `keep_server_alive.sh` | Bash script to run and restart server   |

## Tech stack

- **Language**: C++
- **APIs**: POSIX sockets, pthreads, mutex
- **Environment**: Linux
- **Scripting**: Bash
