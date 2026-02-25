#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

class DockerManager {
public:
    struct ContainerInfo {
        std::string id;
        std::string name;
        std::string status;
        std::string ports;
        bool running = false;
    };

    std::vector<ContainerInfo> getContainerList() const;
    std::vector<std::pair<std::string, std::string>> getImageList() const;

    bool pullImage(const std::string& image, std::string& message) const;
    bool runContainerInteractive(const std::string& image,
                                 const std::vector<std::string>& ports,
                                 std::string& message) const;
    bool startInteractive(const std::string& containerId, std::string& message) const;
    bool startDetached(const std::string& containerId, std::string& message) const;
    bool deleteImage(const std::string& imageId, std::string& message) const;
    bool stopContainer(const std::string& containerId, std::string& message) const;
    bool removeContainer(const std::string& containerId, std::string& message) const;
    bool execShell(const std::string& containerId, std::string& message) const;
    bool execDetachedCommand(const std::string& containerId,
                             const std::string& command,
                             std::string& message) const;
    bool spinUpMySQL(const std::string& port,
                     const std::string& password,
                     const std::string& version,
                     std::string& message) const;
    bool showContainerIP(const std::string& containerId, std::string& message) const;
    bool createDockerfile(const std::string& baseImage,
                          const std::string& bashScriptPath,
                          std::string outputFile,
                          const std::string& imageName,
                          std::string& message) const;

private:
    static bool runCommand(const std::string& cmd, bool quiet = true);
    static std::string shellEscape(const std::string& value);
};

bool DockerManager::runCommand(const std::string& cmd, bool quiet) {
    std::string command = cmd;
    if (quiet) {
        command += " > /dev/null 2>&1";
    }
    const int code = std::system(command.c_str());
    return code == 0;
}

std::string DockerManager::shellEscape(const std::string& value) {
    std::string escaped = "'";
    for (char c : value) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

std::vector<std::pair<std::string, std::string>> DockerManager::getImageList() const {
    std::vector<std::pair<std::string, std::string>> images;
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = popen("docker images --format '{{.ID}} {{.Repository}}:{{.Tag}}'", "r");
    if (!pipe) {
        return images;
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result = buffer.data();
        std::stringstream ss(result);
        std::string id;
        std::string repoTag;
        ss >> id >> repoTag;
        if (!id.empty() && !repoTag.empty()) {
            images.emplace_back(id, repoTag);
        }
    }
    pclose(pipe);
    return images;
}

std::vector<DockerManager::ContainerInfo> DockerManager::getContainerList() const {
    std::vector<ContainerInfo> containers;
    std::array<char, 256> buffer{};
    std::string result;

    FILE* pipe = popen("docker ps -a --format '{{.ID}}\t{{.Names}}\t{{.Status}}\t{{.Ports}}'", "r");
    if (!pipe) {
        return containers;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result = buffer.data();
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        std::stringstream ss(result);
        ContainerInfo info;
        std::getline(ss, info.id, '\t');
        std::getline(ss, info.name, '\t');
        std::getline(ss, info.status, '\t');
        std::getline(ss, info.ports);

        if (!info.id.empty() && !info.name.empty()) {
            info.running = info.status.rfind("Up", 0) == 0;
            containers.push_back(info);
        }
    }
    pclose(pipe);
    return containers;
}

bool DockerManager::pullImage(const std::string& image, std::string& message) const {
    if (image.empty()) {
        message = "Please provide an image name.";
        return false;
    }
    const bool ok = runCommand("docker pull " + shellEscape(image));
    message = ok ? "Image pulled successfully." : "Could not pull that image.";
    return ok;
}

bool DockerManager::runContainerInteractive(const std::string& image,
                                            const std::vector<std::string>& ports,
                                            std::string& message) const {
    if (image.empty()) {
        message = "Please choose an image first.";
        return false;
    }

    std::string cmd = "docker run -it ";
    for (const auto& port : ports) {
        cmd += "-p " + shellEscape(port) + " ";
    }
    cmd += shellEscape(image) + " /bin/sh";

    const bool ok = runCommand(cmd, false);
    message = ok ? "Interactive container session finished." : "Could not start that container session.";
    return ok;
}

bool DockerManager::startInteractive(const std::string& containerId, std::string& message) const {
    const bool ok = runCommand("docker start -ai " + shellEscape(containerId), false);
    message = ok ? "Interactive container session finished." : "Could not start that container interactively.";
    return ok;
}

bool DockerManager::startDetached(const std::string& containerId, std::string& message) const {
    const bool ok = runCommand("docker start " + shellEscape(containerId));
    message = ok ? "Container started in detached mode." : "Could not start that container.";
    return ok;
}

bool DockerManager::deleteImage(const std::string& imageId, std::string& message) const {
    const bool ok = runCommand("docker rmi " + shellEscape(imageId));
    message = ok ? "Image deleted." : "Could not delete that image. It may still be in use.";
    return ok;
}

bool DockerManager::stopContainer(const std::string& containerId, std::string& message) const {
    const bool ok = runCommand("docker stop " + shellEscape(containerId));
    message = ok ? "Container stopped." : "Could not stop that container.";
    return ok;
}

bool DockerManager::removeContainer(const std::string& containerId, std::string& message) const {
    const bool ok = runCommand("docker rm " + shellEscape(containerId));
    message = ok ? "Container removed." : "Could not remove that container.";
    return ok;
}

bool DockerManager::execShell(const std::string& containerId, std::string& message) const {
    const bool ok = runCommand("docker exec -it " + shellEscape(containerId) + " /bin/sh", false);
    message = ok ? "Shell session finished." : "Could not open a shell in that container.";
    return ok;
}

bool DockerManager::execDetachedCommand(const std::string& containerId,
                                        const std::string& command,
                                        std::string& message) const {
    if (command.empty()) {
        message = "Please provide a command to run.";
        return false;
    }

    const std::string cmd = "docker exec -d " + shellEscape(containerId) + " /bin/sh -c " +
                            shellEscape(command);
    const bool ok = runCommand(cmd);
    message = ok ? "Command dispatched in detached mode." : "Could not run that command.";
    return ok;
}

bool DockerManager::spinUpMySQL(const std::string& port,
                                const std::string& password,
                                const std::string& version,
                                std::string& message) const {
    const std::string cmd = "docker run -p " + shellEscape(port) +
                            " --name mysql-container -e MYSQL_ROOT_PASSWORD=" +
                            shellEscape(password) + " -d " + shellEscape("mysql:" + version);
    const bool ok = runCommand(cmd);
    message = ok ? "MySQL container launched." : "Could not launch MySQL. Check the version and port mapping.";
    return ok;
}

bool DockerManager::showContainerIP(const std::string& containerId, std::string& message) const {
    const std::string command = "docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' " +
                                shellEscape(containerId);

    std::array<char, 128> buffer{};
    std::string ip;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        message = "Could not inspect that container.";
        return false;
    }

    if (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        ip = buffer.data();
    }
    pclose(pipe);

    while (!ip.empty() && (ip.back() == '\n' || ip.back() == '\r' || ip.back() == ' ' || ip.back() == '\t')) {
        ip.pop_back();
    }

    if (ip.empty()) {
        message = "No IP address found. The container may be stopped.";
        return false;
    }

    message = "Container IP address: " + ip;
    return true;
}

