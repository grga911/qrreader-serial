#include "process_io.h"

// cppcheck-suppress-begin missingIncludeSystem
#include <array>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
// cppcheck-suppress-end missingIncludeSystem

namespace qrreader {

namespace {

constexpr size_t kReadChunkSize = 4096;

int openDevNullWrite() {
    return open("/dev/null", O_WRONLY);
}

std::string readFdOnce(int fd) {
    int waiting = 0;
    if (ioctl(fd, FIONREAD, &waiting) == 0 && waiting > 0) {
        std::string data(static_cast<size_t>(waiting), '\0');
        const ssize_t n = read(fd, data.data(), data.size());
        if (n > 0) {
            data.resize(static_cast<size_t>(n));
            return data;
        }
        return "";
    }

    std::array<char, kReadChunkSize> buffer{};
    const ssize_t n = read(fd, buffer.data(), buffer.size());
    if (n <= 0) {
        return "";
    }
    return std::string(buffer.data(), static_cast<size_t>(n));
}

}  // namespace

std::string readPipeOutput(int fd) {
    return readFdOnce(fd);
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
    return readFdOnce(fd);
}

}  // namespace qrreader
