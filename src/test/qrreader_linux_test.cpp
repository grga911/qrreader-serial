#include "process_io.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expectTrue(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectEq(const std::string& actual, const std::string& expected, const char* message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << '\n';
        std::cerr << "  expected: [" << expected << "]\n";
        std::cerr << "  actual:   [" << actual << "]\n";
        ++failures;
    }
}

std::string fixturePath(const char* name) {
    const char* root = std::getenv("QRREADER_TEST_FIXTURES");
    if (root == nullptr || root[0] == '\0') {
        throw std::runtime_error("QRREADER_TEST_FIXTURES is not set");
    }
    return std::string(root) + "/" + name;
}

std::string tempPath(const char* prefix) {
    std::vector<char> path(256);
    std::snprintf(path.data(), path.size(), "/tmp/%s-XXXXXX", prefix);
    const int fd = mkstemp(path.data());
    if (fd < 0) {
        throw std::runtime_error("mkstemp failed");
    }
    close(fd);
    return std::string(path.data());
}

struct MockToolPaths {
    std::string xclipPath;
    std::string xdotoolPath;

    qrreader::ExternalTools tools() const {
        qrreader::ExternalTools tools;
        tools.xclip = xclipPath.c_str();
        tools.xdotool = xdotoolPath.c_str();
        return tools;
    }
};

MockToolPaths mockTools() {
    MockToolPaths mock;
    mock.xclipPath = fixturePath("mock-xclip.sh");
    mock.xdotoolPath = fixturePath("mock-xdotool.sh");
    return mock;
}

void testReadClipboardCapturesStdout() {
    setenv("MOCK_XCLIP_IN", "hello-from-clipboard", 1);
    unsetenv("MOCK_XCLIP_IN_FILE");
    setenv("MOCK_XCLIP_READ_EXIT", "0", 1);

    const auto mock = mockTools();
    const auto tools = mock.tools();
    const std::string output = qrreader::readClipboard(tools);
    expectEq(output, "hello-from-clipboard", "readClipboard captures stdout from mock xclip");
}

void testWriteClipboardPassesStdin() {
    const std::string outFile = tempPath("qrreader-xclip-out");
    setenv("MOCK_XCLIP_OUT_FILE", outFile.c_str(), 1);
    setenv("MOCK_XCLIP_WRITE_EXIT", "0", 1);

    const auto mock = mockTools();
    const auto tools = mock.tools();
    const bool ok = qrreader::writeClipboard("scan-text-123", tools);
    expectTrue(ok, "writeClipboard returns true for successful mock xclip");

    std::ifstream in(outFile);
    std::ostringstream captured;
    captured << in.rdbuf();
    expectEq(captured.str(), "scan-text-123", "writeClipboard passes stdin to mock xclip");

    std::remove(outFile.c_str());
}

void testSimulateCtrlVInvokesXdotool() {
    const std::string logFile = tempPath("qrreader-xdotool-log");
    setenv("MOCK_XDOTOOL_LOG", logFile.c_str(), 1);
    setenv("MOCK_XDOTOOL_EXIT", "0", 1);

    const auto mock = mockTools();
    const auto tools = mock.tools();
    const bool ok = qrreader::simulateCtrlV(tools);
    expectTrue(ok, "simulateCtrlV returns true for successful mock xdotool");

    std::ifstream in(logFile);
    std::ostringstream captured;
    captured << in.rdbuf();
    std::string logged = captured.str();
    while (!logged.empty() && (logged.back() == '\n' || logged.back() == '\r')) {
        logged.pop_back();
    }
    expectTrue(logged.find("key") != std::string::npos, "simulateCtrlV log contains key");
    expectTrue(logged.find("ctrl+v") != std::string::npos, "simulateCtrlV log contains ctrl+v");

    std::remove(logFile.c_str());
}

void testReadAvailableDataRespectsBufferLimit() {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        throw std::runtime_error("pipe failed");
    }

    std::string payload;
    payload.reserve(2500);
    for (int i = 0; i < 2500; ++i) {
        payload.push_back(static_cast<char>('A' + (i % 26)));
    }

    if (write(pipefd[1], payload.data(), payload.size()) < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("write to pipe failed");
    }
    close(pipefd[1]);

    const std::string readBack = qrreader::readAvailableData(pipefd[0]);
    close(pipefd[0]);
    expectEq(readBack, payload, "readAvailableData reads multi-chunk serial input");
    expectTrue(readBack.size() > 1024, "readAvailableData reads more than one 1024-byte buffer");
}

void testReadAvailableDataHandlesEagain() {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        throw std::runtime_error("pipe failed");
    }

    const char chunk[] = "partial";
    if (write(pipefd[1], chunk, sizeof(chunk) - 1) < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("write to pipe failed");
    }
    close(pipefd[1]);

    const int flags = fcntl(pipefd[0], F_GETFL);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    const std::string readBack = qrreader::readAvailableData(pipefd[0]);
    close(pipefd[0]);
    expectEq(readBack, "partial", "readAvailableData stops cleanly on EAGAIN");
}

void testSpawnWithStdoutExecFailure() {
    char* argv[] = {
        const_cast<char*>(fixturePath("does-not-exist.sh").c_str()),
        nullptr,
    };
    std::string output;
    const bool ok = qrreader::spawnWithStdout(argv, output);
    expectTrue(!ok, "spawnWithStdout returns false when exec fails");
    expectTrue(output.empty(), "spawnWithStdout leaves output empty when exec fails");
}

void testSpawnWithStdoutNonZeroExit() {
    char* argv[] = {
        const_cast<char*>("/bin/sh"),
        const_cast<char*>(fixturePath("mock-fail.sh").c_str()),
        nullptr,
    };
    std::string output;
    const bool ok = qrreader::spawnWithStdout(argv, output);
    expectTrue(!ok, "spawnWithStdout returns false when child exits non-zero");
}

void testSpawnWithStdinWriteFailurePath() {
    char* argv[] = {
        const_cast<char*>(fixturePath("does-not-exist.sh").c_str()),
        nullptr,
    };
    const bool ok = qrreader::spawnWithStdin(argv, "payload");
    expectTrue(!ok, "spawnWithStdin returns false when exec fails");
}

void testSimulateCtrlVFailurePath() {
    const std::string logFile = tempPath("qrreader-xdotool-fail-log");
    setenv("MOCK_XDOTOOL_LOG", logFile.c_str(), 1);
    setenv("MOCK_XDOTOOL_EXIT", "1", 1);

    const auto mock = mockTools();
    const auto tools = mock.tools();
    const bool ok = qrreader::simulateCtrlV(tools);
    expectTrue(!ok, "simulateCtrlV returns false when mock xdotool exits non-zero");

    std::remove(logFile.c_str());
}

}  // namespace

int main() {
    try {
        testReadClipboardCapturesStdout();
        testWriteClipboardPassesStdin();
        testSimulateCtrlVInvokesXdotool();
        testReadAvailableDataRespectsBufferLimit();
        testReadAvailableDataHandlesEagain();
        testSpawnWithStdoutExecFailure();
        testSpawnWithStdoutNonZeroExit();
        testSpawnWithStdinWriteFailurePath();
        testSimulateCtrlVFailurePath();
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << '\n';
        return 1;
    }

    if (failures == 0) {
        std::cout << "All qrreader_linux tests passed\n";
        return 0;
    }

    std::cerr << failures << " qrreader_linux test(s) failed\n";
    return 1;
}
