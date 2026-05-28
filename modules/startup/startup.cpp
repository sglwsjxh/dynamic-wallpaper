#include "startup/startup.h"
#include "logs/log.h"
#include "path/path.h"
#include <windows.h>

namespace {

bool RunElevatedSchtasks(const std::wstring& args) {
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"schtasks.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return false;
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    return true;
}

}

namespace startup {

void EnsureTask() {
    const std::wstring exePath = path::GetExePath();
    if (exePath.empty()) return;

    std::wstring args = L"/Create /TN \"DynamicWallpaper\" /SC ONLOGON /DELAY 0000:15 "
                        L"/RL LIMITED /IT /F /TR \"" + exePath + L"\"";
    if (RunElevatedSchtasks(args))
        LOG_INFO << "开机自启任务已注册";
}

void RemoveTask() {
    if (RunElevatedSchtasks(L"/Delete /TN \"DynamicWallpaper\" /F"))
        LOG_INFO << "开机自启任务已移除";
}

}
