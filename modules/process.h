#pragma once
#include <string>
#include <windows.h>

namespace proc {
    void CloseOldInstances();
    bool RunHiddenAndWait(const std::wstring& commandLine, DWORD* exitCode = nullptr);
}
