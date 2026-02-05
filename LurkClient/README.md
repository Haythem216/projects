# Lurk — Game Client (Python)

Graphical client for the Lurk multiplayer game. Connects to the **C++ server** over TCP and parses the binary protocol. Demonstrates **Python** for application-level logic and **socket** programming.

## Relevance to software engineering roles

- **Python**: Full client implementation in Python (sockets, struct unpacking, state machine).
- **Networking**: TCP client; sends and receives binary protocol messages consistent with the server.
- **Practical experience**: Builds a complete client–server application (pair with [LurkServer](../LurkServer)).
- **Revision control**: Developed with **Git**; good candidate for portfolio or interview discussion.

## Features

- PyQt5-based UI for gameplay and connection.
- Binary protocol parsing (message types, flags, character/room data).
- Connection handling and basic error feedback.

## Setup & run

```bash
# Recommended: use a virtual environment
python3 -m venv venv
source venv/bin/activate   # Linux/macOS

pip install PyQt5

python client_ok.py
```

Configure the server host/port in the client to match where `LurkServer` is running.

## Tech stack

- **Language**: Python 3
- **UI**: PyQt5
- **Networking**: `socket`, `struct` for binary protocol
