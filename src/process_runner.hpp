#pragma once

#include <string>
#include <vector>

struct ProcessResult {
    int exit_code = -1;
    bool signaled = false;
    int signal = 0;
    std::string stdout_text;
    std::string stderr_text;

    bool ok() const { return !signaled && exit_code == 0; }
};

struct ProcessOptions {
    bool capture_stdout = true;
    bool capture_stderr = true;
    bool inherit_stdio = false;
};

class ProcessRunner {
public:
    static ProcessResult run(const std::vector<std::string>& args,
                             const ProcessOptions& options = {});
};
