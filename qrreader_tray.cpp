#include "qrreader_win_common.hpp"

#include <shellapi.h>

#include <atomic>
#include <string>
#include <thread>
#include <resource.h>

namespace {

constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr UINT ID_TRAY_ICON = 1;
constexpr UINT IDM_OPEN_LOG = 1001;
constexpr UINT IDM_OPEN_CONFIG = 1002;
constexpr UINT IDM_RESET_SERIAL = 1003;
constexpr UINT IDM_TOGGLE_AUTO_RECONNECT = 1004;
constexpr UINT IDM_EXIT = 1005;

NOTIFYICONDATAW g_nid{};
std::atomic<bool> g_stop{false};
std::thread g_worker;
HANDLE g_trayMutex = nullptr;

void updateTrayTooltip(HWND) {
    const std::string port = qrreader::getConfigPort();
    wchar_t wport[32] = {};
    MultiByteToWideChar(CP_UTF8, 0, port.c_str(), -1, wport, 32);
    std::wstring tooltip = L"QR Reader - ";
    tooltip += wport;
    wcsncpy_s(g_nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void showContextMenu(HWND hwnd) {
    const bool autoReconnect = qrreader::getAutoReconnectEnabled();
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_OPEN_LOG, L"Open log file");
    AppendMenuW(menu, MF_STRING, IDM_OPEN_CONFIG, L"Open config folder");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_RESET_SERIAL, L"Reset serial connection");
    AppendMenuW(menu, MF_STRING, IDM_TOGGLE_AUTO_RECONNECT,
                autoReconnect ? L"Auto-reconnect: ON" : L"Auto-reconnect: OFF");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}

void stopWorker() {
    g_stop.store(true);
    if (g_worker.joinable()) {
        g_worker.join();
    }
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                showContextMenu(hwnd);
            } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                const std::wstring logPath = qrreader::getLogFilePath();
                ShellExecuteW(nullptr, L"open", L"notepad.exe", logPath.c_str(), nullptr, SW_SHOWNORMAL);
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_OPEN_LOG: {
                    const std::wstring logPath = qrreader::getLogFilePath();
                    ShellExecuteW(nullptr, L"open", L"notepad.exe", logPath.c_str(), nullptr, SW_SHOWNORMAL);
                    break;
                }
                case IDM_OPEN_CONFIG: {
                    const std::wstring dir = qrreader::getDataDirectory();
                    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    break;
                }
                case IDM_RESET_SERIAL:
                    qrreader::requestSerialReset();
                    MessageBoxW(hwnd, L"Serial connection reset requested.", L"QR Reader", MB_ICONINFORMATION);
                    break;
                case IDM_TOGGLE_AUTO_RECONNECT: {
                    const bool next = !qrreader::getAutoReconnectEnabled();
                    qrreader::setAutoReconnectEnabled(next);
                    qrreader::logLine(std::string("auto_reconnect ") + (next ? "enabled" : "disabled"));
                    if (!next) {
                        qrreader::requestSerialReset();
                    }
                    break;
                }
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;

        case WM_DESTROY:
            stopWorker();
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            if (g_trayMutex) {
                CloseHandle(g_trayMutex);
                g_trayMutex = nullptr;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int) {
    (void)cmdLine;

    g_trayMutex = CreateMutexW(nullptr, TRUE, qrreader::kTrayMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"QR Reader tray is already running.", L"QR Reader", MB_ICONINFORMATION);
        return 0;
    }

    qrreader::logLine("Tray application starting");
    if (GetFileAttributesW(qrreader::getPortConfigPath().c_str()) == INVALID_FILE_ATTRIBUTES) {
        qrreader::saveConfigPort(qrreader::kDefaultPort);
    }
    if (GetFileAttributesW(qrreader::getAutoReconnectConfigPath().c_str()) == INVALID_FILE_ATTRIBUTES) {
        qrreader::setAutoReconnectEnabled(true);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = qrreader::kTrayWindowClass;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, qrreader::kTrayWindowClass, L"QR Reader", 0, 0, 0, 0, 0,
                                nullptr, nullptr, instance, nullptr);
    qrreader::setTrayNotifyWindow(hwnd);

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = ID_TRAYICON;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;

    g_nid.hIcon = (HICON)LoadImage(
        instance,
        MAKEINTRESOURCE(ID_TRAYICON),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    );

    if (!g_nid.hIcon) {
        g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }
    updateTrayTooltip(hwnd);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    const std::string port = qrreader::getConfigPort();
    g_stop.store(false);
    g_worker = std::thread([port]() { qrreader::runReaderLoop(g_stop, port); });

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
