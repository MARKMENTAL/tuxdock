#include "container_parser.hpp"

#include <nlohmann/json.hpp>

#include <sstream>

using json = nlohmann::json;

namespace {

std::string optionalString(const json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

std::string portsFor(const json& item) {
    const auto it = item.find("Ports");
    if (it == item.end() || !it->is_array()) return {};
    std::ostringstream result;
    bool first = true;
    for (const auto& port : *it) {
        if (!port.is_object()) continue;
        const auto public_it = port.find("PublicPort");
        const auto private_it = port.find("PrivatePort");
        if (public_it == port.end() || private_it == port.end() ||
            !public_it->is_number() || !private_it->is_number()) continue;
        if (!first) result << ", ";
        result << *public_it << ":" << *private_it;
        first = false;
    }
    return result.str();
}

}  // namespace

DockerManager::ListResult<DockerManager::ContainerInfo>
parseContainerList(const std::string& body) {
    DockerManager::ListResult<DockerManager::ContainerInfo> result;
    try {
        const auto parsed = json::parse(body);
        if (!parsed.is_array()) {
            result.error = "Container response is not an array.";
            return result;
        }
        for (const auto& item : parsed) {
            if (!item.is_object()) continue;
            DockerManager::ContainerInfo info;
            info.id = optionalString(item, "Id");
            const auto names = item.find("Names");
            if (names != item.end() && names->is_array() && !names->empty() && names->front().is_string()) {
                info.name = names->front().get<std::string>();
            }
            if (!info.name.empty() && info.name.front() == '/') info.name.erase(0, 1);
            info.status = optionalString(item, "Status");
            info.running = optionalString(item, "State") == "running";
            info.ports = portsFor(item);
            if (!info.id.empty() && !info.name.empty()) result.items.push_back(std::move(info));
        }
    } catch (const json::exception& error) {
        result.error = std::string("Could not parse container data: ") + error.what();
    }
    return result;
}
