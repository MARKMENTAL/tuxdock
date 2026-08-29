#include "src/docker_manager.hpp"
#include "src/operation_state.hpp"

#include <cctype>
#include <csignal>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace {

volatile std::sig_atomic_t g_sigint_busy = 0;
volatile std::sig_atomic_t g_sigint_trapped = 0;
volatile std::sig_atomic_t g_sigtstp_trapped = 0;
bool g_sigint_trap_installed = false;
bool g_sigtstp_trap_installed = false;
struct sigaction g_sigint_previous {};
struct sigaction g_sigtstp_previous {};

void TuxDockHandleSigint(int signal_number) {
    if (g_sigint_busy != 0) {
        ++g_sigint_trapped;
        return;
    }
    if (g_sigint_trap_installed) {
        const auto previous_handler = g_sigint_previous.sa_handler;
        if (previous_handler != nullptr && previous_handler != SIG_DFL) {
            if (previous_handler != SIG_IGN) previous_handler(signal_number);
            return;
        }
    }
    std::signal(signal_number, SIG_DFL);
    std::raise(signal_number);
}

void TuxDockHandleSigtstp(int signal_number) {
    if (g_sigint_busy != 0) {
        ++g_sigtstp_trapped;
        return;
    }
    if (g_sigtstp_trap_installed) {
        const auto previous_handler = g_sigtstp_previous.sa_handler;
        if (previous_handler != nullptr && previous_handler != SIG_DFL) {
            if (previous_handler != SIG_IGN) previous_handler(signal_number);
            return;
        }
    }
    std::signal(signal_number, SIG_DFL);
    std::raise(signal_number);
}

void InstallSignalTraps() {
    g_sigint_trapped = 0;
    g_sigtstp_trapped = 0;
    g_sigint_busy = 1;
    if (!g_sigint_trap_installed) {
        struct sigaction trap {};
        sigemptyset(&trap.sa_mask);
        trap.sa_flags = SA_RESTART;
        trap.sa_handler = TuxDockHandleSigint;
        if (sigaction(SIGINT, &trap, &g_sigint_previous) == 0) {
            g_sigint_trap_installed = true;
        }
    }
    if (!g_sigtstp_trap_installed) {
        struct sigaction trap {};
        sigemptyset(&trap.sa_mask);
        trap.sa_flags = SA_RESTART;
        trap.sa_handler = TuxDockHandleSigtstp;
        if (sigaction(SIGTSTP, &trap, &g_sigtstp_previous) == 0) {
            g_sigtstp_trap_installed = true;
        }
    }
}

void RemoveSignalTraps() {
    g_sigint_busy = 0;
    g_sigint_trapped = 0;
    g_sigtstp_trapped = 0;
    if (g_sigint_trap_installed) {
        sigaction(SIGINT, &g_sigint_previous, nullptr);
        g_sigint_trap_installed = false;
    }
    if (g_sigtstp_trap_installed) {
        sigaction(SIGTSTP, &g_sigtstp_previous, nullptr);
        g_sigtstp_trap_installed = false;
    }
}

}

class TuxDockApp {
public:
    int Run();

private:
    enum class ModalMode { None, Input, Confirm, Select, Message, Busy };
    struct RunContainerContext {
        std::string name;
        std::string image;
        int port_count = 0;
        std::vector<std::string> ports;
    };

