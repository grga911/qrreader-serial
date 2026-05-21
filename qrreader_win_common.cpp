#include "qrreader_win_common.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

#include <shlobj.h>
#include <userenv.h>
#include <wtsapi32.h>

namespace qrreader {

void logLine(const std::string& line);

namespace {

std::mutex g_logMutex;
bool g_consoleLogging = false;
HWND g_trayHwnd = nullptr;
std::atomic<bool> g_serialResetRequested{false};

struct SerialPollResult {
    DWORD bytes = 0;
    bool ok = false;
    bool disconnected = false;
};

struct SerialReadResult {
    std::string data;
    bool disconnected = false;
};

std::string currentTimestamp() {
    using Clock = std::chrono::system_clock;
    const auto now = Clock::now();
    const std::time_t nowTime = Clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmNow{};
    localtime_s(&tmNow, &nowTime);

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

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &wide[0], size);
    return wide;
}

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
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

bool openClipboardWithRetry(DWORD attempts = 20) {
    for (DWORD i = 0; i < attempts; ++i) {
        if (OpenClipboard(nullptr)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::string getClipboardUtf8() {
    if (!openClipboardWithRetry()) {
        return {};
    }
    std::string result;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        const auto* text = static_cast<const wchar_t*>(GlobalLock(data));
        if (text) {
            result = wideToUtf8(text);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return result;
}

bool setClipboardUtf8(const std::string& text) {
    const std::wstring wide = utf8ToWide(text);
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) {
        return false;
    }
    void* locked = GlobalLock(mem);
    if (!locked) {
        GlobalFree(mem);
        return false;
    }
    std::memcpy(locked, wide.c_str(), bytes);
    GlobalUnlock(mem);

    if (!openClipboardWithRetry()) {
        GlobalFree(mem);
        return false;
    }
    EmptyClipboard();
    const BOOL ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
    CloseClipboard();
    if (!ok) {
        GlobalFree(mem);
        return false;
    }
    return true;
}

bool waitClipboardMatchesExpected(const std::string& expected, std::chrono::milliseconds timeout,
                                  std::chrono::milliseconds poll) {
    const std::string want = normalizeClipboardText(expected);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (normalizeClipboardText(getClipboardUtf8()) == want) {
            return true;
        }
        std::this_thread::sleep_for(poll);
    }
    return false;
}

void sendCtrlV() {
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
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

std::string formatComPortPath(const std::string& port) {
    if (port.rfind(R"(\\.\)", 0) == 0) {
        return port;
    }
    if (port.size() >= 3 && (port[0] == 'C' || port[0] == 'c') &&
        (port[1] == 'O' || port[1] == 'o') &&
        (port[2] == 'M' || port[2] == 'm')) {
        return R"(\\.\)" + port;
    }
    return port;
}

HANDLE openSerialPort(const std::string& port) {
    const std::string path = formatComPortPath(port);
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;

    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(handle, &timeouts);

    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return handle;
}

SerialPollResult pollSerialPort(HANDLE handle) {
    SerialPollResult result{};
    DWORD errors = 0;
    COMSTAT status{};
    if (!ClearCommError(handle, &errors, &status)) {
        result.disconnected = true;
        return result;
    }
    if (errors != 0) {
        logLine("serial_comm_error flags=" + std::to_string(errors));
    }
    result.bytes = status.cbInQue;
    result.ok = true;
    return result;
}

DWORD serialBytesWaiting(HANDLE handle) {
    return pollSerialPort(handle).bytes;
}

bool isSerialDisconnectError(DWORD err) {
    return err == ERROR_INVALID_HANDLE || err == ERROR_OPERATION_ABORTED ||
           err == ERROR_GEN_FAILURE || err == ERROR_DEVICE_NOT_CONNECTED ||
           err == ERROR_FILE_NOT_FOUND || err == ERROR_ACCESS_DENIED;
}

SerialReadResult readAvailableDataEx(HANDLE handle) {
    SerialReadResult result;
    std::array<char, 1024> buffer{};
    while (true) {
        DWORD read = 0;
        const BOOL ok = ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (!ok) {
            const DWORD err = GetLastError();
            if (isSerialDisconnectError(err)) {
                result.disconnected = true;
            }
            break;
        }
        if (read == 0) {
            break;
        }
        result.data.append(buffer.data(), read);
    }
    return result;
}

std::string readAvailableData(HANDLE handle) {
    return readAvailableDataEx(handle).data;
}

void flushSerialBuffers(HANDLE handle) {
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

bool ensureDirectoryExists(const std::wstring& dir) {
    if (dir.empty()) {
        return false;
    }
    const DWORD attrs = GetFileAttributesW(dir.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryW(dir.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

} // namespace

void setConsoleLogging(bool enabled) {
    g_consoleLogging = enabled;
}

std::wstring getDataDirectory() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) {
        return L".";
    }
    std::wstring dir = std::wstring(path) + L"\\QRReader";
    ensureDirectoryExists(dir);
    return dir;
}

std::wstring getLogFilePath() {
    return getDataDirectory() + L"\\qrreader.log";
}

std::wstring getPortConfigPath() {
    return getDataDirectory() + L"\\port.txt";
}

std::wstring getAutoReconnectConfigPath() {
    return getDataDirectory() + L"\\auto_reconnect.txt";
}

bool readBoolConfigFile(const std::wstring& path, bool defaultValue) {
    std::ifstream in(path);
    if (!in) {
        return defaultValue;
    }
    std::string value;
    in >> value;
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return defaultValue;
}

bool writeBoolConfigFile(const std::wstring& path, bool value) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << (value ? "1" : "0");
    return true;
}

std::string getConfigPort() {
    const std::wstring path = getPortConfigPath();
    std::ifstream in(path);
    if (!in) {
        return kDefaultPort;
    }
    std::string port;
    std::getline(in, port);
    while (!port.empty() && (port.back() == '\r' || port.back() == '\n' || port.back() == ' ')) {
        port.pop_back();
    }
    if (port.size() < 3) {
        return kDefaultPort;
    }
    return port;
}

bool saveConfigPort(const std::string& port) {
    const std::wstring path = getPortConfigPath();
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << port;
    return true;
}

bool getAutoReconnectEnabled() {
    return readBoolConfigFile(getAutoReconnectConfigPath(), true);
}

bool setAutoReconnectEnabled(bool enabled) {
    return writeBoolConfigFile(getAutoReconnectConfigPath(), enabled);
}

void requestSerialReset() {
    g_serialResetRequested.store(true);
    logLine("serial_reset_requested (manual)");
}

void logLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_consoleLogging) {
        std::cerr << line << '\n';
    }
    std::ofstream log(getLogFilePath(), std::ios::app);
    if (log) {
        log << line << '\n';
    }
}

void logEvent(const std::string& event, const std::string& value) {
    logLine("[" + currentTimestamp() + "] " + event + "=\"" + escapeForLog(value) + "\"");
}

std::wstring getModuleDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    const size_t slash = full.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return full;
    }
    return full.substr(0, slash);
}