bool DockerManager::createDockerfile(const std::string& baseImage,
                                     const std::string& bashScriptPath,
                                     std::string outputFile,
                                     const std::string& imageName,
                                     std::string& message) const {
    if (baseImage.empty() || bashScriptPath.empty()) {
        message = "Base image and script path are required.";
        return false;
    }

    if (!std::filesystem::exists(bashScriptPath)) {
        message = "Could not find that script file.";
        return false;
    }

    if (outputFile.empty()) {
        outputFile = "Dockerfile";
    }

    std::ifstream scriptFile(bashScriptPath);
    std::ofstream dockerfile(outputFile);

    if (!scriptFile.is_open() || !dockerfile.is_open()) {
        message = "Could not open one of the files.";
        return false;
    }

    dockerfile << "FROM " << baseImage << "\n";
    dockerfile << "WORKDIR /app\n\n";
    dockerfile << "# Auto-generated by Tux-Dock\n";

    std::string line;
    while (std::getline(scriptFile, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.rfind("#", 0) == 0 || line.rfind("#!", 0) == 0) {
            continue;
        }

        dockerfile << "RUN " << line << "\n";
    }

    dockerfile << "\nCMD [\"/bin/bash\"]\n";
    dockerfile.close();
    scriptFile.close();

    if (imageName.empty()) {
        message = "Dockerfile created. Build skipped because no image name was provided.";
        return true;
    }

    const bool ok = runCommand("docker build -t " + shellEscape(imageName) + " -f " +
                               shellEscape(outputFile) + " .");
    message = ok ? "Dockerfile created and image build completed." : "Dockerfile created, but the image build failed.";
    return ok;
}

class TuxDockApp {
public:
    void Run();

private:
    enum class ModalMode { None, Input, Confirm, Select, Message };

    struct RunContainerContext {
        std::string image;
        int port_count = 0;
        std::vector<std::string> ports;
    };

    struct MySqlContext {
        std::string port;
        std::string password;
        std::string version;
    };

    struct DockerfileContext {
        std::string base_image;
        std::string script_path;
        std::string output_file;
        std::string image_name;
    };

    DockerManager docker_;