    DockerManager docker_;
    std::vector<DockerManager::ContainerInfo> containers_;
    std::vector<DockerManager::ImageInfo> images_;
    std::vector<std::string> menu_entries_ = {
        "Pull Docker Image",
        "Create Container",
        "List All Containers",
        "List All Images",
        "Start Detached Container Session",
        "Delete Docker Image",
        "Stop Container",
        "Remove Container",
        "Attach Shell to Running Container",
        "Run Detached Command in Container",
        "About Tux-Dock",
        "Exit",
    };
    int menu_selected_ = 0;
    std::string status_ = "Ready. Select an action and press Enter.";
    ModalMode modal_mode_ = ModalMode::None;
    std::string modal_title_;
    std::string modal_text_;
    std::string modal_input_;
    ftxui::Element modal_content_;
    std::vector<std::string> modal_select_entries_;
    int modal_select_index_ = 0;
    ftxui::Component menu_component_ = ftxui::Menu(&menu_entries_, &menu_selected_);
    ftxui::Component input_component_ = ftxui::Input(&modal_input_, "Type here");
    ftxui::Component select_component_ = ftxui::Menu(&modal_select_entries_, &modal_select_index_);
    std::function<void(bool, const std::string&)> input_callback_;
    std::function<void(bool)> confirm_callback_;
    std::function<void(bool, int)> select_callback_;
    ftxui::ScreenInteractive* screen_ = nullptr;
    std::thread refresh_thread_;
    std::thread spinner_thread_;
    std::atomic<bool> spinner_stop_{true};
    std::vector<std::thread> action_threads_;
    OperationState operation_state_;
    std::size_t spinner_frame_ = 0;
    static constexpr int kMaxStopTimeout = 300;

    static bool IsDigits(const std::string& value);
    static bool IsValidContainerName(const std::string& name);
    static std::string ShortId(const std::string& id);
    static bool IsValidPortMapping(const std::string& mapping);
    ftxui::Element FormatContainerList(const std::vector<DockerManager::ContainerInfo>& containers) const;
    ftxui::Element FormatImageList(const std::vector<DockerManager::ImageInfo>& images) const;
    void SetStatus(const std::string& message) { status_ = message; }
    void OpenInput(const std::string&, const std::string&,
                   std::function<void(bool, const std::string&)>, bool secret = false);
    void OpenConfirm(const std::string&, const std::string&, std::function<void(bool)>);
    void OpenSelect(const std::string&, const std::string&, std::vector<std::string>,
                    std::function<void(bool, int)>);
    void OpenMessage(const std::string& title, const std::string& text);
    void OpenListMessage(const std::string& title, ftxui::Element content);
    void ResolveInput(bool);
    void ResolveConfirm(bool);
    void ResolveSelect(bool);
    void CloseMessage();
    void ExecuteSelectedAction();
    void ActionPullImage();
    void ActionRunContainer();
    void ActionListContainers();
    void ActionListImages();
    void ActionStartDetached();
    void ActionDeleteImage();
    void ActionStopContainer();
    void ActionRemoveContainer();
    void ActionExecShell();
    void ActionExecDetachedCommand();
    void ActionAbout();
    void PromptPortCountAndRun(const std::shared_ptr<RunContainerContext>&);
    void PromptNextPort(const std::shared_ptr<RunContainerContext>&, int);
    void PromptContainerSelection(
        const std::string&,
        std::function<void(const std::string&, const std::string&)>);
    void PromptImageSelection(
        const std::string&,
        std::function<void(const std::string&, const std::string&)>);
    void RunDeferredStatusAction(const std::string&, std::function<std::string()>);
    void BeginBusyOperation(const std::string&, const std::string&, std::function<std::string()>);
    void StartSpinner();
    void StopSpinner();
    void BeginStopOperation(const std::string& id, int timeout_seconds = 10);
    void RefreshState(const std::string& message = "Refreshing Docker state...");
    void ApplyRefreshResults(DockerManager::ListResult<DockerManager::ContainerInfo> containers,
                             DockerManager::ListResult<DockerManager::ImageInfo> images);
    static void ClearTerminal();
    void RunWithRestoredIO(const std::function<void()>&, bool clear_before = false,
                           bool clear_after = false);
    bool OnEvent(ftxui::Event);
    ftxui::Element Render() const;
    ftxui::Element RenderModal() const;
};

bool TuxDockApp::IsDigits(const std::string& value) {
    if (value.empty()) return false;
    for (unsigned char c : value) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

bool TuxDockApp::IsValidContainerName(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name) {
        if (!(std::isalnum(c) || c == '_' || c == '.' || c == '-')) return false;
    }
    return true;
}

std::string TuxDockApp::ShortId(const std::string& id) {
    return id.size() <= 12 ? id : id.substr(0, 12);
}

