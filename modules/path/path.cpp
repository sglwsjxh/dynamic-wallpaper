#include "path/path.h"
#include <windows.h>

namespace path {
    std::wstring GetExePath() {
        std::wstring path(MAX_PATH, L'\0');

        while (true) {
            DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (len == 0) return L"";
            if (len < path.size() - 1) {
                path.resize(len);
                return path;
            }
            path.resize(path.size() * 2);
        }
    }

    std::wstring GetExeDir() {
        std::wstring exePath = GetExePath();
        size_t pos = exePath.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : exePath.substr(0, pos);
    }
}
