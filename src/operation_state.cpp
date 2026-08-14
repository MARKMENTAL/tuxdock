#include "operation_state.hpp"

#include <utility>

void OperationState::begin(std::string title, std::string message) {
    busy_ = true;
    title_ = std::move(title);
    message_ = std::move(message);
    result_.clear();
}

void OperationState::complete(std::string result) {
    busy_ = false;
    result_ = std::move(result);
}
