#pragma once

#include <string>

class OperationState {
public:
    void begin(std::string title, std::string message);
    void complete(std::string result);

    bool busy() const { return busy_; }
    const std::string& title() const { return title_; }
    const std::string& message() const { return message_; }
    const std::string& result() const { return result_; }

private:
    bool busy_ = false;
    std::string title_;
    std::string message_;
    std::string result_;
};
