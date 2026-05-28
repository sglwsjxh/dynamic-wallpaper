#include "startup/startup.h"
#include "logs/log.h"
#include "path/path.h"
#include <windows.h>

namespace {

DWORD RunElevatedSchtasks(const std::wstring& args) {
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"schtasks.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        LOG_ERR << "RunElevatedSchtasks: ShellExecuteExW 失败, err=" << GetLastError();
        return 1;
    }
    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess);
    return exitCode;
}

}

namespace startup {

void EnsureTask() {
    const std::wstring exePath = path::GetExePath();
    if (exePath.empty()) return;

    std::wstring args = L"/Create /TN \"DynamicWallpaper\" /SC ONLOGON /DELAY 0000:15 "
                        L"/RL LIMITED /IT /F /TR \"" + exePath + L"\"";
    DWORD code = RunElevatedSchtasks(args);
    if (code == 0)
        LOG_INFO << "开机自启任务已注册";
    else
        LOG_WARN << "开机自启任务注册失败, exitCode=" << code;
}

void RemoveTask() {
    DWORD code = RunElevatedSchtasks(L"/Delete /TN \"DynamicWallpaper\" /F");
    if (code == 0)
        LOG_INFO << "开机自启任务已移除";
    else
        LOG_WARN << "开机自启任务移除失败, exitCode=" << code;
}

}
