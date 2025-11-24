#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <array>
#include <fstream>
#include <filesystem>
#include <limits>

using namespace std;

class DockerManager {
public:
    void pullImage();
    void runContainerInteractive();
    void listContainers() const;
    void listImages() const;
    void startInteractive();
    void startDetached();
    void deleteImage();
    void stopContainer();
    void removeContainer();
    void execShell();
    void execDetachedCommand();
    void createDockerfile();
    void spinUpMySQL();
    void showContainerIP();

private:
    static void runCommand(const string& cmd);
    vector<pair<string, string>> getContainerList() const;
    string selectContainer(const string& prompt);
    /* NEW helper – retrieves all images */
    vector<pair<string, string>> getImageList() const;
};

// ---------------- Core Utility ----------------

void DockerManager::runCommand(const string& cmd) {
    system(cmd.c_str());
}

vector<pair<string, string>> DockerManager::getImageList() const {
    vector<pair<string, string>> images;
    array<char, 256> buffer{};
    string result;
    FILE* pipe = popen("docker images --format '{{.ID}} {{.Repository}}:{{.Tag}}'", "r");
    if (!pipe) return images;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result = buffer.data();
        stringstream ss(result);
        string id, repoTag;
        ss >> id >> repoTag;
        if (!id.empty() && !repoTag.empty())
            images.emplace_back(id, repoTag);
    }
    pclose(pipe);
    return images;
}

vector<pair<string, string>> DockerManager::getContainerList() const {
    vector<pair<string, string>> containers;
    array<char, 256> buffer{};
    string result;

    FILE* pipe = popen("docker ps -a --format '{{.ID}} {{.Names}}'", "r");
    if (!pipe) return containers;

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result = buffer.data();
        stringstream ss(result);
        string id, name;
        ss >> id >> name;
        if (!id.empty() && !name.empty())
            containers.emplace_back(id, name);
    }
    pclose(pipe);
    return containers;
}

string DockerManager::selectContainer(const string& prompt) {
    auto containers = getContainerList();
    if (containers.empty()) {
        cout << "No containers available.\n";
        return "";
    }

    cout << "\nAvailable Containers:\n";
    int i = 1;
    for (const auto& c : containers)
        cout << i++ << ". " << c.second << " (" << c.first.substr(0, 12) << ")\n";

    int choice;
    cout << prompt << " (1-" << containers.size() << "): ";
    cin >> choice;

    if (choice < 1 || choice > static_cast<int>(containers.size())) {
        cout << "Invalid selection.\n";
        return "";
    }

    return containers[choice - 1].first;
}

// ---------------- Docker Actions ----------------

void DockerManager::pullImage() {
    const vector<string> images = {"debian:stable", "ubuntu:noble", "rockylinux:9.3", "alpine:latest"};
    
    cout << "\nSelect Docker image to pull:\n";
    for (size_t i = 0; i < images.size(); ++i) cout << i + 1 << ". " << images[i] << "\n";
    cout << images.size() + 1 << ". Enter custom image\n";
    cout << "Choose an option (1-" << images.size() + 1 << "): ";
    
    int choice;
    cin >> choice;
    
    string image;
    if (choice >= 1 && choice <= static_cast<int>(images.size())) {
        image = images[choice - 1];
    } else if (choice == static_cast<int>(images.size()) + 1) {
        cout << "Enter custom Docker image name: ";
        cin >> image;
    } else {
        cout << "Invalid selection. Aborting.\n";
        return;
    }
    
    cout << "Pulling image: " << image << "\n";
    runCommand("docker pull " + image);
}

