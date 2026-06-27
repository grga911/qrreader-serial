#pragma once

#include <string>

namespace qrreader {

struct ExternalTools {
    const char* xclip = "xclip";
    const char* xdotool = "xdotool";
};

std::string readPipeOutput(int fd);

bool spawnWithStdout(char* const argv[], std::string& output);
bool spawnWithStdin(char* const argv[], const std::string& input);
bool spawnSimple(char* const argv[]);

std::string readClipboard(const ExternalTools& tools = ExternalTools{});
bool writeClipboard(const std::string& text, const ExternalTools& tools = ExternalTools{});
bool simulateCtrlV(const ExternalTools& tools = ExternalTools{});

std::string readAvailableData(int fd);

}  // namespace qrreader