std::wstring getSiblingExecutablePath(const wchar_t* exeName) {
    return getModuleDirectory() + L"\\" + exeName;
}

bool isTrayProcessRunning() {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, kTrayMutexName);
    if (!mutex) {
        return false;
    }
    CloseHandle(mutex);
    return true;
}

bool launchTrayInActiveSession() {
    if (isTrayProcessRunning()) {
        return true;
    }

    const std::wstring trayPath = getSiblingExecutablePath(kTrayExeName);
    if (GetFileAttributesW(trayPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        logLine("launchTray: missing " + wideToUtf8(trayPath));
        return false;
    }

    const DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId == 0xFFFFFFFF) {
        logLine("launchTray: no active console session");
        return false;
    }

    HANDLE userToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &userToken)) {
        logLine("launchTray: WTSQueryUserToken failed, error=" + std::to_string(GetLastError()));
        return false;
    }

    HANDLE primaryToken = nullptr;
    if (!DuplicateTokenEx(userToken, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation, TokenPrimary, &primaryToken)) {
        CloseHandle(userToken);
        logLine("launchTray: DuplicateTokenEx failed");
        return false;
    }
    CloseHandle(userToken);

    LPVOID environment = nullptr;
    CreateEnvironmentBlock(&environment, primaryToken, FALSE);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.lpDesktop = const_cast<wchar_t*>(L"winsta0\\default");
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + trayPath + L"\"";
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');
    const BOOL ok = CreateProcessAsUserW(
        primaryToken, nullptr, cmdBuffer.data(), nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, environment, nullptr, &si, &pi);

    if (environment) {
        DestroyEnvironmentBlock(environment);
    }
    CloseHandle(primaryToken);

    if (!ok) {
        logLine("launchTray: CreateProcessAsUser failed, error=" + std::to_string(GetLastError()));
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    logLine("launchTray: started tray in session " + std::to_string(sessionId));
    return true;
}

