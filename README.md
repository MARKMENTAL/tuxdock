# 🐧 Tux-Dock
### A lightweight C++ Docker management CLI

Tux-Dock is a simple, modern, and fast **C++17** command-line utility for managing Docker containers and images.
It offers a clean, interactive menu for common Docker operations like pulling images, running containers, and inspecting IP addresses — without the overhead of complex GUIs or scripts.

---

## ✨ Features

- 🔹 **Interactive container management** — start, stop, remove, or attach to containers with simple numbered menus.
- 🔹 **Port mapping made clear** — automatically prompts for and explains host ↔ container port bindings.
- 🔹 **Image operations** — pull, list, and delete Docker images.
- 🔹 **Script-to-image workflow** — turn a bash setup script into a Dockerfile and build the resulting image in one go.
- 🔹 **Quick MySQL setup** — spin up a MySQL container with version, password, and port configuration in seconds.
- 🔹 **Get container IP address** — cleanly retrieves and displays only the container’s assigned IP.
- 🔹 **Run detached commands** — execute background jobs inside a container without attaching an interactive shell.
- 🔹 **Modern C++ design** — built with classes, minimal dependencies, and clear abstractions.

---

## 🧩 Build Requirements

- **C++17 or newer** compiler
  (e.g., `g++`, `clang++`)
- **Docker Engine** installed and running
  (tested on Debian 12/13, Alpine Linux, and Arch Linux)

---

## ⚙️ Build & Run

```bash
# Clone the repo
git clone https://mentalnet.xyz/gitea/markmental/tuxdock.git
cd tuxdock

# Build the binary
g++ -std=c++17 tux-dock.cpp -o tux-dock

# Run it (requires Docker permissions)
sudo ./tux-dock
````

Prefer a prebuilt binary? The CI runner publishes build artifacts for the latest commits at:
https://mentalnet.xyz/forgejo/markmental/tuxdock/actions

---

## 🖥️ Menu Overview

```
Tux-Dock: Docker Management Menu
----------------------------------
1.  Pull Docker Image
2.  Run/Create Interactive Container
3.  List All Containers
4.  List All Images
5.  Start Container Interactively (boot new session)
6.  Start Detached Container Session
7.  Delete Docker Image
8.  Stop Container
9.  Remove Container
10. Attach Shell to Running Container
11. Run Detached Command in Container
12. Spin Up MySQL Container
13. Get Container IP Address
14. Create Dockerfile & Build Image from Bash Script
15. Exit
```

Each action guides you through the required steps.
For container-related commands, Tux-Dock automatically lists available containers and lets you choose by **number**, not by typing long IDs.

---

## 📡 Example: Getting a Container’s IP Address

```
Available Containers:
1. mysql-container (ebaf5dbae393)
2. webapp (fa29b6c1f1e8)

Select container to view IP (1-2): 2
Container IP Address: 172.17.0.3
```

---

## 🧱 Design Overview

Tux-Dock is built using a single class:

```cpp
class DockerManager {
    void pullImage();
    void runContainerInteractive();
    void listContainers() const;
    void startInteractive();
    void execDetachedCommand();
    void stopContainer();
    void showContainerIP();
};
```

This makes the codebase **extensible** — adding new Docker features like `docker logs` or `docker stats` requires only a small new method.

---

## 🧠 Why C++?

C++ was one of my formative languages when I was learning to program — it’s where I first grasped core concepts like memory management, data structures, OOP and abstraction.
Writing Tux-Dock in C++ is both nostalgic and practical: it combines the clarity of modern design with the raw performance and control that first inspired me to code.

---

## 📜 License

MIT License — free to use, modify, and share.

---

### 💡 Tip

If you’d like Tux-Dock to run without `sudo`, add your user to the Docker group:

```bash
sudo usermod -aG docker $USER
```

Then log out and back in.

---

**Author:** MARKMENTAL
**Project:** Tux-Dock — *A clean and modern CLI for Docker power users.*


---