    std::vector<std::string> menu_entries_ = {
        "Pull Docker Image",
        "Run/Create Interactive Container",
        "List All Containers",
        "List All Images",
        "Start Container Interactively (boot new session)",
        "Start Detached Container Session",
        "Delete Docker Image",
        "Stop Container",
        "Remove Container",
        "Attach Shell to Running Container",
        "Run Detached Command in Container",
        "Spin Up MySQL Container",
        "Get Container IP Address",
        "Create Dockerfile & Build Image from Bash Script",
        "Exit",
    };

    int menu_selected_ = 0;
    std::string status_ = "Ready. Select an action and press Enter.";

    ModalMode modal_mode_ = ModalMode::None;
    std::string modal_title_;
    std::string modal_text_;
    std::string modal_input_;
    std::vector<std::string> modal_select_entries_;
    int modal_select_index_ = 0;

    ftxui::Component menu_component_ = ftxui::Menu(&menu_entries_, &menu_selected_);
    ftxui::Component input_component_ = ftxui::Input(&modal_input_, "Type here");
    ftxui::Component select_component_ = ftxui::Menu(&modal_select_entries_, &modal_select_index_);

    std::function<void(bool, const std::string&)> input_callback_;
    std::function<void(bool)> confirm_callback_;
    std::function<void(bool, int)> select_callback_;

    ftxui::ScreenInteractive* screen_ = nullptr;

    static bool IsDigits(const std::string& value);
    static std::string ShortId(const std::string& id);
    static bool IsValidPortMapping(const std::string& mapping);

    std::string FormatContainerList(const std::vector<DockerManager::ContainerInfo>& containers) const;
    std::string FormatImageList(const std::vector<std::pair<std::string, std::string>>& images) const;

    void SetStatus(const std::string& message);
    void OpenInput(const std::string& title,
                   const std::string& text,
                   std::function<void(bool, const std::string&)> callback,
                   bool secret = false);
    void OpenConfirm(const std::string& title, const std::string& text, std::function<void(bool)> callback);
    void OpenSelect(const std::string& title,
                    const std::string& text,
                    std::vector<std::string> options,
                    std::function<void(bool, int)> callback);
    void OpenMessage(const std::string& title, const std::string& text);
    void ResolveInput(bool confirmed);
    void ResolveConfirm(bool confirmed);
    void ResolveSelect(bool confirmed);
    void CloseMessage();

    void ExecuteSelectedAction();
    void ActionPullImage();
    void ActionRunContainer();
    void ActionListContainers();
    void ActionListImages();
    void ActionStartInteractive();
    void ActionStartDetached();
    void ActionDeleteImage();
    void ActionStopContainer();
    void ActionRemoveContainer();
    void ActionExecShell();
    void ActionExecDetachedCommand();
    void ActionSpinUpMySQL();
    void ActionShowContainerIP();
    void ActionCreateDockerfile();
    void PromptPortCountAndRun(const std::shared_ptr<RunContainerContext>& context);
    void PromptNextPort(const std::shared_ptr<RunContainerContext>& context, int index);

    void PromptContainerSelection(const std::string& title,
                                  std::function<void(const std::string& id, const std::string& name)> callback);
    void PromptImageSelection(const std::string& title,
                              std::function<void(const std::string& id, const std::string& tag)> callback);
    static void ClearTerminal();
    void RunWithRestoredIO(const std::function<void()>& action,
                           bool clear_before = false,
                           bool clear_after = false);

    bool OnEvent(ftxui::Event event);
    ftxui::Element Render() const;
    ftxui::Element RenderModal() const;
};

bool TuxDockApp::IsDigits(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (unsigned char c : value) {
        if (!std::isdigit(c)) {
            return false;
        }
    }
    return true;
}

std::string TuxDockApp::ShortId(const std::string& id) {
    if (id.size() <= 12) {
        return id;
    }
    return id.substr(0, 12);
}

bool TuxDockApp::IsValidPortMapping(const std::string& mapping) {
    const std::size_t separator = mapping.find(':');
    if (separator == std::string::npos) {
        return false;
    }
    const std::string host = mapping.substr(0, separator);
    const std::string container = mapping.substr(separator + 1);
    return IsDigits(host) && IsDigits(container);
}

std::string TuxDockApp::FormatContainerList(
    const std::vector<DockerManager::ContainerInfo>& containers) const {
    std::stringstream ss;
    for (std::size_t i = 0; i < containers.size(); ++i) {
        const auto& container = containers[i];
        const std::string state = container.running ? "running" : "stopped";
        const std::string ports = container.ports.empty() ? "none" : container.ports;
        ss << i + 1 << ". " << container.name << " (" << ShortId(container.id) << ")"
           << " [" << state << "]"
           << " ports: " << ports;
        if (i + 1 < containers.size()) {
            ss << "\n";
        }
    }
    return ss.str();
}

