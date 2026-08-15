# Tux-Dock
### A lightweight C++ Docker TUI

Tux-Dock is a modern **C++17** Docker terminal frontend built with **FTXUI**.
It gives you a guided, keyboard-first TUI for common Docker operations without memorizing long CLI flags.

---

## Features

- Interactive Docker workflows through a single-screen TUI with modal steps.
- Picker-based selection (arrow keys + Enter) for containers/images instead of numeric menus.
- Busy-operation modals with a spinner and input blocking while Docker work completes.
- Rich container display with state and forwarded ports.
- Interactive shell handoff with clean terminal clear before/after shell transitions.
- Image operations: pull/list/delete with curated quick picks and custom image support.
- Docker Engine API access through `/var/run/docker.sock` for structured list and lifecycle operations.
- Direct `fork`/`exec` process execution for CLI-backed streaming and interactive commands.
- Persistent container listings that retain exited containers.
- Robust stop handling with state polling, timeout retry, and idempotent stop responses.
- About screen in-app with project/version/repository info.

---

## Build Requirements

- **C++17 or newer** compiler (e.g. `g++`, `clang++`)
- **CMake 3.16+**
- **Docker Engine** installed and running

---

## Build & Run

```bash
# Clone the repo
git clone https://mentalnet.xyz/forgejo/markmental/tuxdock.git
cd tuxdock

# Configure, build, and test (FTXUI and nlohmann/json are fetched automatically)
./compile.sh

# Run it (requires Docker permissions)
sudo ./build/tux-dock

# Build without running tests
./compile.sh --no-test

# Run tests and serve an HTML 3.2 report on port 8095
./compile.sh --web-test-view

# Override the report server port
./compile.sh --web-test-view 9000
```

The web report is generated under `/tmp` and includes exact CTest output, test summaries, Docker integration output, and a text rendition of the TUI flow. It requires `nc`, `netcat`, or Nmap's `ncat`; press `Ctrl-C` to stop the server. The default port is `8095`; pass a port after `--web-test-view` to override it. `--web-test-view --no-test` is invalid. Normal test runs include a Docker integration test using `debian:forky`, so Docker must be available. On systems with 1 GB or less of available memory, the script automatically uses a smaller build configuration and a single build job.

---

## Menu Overview

Current TUI actions:

1. Pull Docker Image
2. Run/Create Interactive Container
3. List All Containers
4. List All Images
5. Start Container Interactively (boot new session)
6. Start Detached Container Session
7. Delete Docker Image
8. Stop Container
9. Remove Container
10. Attach Shell to Running Container
11. Run Detached Command in Container
12. About Tux-Dock
13. Exit

---

## Design Overview

Tux-Dock is organized around the TUI, Docker manager, Engine API client, and direct process runner:

### Responsibilities

- `DockerEngineClient`
  - Talks directly to Docker over the Unix socket.
  - Parses HTTP responses, including chunked and bodyless responses.

- `ProcessRunner`
  - Executes direct argument vectors using `fork` and `exec`.
  - Supports captured output and inherited terminal I/O.

- `DockerManager`
  - Maps Engine API JSON into application data.
  - Performs lifecycle operations and robust stop-state confirmation.
  - Preserves cached state when refreshes fail.

- `TuxDockApp`
  - Renders the FTXUI interface.
  - Manages modal flows (input/select/confirm/message).
  - Handles modal flows, busy operations, blocked input, and interactive shell transitions.
  - Coordinates end-to-end user flows by calling `DockerManager` methods.

This split keeps Docker behavior isolated while making UI behavior easier to extend.

---

## Testing

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`compile.sh` runs these tests by default. Pass `--no-test` to skip them.

---

## About / Version

- Version: `0.1-beta`
- Created by: `markmental`
- GitHub: https://github.com/MARKMENTAL/tuxdock
- Forgejo: https://mentalnet.xyz/forgejo-v2/markmental/tuxdock

---

## License

MIT License — free to use, modify, and share.
