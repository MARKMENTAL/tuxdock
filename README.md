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
- `tuxreaperd` micro init system: a tiny statically-linked C daemon mounted into created containers as PID 1, acting as a subreaper so orphaned processes never linger as zombies, broadcasting lifecycle signals (SIGTERM, SIGQUIT, SIGINT, SIGHUP, SIGUSR1, SIGUSR2) to the whole process tree via `kill(-1, sig)`, and propagating exit status. [Processes don't fear tuxreaperd!](tuxreaperdpromo.jpg)
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

# Force the libc-based tuxreaperd implementation
./compile.sh --force-libc-reaper
```

`compile.sh` builds the `tuxreaperd` micro init before the main project. On supported hosts (`x86_64`/`amd64`/`aarch64`/`arm64`) it first tries the freestanding syscall implementation in `tuxreaperdasm.c`; on other architectures, or if the freestanding build fails, it falls back to the standard POSIX implementation in `tuxreaperdgnu.c`. Pass `--force-libc-reaper` to skip the freestanding build and use the libc implementation directly.

The web report is generated under `/tmp` and includes exact CTest output, test summaries, Docker integration output, a text rendition of the TUI flow, and the tuxreaperd zombie-reaping results. It requires `nc`, `netcat`, or Nmap's `ncat`; press `Ctrl-C` to stop the server. The default port is `8095`; pass a port after `--web-test-view` to override it. `--web-test-view --no-test` is invalid. Normal test runs include a Docker integration test using `debian:forky`, so Docker must be available. On systems with 1 GB or less of available memory, the script automatically uses a smaller build configuration and a single build job.

---

## Menu Overview

Current TUI actions:

1. Pull Docker Image
2. Create Container
3. List All Containers
4. List All Images
5. Start Detached Container Session
6. Delete Docker Image
7. Stop Container
8. Remove Container
9. Attach Shell to Running Container
10. Run Detached Command in Container
11. About Tux-Dock
12. Exit

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

`compile.sh` runs these tests by default. Pass `--no-test` to skip them. The suite includes `tuxreaperd-zombie-tests`, which boots a container with `tuxreaperd` as PID 1 and verifies orphaned processes are reaped (no zombies), exit status propagates, and SIGTERM is forwarded on `docker stop`. Docker must be available.

---

## About / Version

- Version: `0.3-beta`
- Created by: `markmental`
- GitHub: https://github.com/MARKMENTAL/tuxdock
- Forgejo: https://mentalnet.xyz/forgejo-v2/markmental/tuxdock

---

## License

MIT License — free to use, modify, and share.
