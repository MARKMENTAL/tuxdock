#include "process_runner.hpp"

#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <poll.h>
namespace {

struct Pipe {
    int read = -1;
    int write = -1;

    ~Pipe() {
        if (read != -1) close(read);
        if (write != -1) close(write);
    }
};

}  // namespace

ProcessResult ProcessRunner::run(const std::vector<std::string>& args,
                                 const ProcessOptions& options) {
    ProcessResult result;
    if (args.empty()) {
        result.stderr_text = "No executable specified.";
        return result;
    }

    Pipe stdout_pipe;
    Pipe stderr_pipe;
    if (!options.inherit_stdio && options.capture_stdout) {
        int fds[2];
        if (pipe(fds) != 0) {
            result.stderr_text = std::strerror(errno);
            return result;
        }
        stdout_pipe.read = fds[0];
        stdout_pipe.write = fds[1];
    }
    if (!options.inherit_stdio && options.capture_stderr) {
        int fds[2];
        if (pipe(fds) != 0) {
            result.stderr_text = std::strerror(errno);
            return result;
        }
        stderr_pipe.read = fds[0];
        stderr_pipe.write = fds[1];
    }

    pid_t pid = fork();
    if (pid == -1) {
        result.stderr_text = std::strerror(errno);
        return result;
    }

    if (pid == 0) {
        if (!options.inherit_stdio && options.capture_stdout) {
            if (dup2(stdout_pipe.write, STDOUT_FILENO) == -1) _exit(127);
        }
        if (!options.inherit_stdio && options.capture_stderr) {
            if (dup2(stderr_pipe.write, STDERR_FILENO) == -1) _exit(127);
        }
        if (stdout_pipe.read != -1) close(stdout_pipe.read);
        if (stdout_pipe.write != -1) close(stdout_pipe.write);
        if (stderr_pipe.read != -1) close(stderr_pipe.read);
        if (stderr_pipe.write != -1) close(stderr_pipe.write);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    if (stdout_pipe.write != -1) close(stdout_pipe.write);
    stdout_pipe.write = -1;
    if (stderr_pipe.write != -1) close(stderr_pipe.write);
    stderr_pipe.write = -1;
    std::array<char, 4096> buffer{};
    pollfd poll_fds[2]{};
    int active = 0;
    if (stdout_pipe.read != -1) poll_fds[active++] = {stdout_pipe.read, POLLIN, 0};
    if (stderr_pipe.read != -1) poll_fds[active++] = {stderr_pipe.read, POLLIN, 0};
    while (active > 0) {
        if (poll(poll_fds, static_cast<nfds_t>(active), -1) == -1) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < active;) {
            if ((poll_fds[i].revents & (POLLIN | POLLHUP)) == 0) {
                ++i;
                continue;
            }
            const ssize_t count = read(poll_fds[i].fd, buffer.data(), buffer.size());
            if (count > 0) {
                if (poll_fds[i].fd == stdout_pipe.read) result.stdout_text.append(buffer.data(), static_cast<std::size_t>(count));
                else result.stderr_text.append(buffer.data(), static_cast<std::size_t>(count));
                ++i;
            } else {
                close(poll_fds[i].fd);
                poll_fds[i] = poll_fds[--active];
            }
        }
    }

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        result.stderr_text = std::strerror(errno);
        return result;
    }
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signaled = true;
        result.signal = WTERMSIG(status);
    }
    return result;
}