std::string TuxDockApp::FormatImageList(const std::vector<std::pair<std::string, std::string>>& images) const {
    std::stringstream ss;
    for (std::size_t i = 0; i < images.size(); ++i) {
        ss << i + 1 << ". " << images[i].second << " (" << ShortId(images[i].first) << ")";
        if (i + 1 < images.size()) {
            ss << "\n";
        }
    }
    return ss.str();
}

void TuxDockApp::SetStatus(const std::string& message) {
    status_ = message;
}

void TuxDockApp::OpenInput(const std::string& title,
                           const std::string& text,
                           std::function<void(bool, const std::string&)> callback,
                           bool secret) {
    modal_mode_ = ModalMode::Input;
    modal_title_ = title;
    modal_text_ = text;
    modal_input_.clear();
    ftxui::InputOption input_option;
    input_option.password = secret;
    input_component_ = ftxui::Input(&modal_input_, "Type here", input_option);
    input_callback_ = std::move(callback);
}

void TuxDockApp::OpenConfirm(const std::string& title,
                             const std::string& text,
                             std::function<void(bool)> callback) {
    modal_mode_ = ModalMode::Confirm;
    modal_title_ = title;
    modal_text_ = text;
    confirm_callback_ = std::move(callback);
}

void TuxDockApp::OpenSelect(const std::string& title,
                            const std::string& text,
                            std::vector<std::string> options,
                            std::function<void(bool, int)> callback) {
    modal_mode_ = ModalMode::Select;
    modal_title_ = title;
    modal_text_ = text;
    modal_select_entries_ = std::move(options);
    modal_select_index_ = 0;
    select_component_ = ftxui::Menu(&modal_select_entries_, &modal_select_index_);
    select_callback_ = std::move(callback);
}

void TuxDockApp::OpenMessage(const std::string& title, const std::string& text) {
    modal_mode_ = ModalMode::Message;
    modal_title_ = title;
    modal_text_ = text;
}

void TuxDockApp::ResolveInput(bool confirmed) {
    const std::string value = modal_input_;
    auto callback = std::move(input_callback_);

    modal_mode_ = ModalMode::None;
    modal_title_.clear();
    modal_text_.clear();
    modal_input_.clear();
    input_component_ = ftxui::Input(&modal_input_, "Type here");
    input_callback_ = {};

    if (callback) {
        callback(confirmed, value);
    }
}

void TuxDockApp::ResolveConfirm(bool confirmed) {
    auto callback = std::move(confirm_callback_);

    modal_mode_ = ModalMode::None;
    modal_title_.clear();
    modal_text_.clear();
    confirm_callback_ = {};

    if (callback) {
        callback(confirmed);
    }
}

void TuxDockApp::ResolveSelect(bool confirmed) {
    const int selected = modal_select_index_;
    auto callback = std::move(select_callback_);

    modal_mode_ = ModalMode::None;
    modal_title_.clear();
    modal_text_.clear();
    modal_select_entries_.clear();
    modal_select_index_ = 0;
    select_component_ = ftxui::Menu(&modal_select_entries_, &modal_select_index_);
    select_callback_ = {};

    if (callback) {
        callback(confirmed, selected);
    }
}

void TuxDockApp::CloseMessage() {
    modal_mode_ = ModalMode::None;
    modal_title_.clear();
    modal_text_.clear();
}

void TuxDockApp::PromptContainerSelection(
    const std::string& title,
    std::function<void(const std::string& id, const std::string& name)> callback) {
    auto containers = docker_.getContainerList();
    if (containers.empty()) {
        SetStatus("No containers available.");
        return;
    }

    std::vector<std::string> options;
    options.reserve(containers.size());
    for (const auto& container : containers) {
        const std::string state = container.running ? "running" : "stopped";
        const std::string ports = container.ports.empty() ? "none" : container.ports;
        options.push_back(container.name + " (" + ShortId(container.id) + ") [" + state + "] ports: " + ports);
    }

    OpenSelect(title,
               "Select a container with arrows and press Enter.",
               std::move(options),
               [this, containers, callback = std::move(callback)](bool confirmed, int selected) mutable {
                   if (!confirmed) {
                       SetStatus("Action cancelled.");
                       return;
                   }
                   if (selected < 0 || selected >= static_cast<int>(containers.size())) {
                       SetStatus("Please choose a valid container.");
                       return;
                   }
                   const auto& chosen = containers[static_cast<std::size_t>(selected)];
                   callback(chosen.id, chosen.name);
               });
}