bool TuxDockApp::IsValidPortMapping(const std::string& mapping) {
    const auto separator = mapping.find(':');
    return separator != std::string::npos &&
           IsDigits(mapping.substr(0, separator)) &&
           IsDigits(mapping.substr(separator + 1));
}
ftxui::Element TuxDockApp::FormatContainerList(const std::vector<DockerManager::ContainerInfo>& items) const {
    using namespace ftxui;
    Elements rows;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& container = items[i];
        rows.push_back(vbox(Elements{
            text("[" + std::to_string(i + 1) + "] " + container.name) | bold,
            text((container.running ? "RUNNING" : "STOPPED") + std::string("  id ") + ShortId(container.id)) | dim,
            text("ports: " + (container.ports.empty() ? std::string("none") : container.ports)) | dim,
        }));
        if (i + 1 < items.size()) rows.push_back(separator());
    }
    return vbox(std::move(rows));
}

ftxui::Element TuxDockApp::FormatImageList(const std::vector<DockerManager::ImageInfo>& items) const {
    using namespace ftxui;
    Elements rows;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& image = items[i];
        rows.push_back(vbox(Elements{
            text("[" + std::to_string(i + 1) + "] " + image.second) | bold,
            text("id " + ShortId(image.first)) | dim,
        }));
        if (i + 1 < items.size()) rows.push_back(separator());
    }
    return vbox(std::move(rows));
}

void TuxDockApp::RefreshState(const std::string& message) {
    SetStatus(message);
    if (refresh_thread_.joinable()) refresh_thread_.join();
    auto* active = screen_;
    refresh_thread_ = std::thread([this, active] {
        auto containers = docker_.getContainerList();
        auto images = docker_.getImageList();
        if (active == nullptr || ftxui::ScreenInteractive::Active() != active) return;
        active->Post([this, containers = std::move(containers), images = std::move(images)]() mutable {
            ApplyRefreshResults(std::move(containers), std::move(images));
        });
        active->PostEvent(ftxui::Event::Custom);
    });
}

void TuxDockApp::ApplyRefreshResults(
    DockerManager::ListResult<DockerManager::ContainerInfo> containers,
    DockerManager::ListResult<DockerManager::ImageInfo> images) {
    std::string error;
    if (containers.ok()) {
        containers_ = std::move(containers.items);
    } else {
        error = "Containers: " + containers.error;
    }
    if (images.ok()) {
        images_ = std::move(images.items);
    } else {
        if (!error.empty()) error += "\n";
        error += "Images: " + images.error;
    }
    SetStatus(error.empty() ? "Docker state refreshed." : "Refresh failed; cached state preserved.\n" + error);
}

void TuxDockApp::OpenInput(const std::string& title,
                           const std::string& text,
                           std::function<void(bool, const std::string&)> callback,
                           bool secret) {
    modal_mode_ = ModalMode::Input;
    modal_title_ = title;
    modal_text_ = text;
    modal_input_.clear();

    ftxui::InputOption option;
    option.password = secret;
    input_component_ = ftxui::Input(&modal_input_, "Type here", option);
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
    modal_content_ = {};
}

void TuxDockApp::OpenListMessage(const std::string& title, ftxui::Element content) {
    modal_mode_ = ModalMode::Message;
    modal_title_ = title;
    modal_text_.clear();
    modal_content_ = std::move(content);
}

void TuxDockApp::ResolveInput(bool confirmed) {
    auto callback = std::move(input_callback_);
    const auto value = modal_input_;
    modal_mode_ = ModalMode::None;
    input_callback_ = {};
    if (callback) callback(confirmed, value);
}

void TuxDockApp::ResolveConfirm(bool confirmed) {
    auto callback = std::move(confirm_callback_);
    modal_mode_ = ModalMode::None;
    confirm_callback_ = {};
    if (callback) callback(confirmed);
}

void TuxDockApp::ResolveSelect(bool confirmed) {
    auto callback = std::move(select_callback_);
    const int selected = modal_select_index_;
    modal_mode_ = ModalMode::None;
    select_callback_ = {};
    if (callback) callback(confirmed, selected);
}