void DockerManager::runContainerInteractive() {
    // Get list of available images
    auto images = getImageList();
    string image;
    
    if (images.empty()) {
        cout << "No Docker images found. Please pull an image first.\n";
        return;
    }
    
    // Display available images
    cout << "\nAvailable Docker Images:\n";
    int idx = 1;
    for (const auto& img : images) {
        cout << idx++ << ". " << img.second << " (" << img.first.substr(0, 12) << ")\n";
    }
    
    // Prompt user to select or enter custom image
    cout << idx << ". Enter custom Docker image name\n";
    cout << "Choose an option (1-" << images.size() << " or " << idx << "): ";
    
    int choice;
    cin >> choice;
    
    if (choice >= 1 && choice <= static_cast<int>(images.size())) {
        // User selected an existing image
        image = images[choice - 1].second;
    } else if (choice == idx) {
        // User wants to enter custom image name
        cout << "Enter custom Docker image name: ";
        cin >> image;
    } else {
        cout << "Invalid selection. Aborting.\n";
        return;
    }
    
    // Continue with port mapping
    int portCount;
    cout << "How many port mappings? ";
    cin >> portCount;
    vector<string> ports;
    for (int i = 0; i < portCount; ++i) {
        string port;
        cout << "Enter mapping #" << (i + 1)
             << " (format host:container, e.g., 8080:80): ";
        cin >> port;
        ports.push_back("-p " + port);
    }
    string portArgs;
    for (const auto& p : ports) portArgs += p + " ";
    cout << "\nPort Forwarding Explanation:\n"
         << "  '-p hostPort:containerPort' exposes the container’s port to the host.\n"
         << "  Example: '-p 8080:80' allows access via http://localhost:8080\n\n";
    runCommand("docker run -it " + portArgs + image + " /bin/sh");
}

void DockerManager::listContainers() const { runCommand("docker ps -a"); }
void DockerManager::listImages() const { runCommand("docker images"); }

void DockerManager::startInteractive() {
    string id = selectContainer("Select container to start interactively");
    if (!id.empty()) runCommand("docker start -ai " + id);
}

void DockerManager::startDetached() {
    string id = selectContainer("Select container to start detached");
    if (!id.empty()) runCommand("docker start " + id);
}

void DockerManager::deleteImage() {
    auto images = getImageList();
    if (images.empty()) {
        cout << "No Docker images found.\n";
        return;
    }

    cout << "\nAvailable Images:\n";
    int idx = 1;
    for (const auto& img : images) {
        cout << idx++ << ". " << img.second << " (" << img.first.substr(0, 12) << ")\n";
    }

    int choice;
    cout << "Select image to delete (1-" << images.size() << "): ";
    cin >> choice;

    if (choice < 1 || choice > static_cast<int>(images.size())) {
        cout << "Invalid selection.\n";
        return;
    }

    const string& id = images[choice - 1].first;
    cout << "Deleting image: " << images[choice - 1].second
         << " (ID: " << id << ") ...\n";
    runCommand("docker rmi " + id);
}

void DockerManager::stopContainer() {
    string id = selectContainer("Select container to stop");
    if (!id.empty()) runCommand("docker stop " + id);
}

void DockerManager::removeContainer() {
    string id = selectContainer("Select container to remove");
    if (!id.empty()) runCommand("docker rm " + id);
}

void DockerManager::execShell() {
    string id = selectContainer("Select running container for shell access");
    if (!id.empty()) runCommand("docker exec -it " + id + " /bin/sh");
}

void DockerManager::execDetachedCommand() {
    string id = selectContainer("Select container to run command in (detached)");
    if (id.empty()) return;

    // Flush any leftover newline before using getline
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string command;
    cout << "Enter command to execute inside the container: ";
    getline(cin, command);

    if (command.empty()) {
        cout << "No command entered. Aborting.\n";
        return;
    }

    string escapedCommand;
    escapedCommand.reserve(command.size() * 2);
    for (char c : command) {
        if (c == '"' || c == '\\')
            escapedCommand += '\\';
        escapedCommand += c;
    }

    cout << "Executing command in detached mode...\n";
    runCommand("docker exec -d " + id + " /bin/sh -c \"" + escapedCommand + "\"");
    cout << "Command dispatched.\n";
}

void DockerManager::spinUpMySQL() {
    string port, password, version;
    cout << "Enter port mapping (e.g., 3306:3306): ";
    cin >> port;
    cout << "Enter MySQL root password: ";
    cin >> password;
    cout << "Enter MySQL version tag (e.g., 8): ";
    cin >> version;

    cout << "\nLaunching MySQL container (accessible via localhost:"
         << port.substr(0, port.find(':')) << ")\n";
    runCommand("docker run -p " + port +
               " --name mysql-container -e MYSQL_ROOT_PASSWORD=" + password +
               " -d mysql:" + version);
}

// ---------------- Parsed IP Feature ----------------