void TuxDockApp::PromptImageSelection(const std::string& title,
                                      std::function<void(const std::string& id, const std::string& tag)> callback) {
    auto images = docker_.getImageList();
    if (images.empty()) {
        SetStatus("No images found.");
        return;
    }

    std::vector<std::string> options;
    options.reserve(images.size());
    for (const auto& image : images) {
        options.push_back(image.second + " (" + ShortId(image.first) + ")");
    }

    OpenSelect(title,
               "Select an image with arrows and press Enter.",
               std::move(options),
               [this, images, callback = std::move(callback)](bool confirmed, int selected) mutable {
                   if (!confirmed) {
                       SetStatus("Action cancelled.");
                       return;
                   }
                   if (selected < 0 || selected >= static_cast<int>(images.size())) {
                       SetStatus("Please choose a valid image.");
                       return;
                   }
                   const auto& chosen = images[static_cast<std::size_t>(selected)];
                   callback(chosen.first, chosen.second);
               });
}

void TuxDockApp::ClearTerminal() {
    std::cout << "\x1b[2J\x1b[H" << std::flush;
}

void TuxDockApp::RunWithRestoredIO(const std::function<void()>& action,
                                   bool clear_before,
                                   bool clear_after) {
    if (screen_ != nullptr) {
        screen_->WithRestoredIO([&] {
            if (clear_before) {
                ClearTerminal();
            }
            action();
            if (clear_after) {
                ClearTerminal();
            }
        })();
        return;
    }
    if (clear_before) {
        ClearTerminal();
    }
    action();
    if (clear_after) {
        ClearTerminal();
    }
}

void TuxDockApp::ActionPullImage() {
    const std::vector<std::string> quick_images = {
        "debian:stable",
        "ubuntu:noble",
        "rockylinux:9.3",
        "alpine:latest",
    };

    std::vector<std::string> options = quick_images;
    options.push_back("Custom image...");

    OpenSelect("Pull Docker Image",
               "Select an image with arrows, then press Enter.",
               std::move(options),
               [this, quick_images](bool confirmed, int selected) {
                   if (!confirmed) {
                       SetStatus("Image pull cancelled.");
                       return;
                   }

                   if (selected < 0 || selected > static_cast<int>(quick_images.size())) {
                       SetStatus("Please select a valid image option.");
                       return;
                   }

                   if (selected < static_cast<int>(quick_images.size())) {
                       std::string message;
                       SetStatus("Pulling image...");
                       RunWithRestoredIO([this, &message, &quick_images, selected] {
                           docker_.pullImage(quick_images[static_cast<std::size_t>(selected)], message);
                       });
                       SetStatus(message);
                       return;
                   }

                   OpenInput("Pull Docker Image",
                             "Enter custom Docker image name:",
                             [this](bool custom_confirmed, const std::string& image) {
                                 if (!custom_confirmed) {
                                     SetStatus("Image pull cancelled.");
                                     return;
                                 }
                                 if (image.empty()) {
                                     SetStatus("Image name cannot be empty.");
                                     ActionPullImage();
                                     return;
                                 }
                                 std::string message;
                                 SetStatus("Pulling image...");
                                 RunWithRestoredIO([this, &message, &image] { docker_.pullImage(image, message); });
                                 SetStatus(message);
                             });
               });
}

void TuxDockApp::ActionRunContainer() {
    auto images = docker_.getImageList();
    std::vector<std::string> options;
    options.reserve(images.size() + 1);
    for (const auto& image : images) {
        options.push_back(image.second + " (" + ShortId(image.first) + ")");
    }
    options.push_back("Custom image...");

    OpenSelect("Run Interactive Container",
               "Select an image with arrows, then press Enter.",
               std::move(options),
               [this, images](bool confirmed, int selected) {
                   if (!confirmed) {
                       SetStatus("Container run cancelled.");
                       return;
                   }

                   if (selected < 0 || selected > static_cast<int>(images.size())) {
                       SetStatus("Please select a valid image option.");
                       return;
                   }

                   auto context = std::make_shared<RunContainerContext>();
                   if (selected == static_cast<int>(images.size())) {
                       OpenInput("Run Interactive Container",
                                 "Enter custom image name:",
                                 [this, context](bool custom_confirmed, const std::string& image_name) {
                                     if (!custom_confirmed) {
                                         SetStatus("Container run cancelled.");
                                         return;
                                     }
                                     if (image_name.empty()) {
                                         SetStatus("Image name cannot be empty.");
                                         ActionRunContainer();
                                         return;
                                     }
                                     context->image = image_name;
                                     PromptPortCountAndRun(context);
                                 });
                       return;
                   }

                   context->image = images[static_cast<std::size_t>(selected)].second;
                   PromptPortCountAndRun(context);
               });
}