void TuxDockApp::CloseMessage() {
    modal_mode_ = ModalMode::None;
    modal_content_ = {};
}

void TuxDockApp::PromptContainerSelection(
    const std::string& title,
    std::function<void(const std::string&, const std::string&)> callback) {
    if (containers_.empty()) {
        RefreshState("No cached containers. Refreshing...");
        return;
    }

    std::vector<std::string> options;
    for (const auto& container : containers_) {
        options.push_back(container.name + " (" + ShortId(container.id) + ") [" +
                         (container.running ? "running" : "stopped") + "]");
    }

    OpenSelect(
        title,
        "Select a container with arrows and press Enter.",
        std::move(options),
        [this, callback = std::move(callback)](bool ok, int selected) {
            if (!ok) return SetStatus("Action cancelled.");
            if (selected < 0 || selected >= static_cast<int>(containers_.size())) {
                return SetStatus("Please choose a valid container.");
            }
            const auto& container = containers_[static_cast<std::size_t>(selected)];
            callback(container.id, container.name);
        });
}

void TuxDockApp::PromptImageSelection(
    const std::string& title,
    std::function<void(const std::string&, const std::string&)> callback) {
    if (images_.empty()) {
        RefreshState("No cached images. Refreshing...");
        return;
    }

    std::vector<std::string> options;
    for (const auto& image : images_) {
        options.push_back(image.second + " (" + ShortId(image.first) + ")");
    }

    OpenSelect(
        title,
        "Select an image with arrows and press Enter.",
        std::move(options),
        [this, callback = std::move(callback)](bool ok, int selected) {
            if (!ok) return SetStatus("Action cancelled.");
            if (selected < 0 || selected >= static_cast<int>(images_.size())) {
                return SetStatus("Please choose a valid image.");
            }
            const auto& image = images_[static_cast<std::size_t>(selected)];
            callback(image.first, image.second);
        });
}

void TuxDockApp::RunDeferredStatusAction(const std::string& wait, std::function<std::string()> action) {
    BeginBusyOperation("Working", wait, std::move(action));
}

void TuxDockApp::StartSpinner() {
    StopSpinner();
    spinner_stop_ = false;
    auto* active = screen_;
    spinner_thread_ = std::thread([this, active] {
        while (active && !spinner_stop_.load() && ftxui::ScreenInteractive::Active() == active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (spinner_stop_.load() || ftxui::ScreenInteractive::Active() != active) break;
            active->RequestAnimationFrame();
            active->Post([this] { if (!spinner_stop_.load()) ++spinner_frame_; });
        }
    });
}

void TuxDockApp::StopSpinner() {
    spinner_stop_ = true;
    if (spinner_thread_.joinable()) spinner_thread_.join();
}

void TuxDockApp::BeginBusyOperation(const std::string& title,
                                    const std::string& message,
                                    std::function<std::string()> action) {
    operation_state_.begin(title, message);
    modal_mode_ = ModalMode::Busy;
    spinner_frame_ = 0;
    InstallSignalTraps();
    auto* active = screen_;
    StartSpinner();
    if (active) active->PostEvent(ftxui::Event::Custom);
    action_threads_.emplace_back([this, active, action = std::move(action)]() mutable {
        const auto message = action();
        if (active && ftxui::ScreenInteractive::Active() == active) {
            active->Post([this, message] {
                RemoveSignalTraps();
                operation_state_.complete(message);
                StopSpinner();
                modal_mode_ = ModalMode::None;
                OpenMessage("Operation complete", message);
                RefreshState();
            });
            active->PostEvent(ftxui::Event::Custom);
        }
    });
}
void TuxDockApp::BeginStopOperation(const std::string& id, int timeout_seconds) {
    operation_state_.begin("Stopping container", "Stopping and refreshing state...");
    modal_mode_ = ModalMode::Busy;
    InstallSignalTraps();
    auto* active = screen_;
    spinner_frame_ = 0;
    StartSpinner();
    action_threads_.emplace_back([this, active, id, timeout_seconds] {
        std::string message;
        const bool stopped = docker_.stopContainer(id, message, timeout_seconds);
        auto containers = docker_.getContainerList();
        auto images = docker_.getImageList();
        if (!active || ftxui::ScreenInteractive::Active() != active) return;
        active->Post([this, stopped, message, containers = std::move(containers), images = std::move(images)]() mutable {
            RemoveSignalTraps();
            ApplyRefreshResults(std::move(containers), std::move(images));
            operation_state_.complete(message);
            StopSpinner();
            modal_mode_ = ModalMode::None;
            OpenMessage(stopped ? "Container stopped" : "Stop failed", message);
        });
        active->PostEvent(ftxui::Event::Custom);
    });
}
void TuxDockApp::ClearTerminal() { std::cout << "\x1b[2J\x1b[H" << std::flush; }
void TuxDockApp::RunWithRestoredIO(const std::function<void()>& action,
                                   bool before,
                                   bool after) {
    if (!screen_) {
        action();
        return;
    }

    screen_->WithRestoredIO([&] {
        if (before) ClearTerminal();
        action();
        if (after) ClearTerminal();
    })();
}