void DockerManager::showContainerIP() {
    string id = selectContainer("Select container to view IP");
    if (id.empty()) return;

    string command = "docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' " + id;

    array<char, 128> buffer{};
    string ip;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        cout << "Failed to inspect container.\n";
        return;
    }

    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        ip = buffer.data();
    }
    pclose(pipe);

    if (ip.empty() || ip == "\n")
        cout << "No IP address found (container may be stopped or not attached to a network).\n";
    else
        cout << "Container IP Address: " << ip;
}

void DockerManager::createDockerfile() {
    string baseImage, bashScriptPath, outputFile, imageName;
    cout << "Enter base Docker image (e.g., ubuntu:22.04): ";
    cin >> baseImage;

    cout << "Enter path to bash script to convert (e.g., setup.sh): ";
    cin >> bashScriptPath;

    if (!filesystem::exists(bashScriptPath)) {
        cout << "Error: Bash script not found.\n";
        return;
    }

    cout << "Enter output Dockerfile name (e.g., Dockerfile or Dockerfile_app): ";
    cin >> outputFile;

    if (outputFile.empty()) outputFile = "Dockerfile";

    ifstream scriptFile(bashScriptPath);
    ofstream dockerfile(outputFile);

    if (!scriptFile.is_open() || !dockerfile.is_open()) {
        cout << "Error: Unable to open file(s).\n";
        return;
    }

    // Write Dockerfile header
    dockerfile << "FROM " << baseImage << "\n";
    dockerfile << "WORKDIR /app\n\n";
    dockerfile << "# Auto-generated by Tux-Dock\n";

    string line;
    while (getline(scriptFile, line)) {
        if (line.empty()) continue;

        // Skip comments or shebang
        if (line.rfind("#", 0) == 0 || line.rfind("#!", 0) == 0)
            continue;

        dockerfile << "RUN " << line << "\n";
    }

    dockerfile << "\nCMD [\"/bin/bash\"]\n";
    dockerfile.close();
    scriptFile.close();

    cout << "Dockerfile created successfully: " << outputFile << "\n";
    cout << "Enter image name to build from this Dockerfile (e.g., myimage): ";
    cin >> imageName;

    if (imageName.empty()) {
        cout << "No image name provided. Skipping build.\n";
        return;
    }

    cout << "Building Docker image '" << imageName << "'...\n";
    runCommand("docker build -t " + imageName + " -f " + outputFile + " .");
    cout << "Docker build command executed.\n";
}

// ---------------- Menu ----------------

int main() {
    DockerManager docker;
    int option;

    while (true) {
        cout << "\nTux-Dock: Docker Management Menu\n"
             << "----------------------------------\n"
             << "1.  Pull Docker Image\n"
             << "2.  Run/Create Interactive Container\n"
             << "3.  List All Containers\n"
             << "4.  List All Images\n"
             << "5.  Start Container Interactively (boot new session)\n"
             << "6.  Start Detached Container Session\n"
             << "7.  Delete Docker Image\n"
             << "8.  Stop Container\n"
             << "9.  Remove Container\n"
             << "10. Attach Shell to Running Container\n"
             << "11. Run Detached Command in Container\n"
             << "12. Spin Up MySQL Container\n"
             << "13. Get Container IP Address\n"
             << "14. Create Dockerfile & Build Image from Bash Script\n"
             << "15. Exit\n"
             << "----------------------------------\n"
             << "Choose an option: ";

        cin >> option;

        switch (option) {
            case 1: docker.pullImage(); break;
            case 2: docker.runContainerInteractive(); break;
            case 3: docker.listContainers(); break;
            case 4: docker.listImages(); break;
            case 5: docker.startInteractive(); break;
            case 6: docker.startDetached(); break;
            case 7: docker.deleteImage(); break;
            case 8: docker.stopContainer(); break;
            case 9: docker.removeContainer(); break;
            case 10: docker.execShell(); break;
            case 11: docker.execDetachedCommand(); break;
            case 12: docker.spinUpMySQL(); break;
            case 13: docker.showContainerIP(); break;
            case 14: docker.createDockerfile(); break;
            case 15:
                cout << "Exiting Tux-Dock.\n";
                return 0;
            default:
                cout << "Invalid option.\n";
        }
    }
}

