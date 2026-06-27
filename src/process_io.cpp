#include "process_io.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace qrreader {

namespace {

int openDevNullWrite() {
    return open("/dev/null", O_WRONLY);
}

}  // namespace

std::string readPipeOutput(int fd) {
    std::string output;
    std::array<char, 1024> buffer{};
    const size_t bufSize = buffer.size();
    while (true) {
        const ssize_t n = read(fd, buffer.data(), bufSize);
        if (n > 0) {
            const size_t count = static_cast<size_t>(n);
            if (count <= bufSize) {
                output.append(buffer.data(), count);
            }
            continue;
        }
        break;
    }
    return output;
}

bool spawnWithStdout(char* const argv[], std::string& output) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return false;
    }

    const pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        const int devnull = openDevNullWrite();
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    close(pipefd[1]);
    output = readPipeOutput(pipefd[0]);
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool spawnWithStdin(char* const argv[], const std::string& input) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        return false;
    }

    const pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[1]);
        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[0]);
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    close(pipefd[0]);
    if (!input.empty()) {
        size_t offset = 0;
        while (offset < input.size()) {
            const ssize_t written = write(
                pipefd[1], input.data() + offset, input.size() - offset);
            if (written <= 0) {
                break;
            }
            offset += static_cast<size_t>(written);
        }
    }
    close(pipefd[1]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool spawnSimple(char* const argv[]) {
    const pid_t pid = fork();
    if (pid == 0) {
        const int devnull = openDevNullWrite();
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid < 0) {
        return false;
    }

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::string readClipboard(const ExternalTools& tools) {
    char* argv[] = {
        const_cast<char*>(tools.xclip),
        const_cast<char*>("-selection"),
        const_cast<char*>("clipboard"),
        const_cast<char*>("-o"),
        nullptr,
    };
    std::string output;
    spawnWithStdout(argv, output);
    return output;
}

bool writeClipboard(const std::string& text, const ExternalTools& tools) {
    char* argv[] = {
        const_cast<char*>(tools.xclip),
        const_cast<char*>("-selection"),
        const_cast<char*>("clipboard"),
        nullptr,
    };
    return spawnWithStdin(argv, text);
}

bool simulateCtrlV(const ExternalTools& tools) {
    char* argv[] = {
        const_cast<char*>(tools.xdotool),
        const_cast<char*>("key"),
        const_cast<char*>("ctrl+v"),
        nullptr,
    };
    return spawnSimple(argv);
}

std::string readAvailableData(int fd) {
    std::string data;
    std::array<char, 1024> buffer{};
    while (true) {
        const size_t bufSize = buffer.size();
        const ssize_t n = read(fd, buffer.data(), bufSize);
        if (n > 0) {
            const size_t count = static_cast<size_t>(n);
            if (count <= bufSize) {
                data.append(buffer.data(), count);
            }
            continue;
        }
        if (n == 0 || (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
            break;
        }
        break;
    }
    return data;
}

}  // namespace qrreader