void TuxDockApp::ActionListContainers() {
    if (containers_.empty()) {
        OpenMessage("Containers", "No containers found.");
        return;
    }
    OpenListMessage("Containers", FormatContainerList(containers_));
}

void TuxDockApp::ActionListImages() {
    if (images_.empty()) {
        OpenMessage("Images", "No images found.");
        return;
    }
    OpenListMessage("Images", FormatImageList(images_));
}
void TuxDockApp::ActionPullImage() {
    OpenInput(
        "Pull Docker Image", "Enter image name:",
        [this](bool ok, const std::string& image) {
            if (!ok) return;
            BeginBusyOperation(
                "Pulling image", "Please wait...",
                [this, image] {
                    std::string message;
                    docker_.pullImage(image, message);
                    return message;
                });
        });
}

void TuxDockApp::ActionRunContainer() {
    PromptImageSelection(
        "Create Container",
        [this](const std::string&, const std::string& tag) {
            auto context = std::make_shared<RunContainerContext>();
            context->image = tag;
            OpenInput(
                "Container Name", "Enter a name for the container:",
                [this, context](bool ok, const std::string& name) {
                    if (!ok) return;
                    if (!IsValidContainerName(name)) {
                        return SetStatus(
                            "Use letters, numbers, '.', '_', or '-' in the container name.");
                    }
                    context->name = name;
                    PromptPortCountAndRun(context);
                });
        });
}

void TuxDockApp::PromptPortCountAndRun(
    const std::shared_ptr<RunContainerContext>& context) {
    OpenInput(
        "Port Mappings", "How many port mappings? (0 for none)",
        [this, context](bool ok, const std::string& value) {
            if (!ok) return;
            if (!IsDigits(value)) return SetStatus("Please enter a valid number.");
            context->port_count = std::stoi(value);
            PromptNextPort(context, 0);
        });
}

void TuxDockApp::PromptNextPort(
    const std::shared_ptr<RunContainerContext>& context,
    int index) {
    if (index >= context->port_count) {
        BeginBusyOperation(
            "Creating container", "Please wait...",
            [this, context] {
                std::string message;
                docker_.createContainer(context->name, context->image,
                                        context->ports, message);
                return message;
            });
        return;
    }

    OpenInput(
        "Port Mapping", "Enter host-to-container port mapping #" + std::to_string(index + 1) + ". e.g: 8080:80.",
        [this, context, index](bool ok, const std::string& value) {
            if (!ok) return;
            if (!IsValidPortMapping(value)) {
                return SetStatus("Use host:container format.");
            }
            context->ports.push_back(value);
            PromptNextPort(context, index + 1);
        });
}

void TuxDockApp::ActionStartDetached() {
    PromptContainerSelection(
        "Start Detached",
        [this](const std::string& id, const std::string&) {
            BeginBusyOperation(
                "Starting container", "Please wait...",
                [this, id] {
                    std::string message;
                    docker_.startDetached(id, message);
                    return message;
                });
        });
}

