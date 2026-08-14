#pragma once

#include "docker_manager.hpp"

#include <string>
#include <vector>

DockerManager::ListResult<DockerManager::ContainerInfo>
parseContainerList(const std::string& body);
