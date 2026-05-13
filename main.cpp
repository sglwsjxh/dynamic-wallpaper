#include <windows.h>
#include <filesystem>
#include "client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <winreg.h>
#include <string>

namespace {
constexpr wchar_t kWindowClassName[] = L"LowMemWallpaper";
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\DynamicWallpaper_SingleInstance";

std::wstring GetExecutablePath() {
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

std::wstring GetExecutableDir() {
    std::wstring exePath = GetExecutablePath();
    size_t pos = exePath.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : exePath.substr(0, pos);
}

bool RunHiddenProcessAndWait(const std::wstring& commandLine, DWORD* exitCode = nullptr) {
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

// 自动清理旧的注册表 Run 项（如果用户之前安装过旧版本）
void RemoveLegacyRunEntry() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"DynamicWallpaper");
        RegCloseKey(hKey);
    }
}

HWND FindWallpaperParentWindow() {
    HWND shellViewHost = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr)) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&shellViewHost));

    if (shellViewHost) {
        HWND nextWorkerW = FindWindowExW(nullptr, shellViewHost, L"WorkerW", nullptr);
        if (nextWorkerW) return nextWorkerW;
    }

    HWND workerWNoIcons = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        if (wcscmp(cls, L"WorkerW") == 0 &&
            !FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr)) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&workerWNoIcons));
    if (workerWNoIcons) return workerWNoIcons;
    if (shellViewHost) return shellViewHost;
    return FindWindowW(L"Progman", L"Program Manager");
}

}

// 添加到开机自启动
void EnsureStartupTask() {
    const std::wstring exePath = GetExecutablePath();
    if (exePath.empty()) return;

    std::wstring taskCmd =
        L"schtasks.exe /Create /TN \"DynamicWallpaper\" /SC ONLOGON /DELAY 0000:20 "
        L"/RL LIMITED /IT /F /TR \"\\\"" + exePath + L"\\\"\"";

    DWORD exitCode = 1;
    if (RunHiddenProcessAndWait(taskCmd, &exitCode) && exitCode == 0)
        RemoveLegacyRunEntry();
}

// 嵌入桌面 WorkerW 层
void EmbedToDesktop(HWND hwnd) {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    if (hProgman) {
        DWORD_PTR result = 0;
        SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
        SendMessageTimeoutW(hProgman, 0x052C, 0xD, 1, SMTO_NORMAL, 1000, &result);
    }

    HWND hDesktopParent = FindWallpaperParentWindow();
    if (!hDesktopParent) return;

    SetParent(hwnd, hDesktopParent);
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~WS_POPUP;
    style |= WS_CHILD;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void ResizeToDesktop(HWND hwnd) {
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    if (width > 0 && height > 0) {
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, width, height, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static UINT taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    if (msg == taskbarCreatedMsg) {
        EmbedToDesktop(hwnd);
        ResizeToDesktop(hwnd);
        return 0;
    }

    switch (msg) {
        case WM_DISPLAYCHANGE: 
            ResizeToDesktop(hwnd);
            break;

        case WM_SIZE: {
            int w = LOWORD(lParam), h = HIWORD(lParam);
            if (w <= 0 || h <= 0) {
                ResizeToDesktop(hwnd);
            } else {
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, w, h, SWP_NOZORDER);
            }
            break;
        }

        // 休眠/唤醒处理
        case WM_POWERBROADCAST:
            if (wParam == PBT_APMSUSPEND) {
                // 可在此暂停 mpv 进一步降压（需另起线程调用 mpv_set_option_string(ctx, "pause", "yes")）
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main() {
    HANDLE singleInstanceMutex = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
    if (!singleInstanceMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (singleInstanceMutex) CloseHandle(singleInstanceMutex);
        return 0;
    }

    EnsureStartupTask();

	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0,
                  GetModuleHandle(nullptr), nullptr, nullptr, 
                  (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr, kWindowClassName, nullptr };
    if (!RegisterClassExW(&wc)) {
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClassName, L"",
                               WS_POPUP | WS_VISIBLE, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        CloseHandle(singleInstanceMutex);
        return 1;
    }
    EmbedToDesktop(hwnd);

    // 初始尺寸（后续由 WM_DISPLAYCHANGE 自动修正）
    ResizeToDesktop(hwnd);

    mpv_handle* ctx = mpv_create();
    if (!ctx) return 1;

    // 低开销参数注入
    mpv_set_option_string(ctx, "audio", "no");
    mpv_set_option_string(ctx, "ao", "null");
    mpv_set_option_string(ctx, "hwdec", "d3d11va");
    mpv_set_option_string(ctx, "vo", "gpu");
    mpv_set_option_string(ctx, "gpu-context", "d3d11");
    mpv_set_option_string(ctx, "gpu-api", "d3d11");
    mpv_set_option_string(ctx, "scale", "bilinear");
    mpv_set_option_string(ctx, "dscale", "bilinear");
    mpv_set_option_string(ctx, "cscale", "bilinear");
    mpv_set_option_string(ctx, "interpolation", "no");
    mpv_set_option_string(ctx, "dither", "no");
    mpv_set_option_string(ctx, "gpu-shader-cache-size", "0");
    mpv_set_option_string(ctx, "demuxer-max-bytes", "8MiB");
    mpv_set_option_string(ctx, "video-sync", "display-vdrop");
    mpv_set_option_string(ctx, "loop", "inf");
    mpv_set_option_string(ctx, "panscan", "1.0");

    int64_t wid = reinterpret_cast<intptr_t>(hwnd);
    mpv_set_option(ctx, "wid", MPV_FORMAT_INT64, &wid);

    if (mpv_initialize(ctx) < 0) {
        std::cerr << "mpv 初始化失败\n";
        CloseHandle(singleInstanceMutex);
        return 1;
    }

    std::string path = (std::filesystem::path(GetExecutableDir()) / "background.mp4").u8string();
    const char* args[] = { "loadfile", path.c_str(), nullptr };
    mpv_command(ctx, args);

    // 验证是否成功硬解
    char* hwdec_str = nullptr;
    mpv_get_property(ctx, "hwdec-current", MPV_FORMAT_STRING, &hwdec_str);
    std::cout << "硬件解码: " << (hwdec_str ? hwdec_str : "软解") << "\n";
    if (hwdec_str) mpv_free(hwdec_str);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mpv_terminate_destroy(ctx);
    CloseHandle(singleInstanceMutex);
    return 0;
}