void TuxDockApp::ActionDeleteImage() {
    PromptImageSelection(
        "Delete Image",
        [this](const std::string& id, const std::string& tag) {
            OpenConfirm(
                "Delete Image", "Delete image " + tag + "?",
                [this, id](bool ok) {
                    if (!ok) return;
                    BeginBusyOperation(
                        "Deleting image", "Please wait...",
                        [this, id] {
                            std::string message;
                            docker_.deleteImage(id, message);
                            return message;
                        });
                });
        });
}

void TuxDockApp::ActionStopContainer() {
    PromptContainerSelection(
        "Stop Container",
        [this](const std::string& id, const std::string& name) {
            OpenInput(
                "Stop Container",
                "Shutdown grace period in seconds for " + name +
                    " (default 10, max " + std::to_string(kMaxStopTimeout) + "):",
                [this, id](bool ok, const std::string& value) {
                    if (!ok) return;
                    int seconds = 10;
                    if (!value.empty()) {
                        try {
                            seconds = std::stoi(value);
                        } catch (...) {
                            seconds = 10;
                        }
                    }
                    if (seconds < 0) seconds = 10;
                    if (seconds > kMaxStopTimeout) seconds = kMaxStopTimeout;
                    BeginStopOperation(id, seconds);
                });
        });
}

void TuxDockApp::ActionRemoveContainer() {
    PromptContainerSelection(
        "Remove Container",
        [this](const std::string& id, const std::string& name) {
            OpenConfirm(
                "Remove Container", "Remove container " + name + "?",
                [this, id](bool ok) {
                    if (!ok) return;
                    BeginBusyOperation(
                        "Removing container", "Please wait...",
                        [this, id] {
                            std::string message;
                            docker_.removeContainer(id, message);
                            return message;
                        });
                });
        });
}

void TuxDockApp::ActionExecShell() {
    PromptContainerSelection(
        "Open Shell",
        [this](const std::string& id, const std::string&) {
            std::string message;
            RunWithRestoredIO(
                [this, &id, &message] { docker_.execShell(id, message); }, true, true);
            SetStatus(message);
        });
}

void TuxDockApp::ActionExecDetachedCommand() {
    PromptContainerSelection(
        "Run Detached Command",
        [this](const std::string& id, const std::string& name) {
            OpenInput(
                "Detached Command", "Enter command to run in " + name + ":",
                [this, id](bool ok, const std::string& command) {
                    if (!ok) return;
                    BeginBusyOperation(
                        "Running command", "Please wait...",
                        [this, id, command] {
                            std::string message;
                            docker_.execDetachedCommand(id, command, message);
                            return message;
                        });
                });
        });
}

void TuxDockApp::ActionAbout() {
    OpenMessage("About Tux-Dock", "Tux-Dock 0.3.1-beta | Created by markmental");
}

void TuxDockApp::ExecuteSelectedAction() {
    switch (menu_selected_) {
        case 0: ActionPullImage(); break;
        case 1: ActionRunContainer(); break;
        case 2: ActionListContainers(); break;
        case 3: ActionListImages(); break;
        case 4: ActionStartDetached(); break;
        case 5: ActionDeleteImage(); break;
        case 6: ActionStopContainer(); break;
        case 7: ActionRemoveContainer(); break;
        case 8: ActionExecShell(); break;
        case 9: ActionExecDetachedCommand(); break;
        case 10: ActionAbout(); break;
        case 11:
            if (screen_) screen_->ExitLoopClosure()();
            break;
        default: break;
    }
}

