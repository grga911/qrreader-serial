// cppcheck-suppress-begin missingIncludeSystem
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
// cppcheck-suppress-end missingIncludeSystem

#include "process_io.h"

namespace {

constexpr std::string_view kDefaultPort = "/dev/ttyACM0";

std::string currentTimestamp() {
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t nowTime = Clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmNow{};
    localtime_r(&nowTime, &tmNow);

    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

std::string escapeForLog(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

void debugLog(const std::string& event, const std::string& value) {
    std::cerr << "[" << currentTimestamp() << "] "
              << event << "=\"" << escapeForLog(value) << "\"\n";
}

std::string trimTrailingNewlinesCrLf(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end - 1] == '\n' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(0, end);
}

std::string normalizeClipboardText(const std::string& s) {
    std::string t = s;
    for (size_t pos = 0;;) {
        pos = t.find("\r\n", pos);
        if (pos == std::string::npos) {
            break;
        }
        t.replace(pos, 2, "\n");
        pos += 1;
    }
    return trimTrailingNewlinesCrLf(t);
}

bool waitClipboardMatchesExpected(const std::string& expected, std::chrono::milliseconds timeout,
                                  std::chrono::milliseconds poll) {
    const std::string want = normalizeClipboardText(expected);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const std::string cur = qrreader::readClipboard();
        if (normalizeClipboardText(cur) == want) {
            return true;
        }
        std::this_thread::sleep_for(poll);
    }
    return false;
}

bool looksLikeScanPayload(const std::string& s) {
    const std::string t = normalizeClipboardText(s);
    if (t.empty()) {
        return false;
    }
    if (t.rfind("K>PR", 0) == 0 || t.rfind("K:PR", 0) == 0) {
        return true;
    }
    if (t.find("#N>") != std::string::npos &&
        (t.find("#I>") != std::string::npos || t.find("#R>") != std::string::npos)) {
        return true;
    }
    if (t.size() >= 8) {
        bool allDigits = true;
        for (char c : t) {
            if (c < '0' || c > '9') {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            return true;
        }
    }
    return false;
}

void restoreOrClearClipboard(const std::string& oldClipboard) {
    if (looksLikeScanPayload(oldClipboard) || trimTrailingNewlinesCrLf(oldClipboard).empty()) {
        qrreader::clearClipboard();
        return;
    }
    if (!qrreader::writeClipboard(oldClipboard)) {
        qrreader::clearClipboard();
    }
}

void finalizeClipboardAfterPaste(const std::string& oldClipboard) {
    // Target apps often read the clipboard asynchronously after Ctrl+V.
    // Restoring Old too soon pastes the previous scan (Merkur sticky bug).
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    restoreOrClearClipboard(oldClipboard);
}

void logScan(const std::string& oldClipboard, const std::string& copyResult) {
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t nowTime = Clock::to_time_t(now);
    std::tm tmNow{};
    localtime_r(&nowTime, &tmNow);
    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%H:%M:%S %d.%m.%Y");
    const std::string stamp = oss.str();
    std::cerr << stamp << " Old '" << oldClipboard << "'\n";
    std::cerr << stamp << " New '" << copyResult << "'\n";
}

bool isUtf8Continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

uint32_t cp1252ToCodePoint(unsigned char b) {
    static const std::unordered_map<unsigned char, uint32_t> cp1252Special = {
        {0x80, 0x20AC}, {0x82, 0x201A}, {0x83, 0x0192}, {0x84, 0x201E}, {0x85, 0x2026},
        {0x86, 0x2020}, {0x87, 0x2021}, {0x88, 0x02C6}, {0x89, 0x2030}, {0x8A, 0x0160},
        {0x8B, 0x2039}, {0x8C, 0x0152}, {0x8E, 0x017D}, {0x91, 0x2018}, {0x92, 0x2019},
        {0x93, 0x201C}, {0x94, 0x201D}, {0x95, 0x2022}, {0x96, 0x2013}, {0x97, 0x2014},
        {0x98, 0x02DC}, {0x99, 0x2122}, {0x9A, 0x0161}, {0x9B, 0x203A}, {0x9C, 0x0153},
        {0x9E, 0x017E}, {0x9F, 0x0178},
    };
    auto it = cp1252Special.find(b);
    if (it != cp1252Special.end()) {
        return it->second;
    }
    return static_cast<uint32_t>(b);
}

void appendUtf8(uint32_t cp, std::string& out) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string decodeMixedUtf8Cp1252(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (size_t i = 0; i < input.size();) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        if ((c & 0xE0) == 0xC0 && i + 1 < input.size()) {
            unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
            if (isUtf8Continuation(c1)) {
                out.push_back(static_cast<char>(c));
                out.push_back(static_cast<char>(c1));
                i += 2;
                continue;
            }
        } else if ((c & 0xF0) == 0xE0 && i + 2 < input.size()) {
            unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
            if (isUtf8Continuation(c1) && isUtf8Continuation(c2)) {
                out.push_back(static_cast<char>(c));
                out.push_back(static_cast<char>(c1));
                out.push_back(static_cast<char>(c2));
                i += 3;
                continue;
            }
        } else if ((c & 0xF8) == 0xF0 && i + 3 < input.size()) {
            unsigned char c1 = static_cast<unsigned char>(input[i + 1]);
            unsigned char c2 = static_cast<unsigned char>(input[i + 2]);
            unsigned char c3 = static_cast<unsigned char>(input[i + 3]);
            if (isUtf8Continuation(c1) && isUtf8Continuation(c2) && isUtf8Continuation(c3)) {
                out.push_back(static_cast<char>(c));
                out.push_back(static_cast<char>(c1));
                out.push_back(static_cast<char>(c2));
                out.push_back(static_cast<char>(c3));
                i += 4;
                continue;
            }
        }
        appendUtf8(cp1252ToCodePoint(c), out);
        ++i;
    }
    return out;
}

std::string replaceNewlinesWithHash(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            out.push_back('#');
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                ++i;
            }
        } else if (input[i] == '\n') {
            out.push_back('#');
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

std::string transliterateAndFilter(const std::string& utf8Text) {
    static const std::unordered_map<std::string, std::string> map = {
        {"А","A"},{"а","a"},{"Б","B"},{"б","b"},{"В","V"},{"в","v"},{"Г","G"},{"г","g"},{"Д","D"},{"д","d"},
        {"Ђ","Đ"},{"ђ","đ"},{"Е","E"},{"е","e"},{"Ж","Ž"},{"ж","ž"},{"З","Z"},{"з","z"},{"И","I"},{"и","i"},
        {"Ј","J"},{"ј","j"},{"К","K"},{"к","k"},{"Л","L"},{"л","l"},{"Љ","Lj"},{"љ","lj"},{"М","M"},{"м","m"},
        {"Н","N"},{"н","n"},{"Њ","Nj"},{"њ","nj"},{"О","O"},{"о","o"},{"П","P"},{"п","p"},{"Р","R"},{"р","r"},
        {"С","S"},{"с","s"},{"Т","T"},{"т","t"},{"Ћ","Ć"},{"ћ","ć"},{"У","U"},{"у","u"},{"Ф","F"},{"ф","f"},
        {"Х","H"},{"х","h"},{"Ц","C"},{"ц","c"},{"Ч","Č"},{"ч","č"},{"Џ","Dž"},{"џ","dž"},{"Ш","Š"},{"ш","š"},
        {":",">"},{"|","#"}
    };

    std::string out;
    for (size_t i = 0; i < utf8Text.size();) {
        unsigned char c = static_cast<unsigned char>(utf8Text[i]);
        size_t len = 1;
        if ((c & 0xE0) == 0xC0 && i + 1 < utf8Text.size()) len = 2;
        else if ((c & 0xF0) == 0xE0 && i + 2 < utf8Text.size()) len = 3;
        else if ((c & 0xF8) == 0xF0 && i + 3 < utf8Text.size()) len = 4;

        std::string ch = utf8Text.substr(i, len);

        bool isControl = (len == 1) &&
            ((static_cast<unsigned char>(ch[0]) <= 0x08) ||
             ch[0] == '\t' ||
             (static_cast<unsigned char>(ch[0]) >= 0x0B && static_cast<unsigned char>(ch[0]) <= 0x0C) ||
             (static_cast<unsigned char>(ch[0]) >= 0x0E && static_cast<unsigned char>(ch[0]) <= 0x1F) ||
             (static_cast<unsigned char>(ch[0]) >= 0x7F && static_cast<unsigned char>(ch[0]) <= 0x9F));
        if (!isControl) {
            auto it = map.find(ch);
            out += (it != map.end()) ? it->second : ch;
        }
        i += len;
    }

    size_t start = out.find_first_not_of(" \n\r\t");
    if (start == std::string::npos) return "";
    size_t end = out.find_last_not_of(" \n\r\t");
    return out.substr(start, end - start + 1);
}

int openSerialPort(const std::string& port) {
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return -1;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag = 0;
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }

    // Discard unread input from the driver (stale data after reconnect or boot).
    tcflush(fd, TCIFLUSH);

    return fd;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string port = (argc >= 2) ? argv[1] : std::string(kDefaultPort);

    while (true) {
        std::cerr << "serial_connecting " << port << "\n";
        int fd = openSerialPort(port);
        if (fd < 0) {
            std::cerr << "serial_open_failed " << port << ": " << std::strerror(errno) << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        std::cerr << "serial_connected " << port << "\n";

        while (true) {
            int waiting = 0;
            if (ioctl(fd, FIONREAD, &waiting) != 0) {
                std::cerr << "serial_disconnected " << port << ": " << std::strerror(errno) << "\n";
                break;
            }
            if (waiting <= 4) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            // Read serial before clipboard: xclip can block; bytes could arrive meanwhile.
            std::string raw = qrreader::readAvailableData(fd);
            std::string oldClipboard = qrreader::readClipboard();
            std::string normalized = replaceNewlinesWithHash(raw);
            std::string decoded = decodeMixedUtf8Cp1252(normalized);
            std::string finalText = transliterateAndFilter(decoded);
            logScan(oldClipboard, finalText);
            debugLog("clipboard_before", oldClipboard);
            debugLog("pasted_text", finalText);

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            bool settled = false;
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (!qrreader::writeClipboard(finalText)) {
                    std::cerr << "Failed to write clipboard. Install xclip.\n";
                    break;
                }
                if (waitClipboardMatchesExpected(finalText, std::chrono::milliseconds(1000),
                                                 std::chrono::milliseconds(30))) {
                    settled = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (!settled) {
                std::cerr << "qrreader_linux: clipboard did not update to scan text; skipping paste.\n";
                restoreOrClearClipboard(oldClipboard);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            if (!waitClipboardMatchesExpected(finalText, std::chrono::milliseconds(500),
                                              std::chrono::milliseconds(30))) {
                std::cerr << "qrreader_linux: clipboard no longer matches scan text before paste; "
                             "skipping paste.\n";
                restoreOrClearClipboard(oldClipboard);
                continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            qrreader::simulateCtrlV();
            finalizeClipboardAfterPaste(oldClipboard);
            debugLog("clipboard_finalized", oldClipboard);
        }

        close(fd);
        std::cerr << "serial_reconnecting " << port << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