void TuxDockApp::PromptPortCountAndRun(const std::shared_ptr<RunContainerContext>& context) {
    OpenInput("Port Mappings",
              "How many port mappings? (0 for none)",
              [this, context](bool confirmed, const std::string& count_value) {
                  if (!confirmed) {
                      SetStatus("Container run cancelled.");
                      return;
                  }
                  if (!IsDigits(count_value)) {
                      SetStatus("Please enter a valid number.");
                      PromptPortCountAndRun(context);
                      return;
                  }

                  context->port_count = std::stoi(count_value);
                  context->ports.clear();
                  if (context->port_count < 0) {
                      SetStatus("Port mappings cannot be negative.");
                      PromptPortCountAndRun(context);
                      return;
                  }

                  PromptNextPort(context, 0);
              });
}

void TuxDockApp::PromptNextPort(const std::shared_ptr<RunContainerContext>& context, int index) {
    if (index >= context->port_count) {
        std::string message;
        SetStatus("Starting interactive container...");
        RunWithRestoredIO([this, context, &message] {
            docker_.runContainerInteractive(context->image, context->ports, message);
        }, true, true);
        SetStatus(message);
        return;
    }

    OpenInput("Port Mapping",
              "Enter mapping #" + std::to_string(index + 1) + " (example: 8080:80)",
              [this, context, index](bool confirmed, const std::string& mapping) {
                  if (!confirmed) {
                      SetStatus("Container run cancelled.");
                      return;
                  }
                  if (!IsValidPortMapping(mapping)) {
                      SetStatus("Use host:container format, for example 8080:80.");
                      PromptNextPort(context, index);
                      return;
                  }

                  context->ports.push_back(mapping);
                  PromptNextPort(context, index + 1);
              });
}

void TuxDockApp::ActionListContainers() {
    auto containers = docker_.getContainerList();
    if (containers.empty()) {
        SetStatus("No containers available.");
        return;
    }
    OpenMessage("Containers", FormatContainerList(containers));
    SetStatus("Container list opened.");
}

void TuxDockApp::ActionListImages() {
    auto images = docker_.getImageList();
    if (images.empty()) {
        SetStatus("No images found.");
        return;
    }
    OpenMessage("Images", FormatImageList(images));
    SetStatus("Image list opened.");
}

void TuxDockApp::ActionStartInteractive() {
    PromptContainerSelection("Start Interactively", [this](const std::string& id, const std::string&) {
        std::string message;
        SetStatus("Starting interactive session...");
        RunWithRestoredIO([this, &id, &message] { docker_.startInteractive(id, message); }, true, true);
        SetStatus(message);
    });
}

void TuxDockApp::ActionStartDetached() {
    PromptContainerSelection("Start Detached", [this](const std::string& id, const std::string&) {
        std::string message;
        SetStatus("Starting container in detached mode...");
        RunWithRestoredIO([this, &id, &message] { docker_.startDetached(id, message); });
        SetStatus(message);
    });
}

void TuxDockApp::ActionDeleteImage() {
    PromptImageSelection("Delete Image", [this](const std::string& id, const std::string& tag) {
        OpenConfirm("Delete Image",
                    "Delete image " + tag + "?",
                    [this, id](bool confirmed) {
                        if (!confirmed) {
                            SetStatus("Image deletion cancelled.");
                            return;
                        }
                        std::string message;
                        SetStatus("Deleting image...");
                        RunWithRestoredIO([this, &id, &message] { docker_.deleteImage(id, message); });
                        SetStatus(message);
                    });
    });
}

void TuxDockApp::ActionStopContainer() {
    PromptContainerSelection("Stop Container", [this](const std::string& id, const std::string&) {
        std::string message;
        SetStatus("Stopping container...");
        RunWithRestoredIO([this, &id, &message] { docker_.stopContainer(id, message); });
        SetStatus(message);
    });
}

void TuxDockApp::ActionRemoveContainer() {
    PromptContainerSelection("Remove Container", [this](const std::string& id, const std::string& name) {
        OpenConfirm("Remove Container",
                    "Remove container " + name + "?",
                    [this, id](bool confirmed) {
                        if (!confirmed) {
                            SetStatus("Container removal cancelled.");
                            return;
                        }
                        std::string message;
                        SetStatus("Removing container...");
                        RunWithRestoredIO([this, &id, &message] { docker_.removeContainer(id, message); });
                        SetStatus(message);
                    });
    });
}

void TuxDockApp::ActionExecShell() {
    PromptContainerSelection("Open Shell", [this](const std::string& id, const std::string&) {
        std::string message;
        SetStatus("Opening shell session...");
        RunWithRestoredIO([this, &id, &message] { docker_.execShell(id, message); }, true, true);
        SetStatus(message);
    });
}