void setTrayNotifyWindow(HWND hwnd) {
    g_trayHwnd = hwnd;
}

HWND getTrayNotifyWindow() {
    return g_trayHwnd;
}

void requestTrayExit() {
    HWND hwnd = g_trayHwnd;
    if (!hwnd) {
        hwnd = FindWindowW(kTrayWindowClass, nullptr);
    }
    if (hwnd) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
}

void runReaderLoop(std::atomic<bool>& stop, const std::string& port) {
    logLine("Reader started on port " + port);

    while (!stop.load()) {
        if (g_serialResetRequested.exchange(false)) {
            logLine("serial_reset: reconnect requested while port closed");
        }

        logLine("serial_connecting port=" + port);
        HANDLE serial = openSerialPort(port);
        if (serial == INVALID_HANDLE_VALUE) {
            logLine("serial_open_failed port=" + port + " error=" + std::to_string(GetLastError()));
            if (!getAutoReconnectEnabled()) {
                logLine("auto_reconnect disabled; waiting for manual reset");
                while (!stop.load() && !g_serialResetRequested.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                g_serialResetRequested.store(false);
                continue;
            }
            for (int i = 0; i < 10 && !stop.load() && !g_serialResetRequested.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }

        logLine("serial_connected port=" + port);

        while (!stop.load()) {
            if (g_serialResetRequested.exchange(false)) {
                logLine("serial_reset: closing port by request");
                break;
            }

            const SerialPollResult poll = pollSerialPort(serial);
            if (poll.disconnected) {
                logLine("serial_disconnected port=" + port);
                break;
            }

            if (poll.bytes <= 4) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            const SerialReadResult readResult = readAvailableDataEx(serial);
            if (readResult.disconnected) {
                logLine("serial_disconnected during read port=" + port);
                break;
            }

            flushSerialBuffers(serial);

            std::string oldClipboard = getClipboardUtf8();
            std::string normalized = replaceNewlinesWithHash(readResult.data);
            std::string decoded = decodeMixedUtf8Cp1252(normalized);
            std::string finalText = transliterateAndFilter(decoded);
            logEvent("clipboard_before", oldClipboard);
            logEvent("pasted_text", finalText);

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            bool settled = false;
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (!setClipboardUtf8(finalText)) {
                    logLine("Failed to write clipboard");
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
                logLine("Clipboard did not update to scan text; skipping paste");
                setClipboardUtf8(oldClipboard);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            sendCtrlV();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            setClipboardUtf8(oldClipboard);
            logEvent("clipboard_restored", oldClipboard);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        CloseHandle(serial);
        logLine("serial_closed port=" + port);

        if (stop.load()) {
            break;
        }

        if (!getAutoReconnectEnabled()) {
            logLine("auto_reconnect disabled; waiting for manual reset");
            while (!stop.load() && !g_serialResetRequested.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            g_serialResetRequested.store(false);
            continue;
        }

        logLine("serial_reconnecting port=" + port);
        for (int i = 0; i < 2 && !stop.load() && !g_serialResetRequested.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    logLine("Reader stopped");
}

} // namespace qrreader
