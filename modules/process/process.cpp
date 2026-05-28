#include "process/process.h"
#include "path/path.h"
#include <tlhelp32.h>

namespace {

std::wstring GetProcessImagePath(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE,
                                 FALSE, pid);
    if (!process) return L"";

    std::wstring path(MAX_PATH, L'\0');
    while (true) {
        DWORD size = static_cast<DWORD>(path.size());
        if (QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
            path.resize(size);
            CloseHandle(process);
            return path;
        }

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            CloseHandle(process);
            return L"";
        }

        path.resize(path.size() * 2);
    }
}

}

namespace proc {

void CloseOldInstances() {
    const std::wstring currentPath = path::GetExePath();
    if (currentPath.empty()) return;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return;
    }

    const DWORD currentPid = GetCurrentProcessId();
    do {
        if (entry.th32ProcessID == currentPid) continue;

        std::wstring processPath = GetProcessImagePath(entry.th32ProcessID);
        if (processPath.empty()) continue;

        if (CompareStringOrdinal(processPath.c_str(), -1, currentPath.c_str(), -1, TRUE) != CSTR_EQUAL)
            continue;

        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (!process) continue;

        TerminateProcess(process, 0);
        WaitForSingleObject(process, INFINITE);
        CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));

    CloseHandle(snapshot);
}

bool RunHiddenAndWait(const std::wstring& commandLine, DWORD* exitCode) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = commandLine;

    BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exitCode) *exitCode = code;
    return true;
}

}
