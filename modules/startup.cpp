#include "startup.h"
#include "path.h"
#include "process.h"
#include <winreg.h>

namespace {

void RemoveLegacyRegistryEntry() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"DynamicWallpaper");
        RegCloseKey(hKey);
    }
}

}

namespace startup {

void EnsureTask() {
    const std::wstring exePath = path::GetExePath();
    if (exePath.empty()) return;

    std::wstring taskCmd =
        L"schtasks.exe /Create /TN \"DynamicWallpaper\" /SC ONLOGON /DELAY 0000:15 "
        L"/RL LIMITED /IT /F /TR \"" + exePath + L"\"";

    DWORD exitCode = 1;
    if (proc::RunHiddenAndWait(taskCmd, &exitCode) && exitCode == 0)
        RemoveLegacyRegistryEntry();
}

}
