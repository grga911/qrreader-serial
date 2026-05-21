#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>

#include <atomic>
#include <string>

namespace qrreader {

constexpr const char* kDefaultPort = "COM21";
constexpr const wchar_t* kTrayWindowClass = L"QrReaderTrayWnd";
constexpr const wchar_t* kTrayMutexName = L"Global\\QrReaderTrayMutex_v1";
constexpr const wchar_t* kServiceName = L"QrReaderService";
constexpr const wchar_t* kServiceDisplayName = L"QR Reader Service";
constexpr const wchar_t* kTrayExeName = L"qrreader_tray.exe";

void setConsoleLogging(bool enabled);

std::wstring getDataDirectory();
std::wstring getLogFilePath();
std::wstring getPortConfigPath();
std::wstring getAutoReconnectConfigPath();

std::string getConfigPort();
bool saveConfigPort(const std::string& port);

void logLine(const std::string& line);
void logEvent(const std::string& event, const std::string& value);

bool getAutoReconnectEnabled();
bool setAutoReconnectEnabled(bool enabled);
void requestSerialReset();

void runReaderLoop(std::atomic<bool>& stop, const std::string& port);

std::wstring getModuleDirectory();
std::wstring getSiblingExecutablePath(const wchar_t* exeName);

bool isTrayProcessRunning();
bool launchTrayInActiveSession();

void setTrayNotifyWindow(HWND hwnd);
HWND getTrayNotifyWindow();
void requestTrayExit();

} // namespace qrreader
