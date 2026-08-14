# Tux-Dock
### A lightweight C++ Docker TUI

Tux-Dock is a modern **C++17** Docker terminal frontend built with **FTXUI**.
It gives you a guided, keyboard-first TUI for common Docker operations like pulling images, running containers, inspecting IPs, and managing images/containers without memorizing long CLI flags.

---

## Features

- Interactive Docker workflows through a single-screen TUI with modal steps.
- Picker-based selection (arrow keys + Enter) for containers/images instead of numeric menus.
- High-level status panel with responsive wait states for slower operations.
- Rich container display with state and forwarded ports.
- Interactive shell handoff with clean terminal clear before/after shell transitions.
- Image operations: pull/list/delete with curated quick picks and custom image support.
- Script-to-image workflow: generate Dockerfile from bash script, then optionally build.
- MySQL quick start flow with version/password/port prompts.
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

# Configure & build (FTXUI is fetched automatically)
cmake -S . -B build
cmake --build build -j

# Run it (requires Docker permissions)
sudo ./build/tux-dock
```

Prefer a prebuilt binary? CI artifacts are published at:
https://mentalnet.xyz/forgejo/markmental/tuxdock/actions

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
12. Spin Up MySQL Container
13. Create Dockerfile & Build Image from Bash Script
14. About Tux-Dock
15. Exit

---

## Design Overview

Tux-Dock is now structured around **two main classes**:

```cpp
class DockerManager {
public:
  struct ContainerInfo {
    std::string id;
    std::string name;
    std::string status;
    std::string ports;
    bool running;
  };

  // Docker command/data layer
  std::vector<ContainerInfo> getContainerList() const;
  std::vector<std::pair<std::string, std::string>> getImageList() const;
  bool pullImage(...);
  bool runContainerInteractive(...);
  bool startInteractive(...);
  bool startDetached(...);
  bool stopContainer(...);
  bool removeContainer(...);
  bool deleteImage(...);
  bool execShell(...);
  bool execDetachedCommand(...);
  bool spinUpMySQL(...);
  bool createDockerfile(...);
};

class TuxDockApp {
public:
  void Run();

private:
  // TUI orchestration layer
  void OpenInput(...);
  void OpenSelect(...);
  void OpenConfirm(...);
  void RunDeferredStatusAction(...);
  void RunWithRestoredIO(...);
  void ExecuteSelectedAction();
  // action handlers bridge UI -> DockerManager
};
```

### Responsibilities

- `DockerManager`
  - Executes Docker commands.
  - Escapes shell arguments and returns success/failure + user-facing messages.
  - Collects container/image data used by the UI.

- `TuxDockApp`
  - Renders the FTXUI interface.
  - Manages modal flows (input/select/confirm/message).
  - Handles status updates, deferred actions, and interactive shell transitions.
  - Coordinates end-to-end user flows by calling `DockerManager` methods.

This split keeps Docker behavior isolated while making UI behavior easier to extend.

---

## About / Version

- Version: `022526-dev`
- Created by: `markmental`
- GitHub: https://github.com/MARKMENTAL/tuxdock
- Forgejo: https://mentalnet.xyz/forgejo/markmental/tuxdock

---

## License

MIT License — free to use, modify, and share.