bool TuxDockApp::OnEvent(ftxui::Event event) {
    if (modal_mode_ == ModalMode::Busy) {
        if (event == ftxui::Event::Custom) ++spinner_frame_;
        return true;
    }
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
        if (event == ftxui::Event::Return || event == ftxui::Event::Character("y")) {
            ResolveConfirm(true);
            return true;
        }
        if (event == ftxui::Event::Escape || event == ftxui::Event::Character("n")) {
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
        body = vbox(Elements{
            paragraph(modal_text_),
            separator(),
            input_component_->Render() | border,
        });
        footer = text("Enter: confirm   Esc: cancel") | dim;
    } else if (modal_mode_ == ModalMode::Confirm) {
        body = paragraph(modal_text_);
        footer = text("Y/Enter: confirm   N/Esc: cancel") | dim;
    } else if (modal_mode_ == ModalMode::Select) {
        body = vbox(Elements{
            paragraph(modal_text_),
            separator(),
            select_component_->Render() | frame | vscroll_indicator,
        });
        footer = text("Up/Down: choose   Enter: confirm   Esc: cancel") | dim;
    } else if (modal_mode_ == ModalMode::Busy) {
        static const std::string spinner = "|/-\\";
        Elements busy_elements = Elements{
            text(operation_state_.message()),
            separator(),
            text(std::string("                 ") + spinner[spinner_frame_ % spinner.size()]) | bold,
        };
        if (g_sigint_trapped > 0) {
            busy_elements.push_back(text("Ctrl+C ignored: operation in progress") | dim);
        }
        if (g_sigtstp_trapped > 0) {
            busy_elements.push_back(text("Ctrl+Z ignored: operation in progress") | dim);
        }
        body = vbox(std::move(busy_elements));
        footer = text("Please wait; input is disabled") | dim;
    } else {
        body = (modal_content_ ? std::move(modal_content_) : paragraph(modal_text_)) |
               frame | vscroll_indicator |
               size(HEIGHT, LESS_THAN, 18) | size(WIDTH, LESS_THAN, 96);
        footer = text("Enter/Esc: close") | dim;
    }
    const std::string title = modal_mode_ == ModalMode::Busy
                                  ? operation_state_.title()
                                  : modal_title_;
    return window(text(title), vbox(Elements{body, separator(), footer})) |
           size(WIDTH, LESS_THAN, 96) | size(WIDTH, GREATER_THAN, 42) |
           size(HEIGHT, LESS_THAN, 24) | size(HEIGHT, GREATER_THAN, 10) | center;
}
ftxui::Element TuxDockApp::Render() const {
    using namespace ftxui;

    auto base = window(
        text("Actions"),
        vbox(Elements{
            menu_component_->Render() | frame | vscroll_indicator,
            separator(),
            text("Up/Down: navigate   Enter: select") | dim,
        })) |
                size(WIDTH, GREATER_THAN, 56) |
                size(WIDTH, LESS_THAN, 90) | center;

    return modal_mode_ == ModalMode::None
               ? base
               : dbox({base, RenderModal() | clear_under | center});
}
int TuxDockApp::Run() {
    std::string connection_error;
    if (!docker_.checkConnection(connection_error)) {
        std::cerr << "Unable to connect to Docker Engine.\n"
                  << "Socket: /var/run/docker.sock\n"
                  << "Reason: " << connection_error << "\n\n"
                  << "Ensure Docker is running and your user can access the Docker socket.\n";
        return 1;
    }

    const auto initial_containers = docker_.getContainerList();
    const auto initial_images = docker_.getImageList();
    if (initial_containers.ok()) containers_ = initial_containers.items;
    if (initial_images.ok()) images_ = initial_images.items;
    if (!initial_containers.ok() || !initial_images.ok()) {
        SetStatus("Docker connected, but initial state could not be fully loaded.");
    } else if (containers_.empty() && images_.empty()) {
        SetStatus("Docker connected. No containers or images found.");
    } else {
        SetStatus("Docker connected. State loaded.");
    }

    auto root = ftxui::Renderer(menu_component_, [this] { return Render(); });
    auto app = ftxui::CatchEvent(root, [this](ftxui::Event event) {
        return OnEvent(event);
    });
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    screen_ = &screen;
    screen.Loop(app);
    screen_ = nullptr;
    if (refresh_thread_.joinable()) refresh_thread_.join();
    if (spinner_thread_.joinable()) spinner_thread_.join();
    for (auto& thread : action_threads_) {
        if (thread.joinable()) thread.join();
    }
    return 0;
}

int main() {
    TuxDockApp app;
    return app.Run();
}
