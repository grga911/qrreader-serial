#include "qrreader_win_common.hpp"

#include <iostream>
#include <string>

#include <wtsapi32.h>

namespace {

SERVICE_STATUS g_status{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;

void setServiceState(DWORD state, DWORD win32Exit = NO_ERROR) {
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = win32Exit;
    SetServiceStatus(g_statusHandle, &g_status);
}

void WINAPI ServiceCtrlHandler(DWORD control) {
    switch (control) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            setServiceState(SERVICE_STOP_PENDING);
            qrreader::requestTrayExit();
            SetEvent(g_stopEvent);
            break;
        default:
            break;
    }
}

void WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_statusHandle = RegisterServiceCtrlHandlerW(qrreader::kServiceName, ServiceCtrlHandler);
    if (!g_statusHandle) {
        return;
    }

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_status.dwWin32ExitCode = NO_ERROR;
    g_status.dwServiceSpecificExitCode = 0;

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    setServiceState(SERVICE_START_PENDING);

    qrreader::setConsoleLogging(false);
    qrreader::logLine("Service starting");

    setServiceState(SERVICE_RUNNING);
    qrreader::launchTrayInActiveSession();

    while (WaitForSingleObject(g_stopEvent, 30000) == WAIT_TIMEOUT) {
        if (!qrreader::isTrayProcessRunning()) {
            qrreader::launchTrayInActiveSession();
        }
    }

    qrreader::requestTrayExit();
    setServiceState(SERVICE_STOPPED);
}

bool runScCommand(const wchar_t* command) {
    wchar_t cmdLine[1024] = {};
    swprintf_s(cmdLine, L"sc %s", command);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

bool installService() {
    const std::wstring servicePath = qrreader::getSiblingExecutablePath(L"qrreader_service.exe");
    const std::wstring binPath = L"\"" + servicePath + L"\"";

    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        return false;
    }

    SC_HANDLE svc = CreateServiceW(
        scm, qrreader::kServiceName, qrreader::kServiceDisplayName, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        binPath.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return true;
}

bool uninstallService() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        return false;
    }
    SC_HANDLE svc = OpenServiceW(scm, qrreader::kServiceName, DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status{};
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    const BOOL deleted = DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return deleted != 0;
}

void printUsage() {
    std::wcerr << L"QR Reader Windows service\n"
              << L"  qrreader_service.exe --install       Install and start (admin)\n"
              << L"  qrreader_service.exe --uninstall     Remove service (admin)\n"
              << L"  qrreader_service.exe --start-tray    Start tray in user session\n";
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc >= 2) {
        const std::wstring arg = argv[1];
        if (arg == L"--install") {
            if (!installService()) {
                std::wcerr << L"Install failed. Run as Administrator.\n";
                return 1;
            }
            runScCommand(L"start QrReaderService");
            std::wcout << L"Service installed and started.\n";
            return 0;
        }
        if (arg == L"--uninstall") {
            runScCommand(L"stop QrReaderService");
            if (!uninstallService()) {
                std::wcerr << L"Uninstall failed. Run as Administrator.\n";
                return 1;
            }
            std::wcout << L"Service removed.\n";
            return 0;
        }
        if (arg == L"--start-tray") {
            return qrreader::launchTrayInActiveSession() ? 0 : 1;
        }
        printUsage();
        return 1;
    }

    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<LPWSTR>(qrreader::kServiceName), ServiceMain},
        {nullptr, nullptr},
    };
    if (!StartServiceCtrlDispatcherW(table)) {
        printUsage();
        return static_cast<int>(GetLastError());
    }
    return 0;
}