void TuxDockApp::ActionExecDetachedCommand() {
    PromptContainerSelection("Run Detached Command",
                             [this](const std::string& id, const std::string& name) {
                                 OpenInput("Detached Command",
                                           "Enter command to run in " + name + ":",
                                           [this, id](bool confirmed, const std::string& command) {
                                                if (!confirmed) {
                                                    SetStatus("Detached command cancelled.");
                                                    return;
                                                }
                                                std::string message;
                                                SetStatus("Running detached command...");
                                                RunWithRestoredIO([this, &id, &command, &message] {
                                                    docker_.execDetachedCommand(id, command, message);
                                                });
                                                SetStatus(message);
                                            });
                             });
}

void TuxDockApp::ActionSpinUpMySQL() {
    auto context = std::make_shared<MySqlContext>();

    OpenInput("MySQL Setup",
              "Enter port mapping (example: 3306:3306)",
              [this, context](bool confirmed, const std::string& port) {
                  if (!confirmed) {
                      SetStatus("MySQL setup cancelled.");
                      return;
                  }
                  if (!IsValidPortMapping(port)) {
                      SetStatus("Use host:container format, for example 3306:3306.");
                      return;
                  }
                  context->port = port;

                  OpenInput("MySQL Setup",
                            "Enter MySQL root password:",
                            [this, context](bool pwd_confirmed, const std::string& password) {
                                if (!pwd_confirmed) {
                                    SetStatus("MySQL setup cancelled.");
                                    return;
                                }
                                if (password.empty()) {
                                    SetStatus("Password cannot be empty.");
                                    return;
                                }
                                context->password = password;

                                OpenInput("MySQL Setup",
                                          "Enter MySQL version tag (example: 8)",
                                          [this, context](bool ver_confirmed, const std::string& version) {
                                              if (!ver_confirmed) {
                                                  SetStatus("MySQL setup cancelled.");
                                                  return;
                                              }
                                              if (version.empty()) {
                                                  SetStatus("Version tag cannot be empty.");
                                                  return;
                                              }
                                              context->version = version;

                                              std::string message;
                                              SetStatus("Launching MySQL container...");
                                              RunWithRestoredIO([this, context, &message] {
                                                  docker_.spinUpMySQL(context->port,
                                                                      context->password,
                                                                      context->version,
                                                                      message);
                                              });
                                              SetStatus(message);
                                          });
                            },
                            true);
              });
}

void TuxDockApp::ActionShowContainerIP() {
    PromptContainerSelection("Container IP", [this](const std::string& id, const std::string&) {
        std::string message;
        SetStatus("Looking up container IP...");
        RunWithRestoredIO([this, &id, &message] { docker_.showContainerIP(id, message); });
        SetStatus(message);
    });
}

void TuxDockApp::ActionCreateDockerfile() {
    auto context = std::make_shared<DockerfileContext>();

    OpenInput("Dockerfile Builder",
              "Enter base image (example: ubuntu:22.04)",
              [this, context](bool base_confirmed, const std::string& base) {
                  if (!base_confirmed) {
                      SetStatus("Dockerfile workflow cancelled.");
                      return;
                  }
                  if (base.empty()) {
                      SetStatus("Base image cannot be empty.");
                      return;
                  }
                  context->base_image = base;

                  OpenInput("Dockerfile Builder",
                            "Enter bash script path:",
                            [this, context](bool script_confirmed, const std::string& script_path) {
                                if (!script_confirmed) {
                                    SetStatus("Dockerfile workflow cancelled.");
                                    return;
                                }
                                if (script_path.empty()) {
                                    SetStatus("Script path cannot be empty.");
                                    return;
                                }
                                context->script_path = script_path;

                                OpenInput("Dockerfile Builder",
                                          "Enter output Dockerfile name (leave empty for Dockerfile)",
                                          [this, context](bool output_confirmed, const std::string& output_file) {
                                              if (!output_confirmed) {
                                                  SetStatus("Dockerfile workflow cancelled.");
                                                  return;
                                              }
                                              context->output_file = output_file;

                                              OpenInput("Dockerfile Builder",
                                                        "Enter image name to build (leave empty to skip build)",
                                                        [this, context](bool image_confirmed,
                                                                        const std::string& image_name) {
                                                            if (!image_confirmed) {
                                                                SetStatus("Dockerfile workflow cancelled.");
                                                                return;
                                                            }
                                                            context->image_name = image_name;

                                                            std::string message;
                                                            SetStatus("Creating Dockerfile and running build...");
                                                            RunWithRestoredIO([this, context, &message] {
                                                                docker_.createDockerfile(context->base_image,
                                                                                         context->script_path,
                                                                                         context->output_file,
                                                                                         context->image_name,
                                                                                         message);
                                                            });
                                                            SetStatus(message);
                                                        });
                                          });
                            });
              });
}

