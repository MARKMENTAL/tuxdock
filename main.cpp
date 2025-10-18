#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <array>

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
    void spinUpMySQL();
    void showContainerIP();

private:
    static void runCommand(const string& cmd);
    vector<pair<string, string>> getContainerList() const;
    string selectContainer(const string& prompt);
};

// ---------------- Core Utility ----------------

void DockerManager::runCommand(const string& cmd) {
    system(cmd.c_str());
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
    string image;
    cout << "Enter Docker image to pull (e.g., alpine): ";
    cin >> image;
    runCommand("docker pull " + image);
}

void DockerManager::runContainerInteractive() {
    string image;
    cout << "Enter Docker image to run interactively (e.g., alpine): ";
    cin >> image;

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
    string image;
    cout << "Enter image name or ID to delete: ";
    cin >> image;
    runCommand("docker rmi " + image);
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
             << "11. Spin Up MySQL Container\n"
             << "12. Get Container IP Address\n"
             << "13. Exit\n"
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
            case 11: docker.spinUpMySQL(); break;
            case 12: docker.showContainerIP(); break;
            case 13:
                cout << "Exiting Tux-Dock.\n";
                return 0;
            default:
                cout << "Invalid option.\n";
        }
    }
}