void TuxDockApp::ExecuteSelectedAction() {
    switch (menu_selected_) {
        case 0:
            ActionPullImage();
            break;
        case 1:
            ActionRunContainer();
            break;
        case 2:
            ActionListContainers();
            break;
        case 3:
            ActionListImages();
            break;
        case 4:
            ActionStartInteractive();
            break;
        case 5:
            ActionStartDetached();
            break;
        case 6:
            ActionDeleteImage();
            break;
        case 7:
            ActionStopContainer();
            break;
        case 8:
            ActionRemoveContainer();
            break;
        case 9:
            ActionExecShell();
            break;
        case 10:
            ActionExecDetachedCommand();
            break;
        case 11:
            ActionSpinUpMySQL();
            break;
        case 12:
            ActionShowContainerIP();
            break;
        case 13:
            ActionCreateDockerfile();
            break;
        case 14:
            SetStatus("Exiting Tux-Dock.");
            if (screen_ != nullptr) {
                screen_->ExitLoopClosure()();
            }
            break;
        default:
            SetStatus("Please choose a valid menu option.");
            break;
    }
}

bool TuxDockApp::OnEvent(ftxui::Event event) {
    if (modal_mode_ == ModalMode::Input) {
        if (event == ftxui::Event::Return) {
            ResolveInput(true);
            return true;
        }
        if (event == ftxui::Event::Escape) {
            ResolveInput(false);
            return true;
        }
        return input_component_->OnEvent(event);
    }

    if (modal_mode_ == ModalMode::Confirm) {
        if (event == ftxui::Event::Character("y") || event == ftxui::Event::Character("Y") ||
            event == ftxui::Event::Return) {
            ResolveConfirm(true);
            return true;
        }
        if (event == ftxui::Event::Character("n") || event == ftxui::Event::Character("N") ||
            event == ftxui::Event::Escape) {
            ResolveConfirm(false);
            return true;
        }
        return true;
    }

    if (modal_mode_ == ModalMode::Select) {
        if (event == ftxui::Event::Return) {
            ResolveSelect(true);
            return true;
        }
        if (event == ftxui::Event::Escape) {
            ResolveSelect(false);
            return true;
        }
        return select_component_->OnEvent(event);
    }

    if (modal_mode_ == ModalMode::Message) {
        if (event == ftxui::Event::Return || event == ftxui::Event::Escape) {
            CloseMessage();
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Return) {
        ExecuteSelectedAction();
        return true;
    }

    return false;
}

ftxui::Element TuxDockApp::RenderModal() const {
    using namespace ftxui;

    Element body;
    Element footer;

    if (modal_mode_ == ModalMode::Input) {
        body = vbox(ftxui::Elements{paragraph(modal_text_), separator(), input_component_->Render() | border});
        footer = text("Enter: confirm   Esc: cancel") | dim;
    } else if (modal_mode_ == ModalMode::Confirm) {
        body = paragraph(modal_text_);
        footer = text("Y/Enter: confirm   N/Esc: cancel") | dim;
    } else if (modal_mode_ == ModalMode::Select) {
        body = vbox(ftxui::Elements{paragraph(modal_text_), separator(), select_component_->Render() | frame | vscroll_indicator});
        footer = text("Up/Down: choose   Enter: confirm   Esc: cancel") | dim;
    } else {
        body = paragraph(modal_text_) | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14);
        footer = text("Enter/Esc: close") | dim;
    }

    return window(text(modal_title_), vbox(ftxui::Elements{body, separator(), footer})) |
           size(WIDTH, GREATER_THAN, 70) | size(HEIGHT, GREATER_THAN, 12) | center;
}

ftxui::Element TuxDockApp::Render() const {
    using namespace ftxui;

    auto actions_panel = window(text("Actions"),
                                vbox(ftxui::Elements{menu_component_->Render() | frame | vscroll_indicator,
                                                     separator(),
                                                     text("Up/Down: navigate   Enter: select") | dim})) |
                         size(WIDTH, GREATER_THAN, 48) | flex;

    auto status_panel = window(text("Status"), vbox(ftxui::Elements{paragraph(status_) | yflex, separator(),
                                                                     text("High-level updates only") | dim})) |
                        size(WIDTH, GREATER_THAN, 48) | flex;

    Element base = hbox(ftxui::Elements{actions_panel, separator(), status_panel}) | border;

    if (modal_mode_ == ModalMode::None) {
        return base;
    }

    return dbox({base, RenderModal() | clear_under | center});
}

void TuxDockApp::Run() {
    auto root = ftxui::Renderer(menu_component_, [this] { return Render(); });
    auto app = ftxui::CatchEvent(root, [this](ftxui::Event event) { return OnEvent(event); });

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    screen_ = &screen;
    screen.Loop(app);
    screen_ = nullptr;
}

int main() {
    TuxDockApp app;
    app.Run();
    return 0;
}
