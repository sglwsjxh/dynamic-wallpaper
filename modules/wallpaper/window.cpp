#include "wallpaper/window.h"
#include "logs/log.h"

namespace win {

// 壁纸窗口：嵌入桌面层，承载 mpv 渲染

HWND CreateWallpaperWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0,
                      hInstance, nullptr, nullptr,
                      (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr,
                      L"LowMemWallpaper", nullptr };
    ATOM atom = RegisterClassEx(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        LOG_ERR << "CreateWallpaperWindow: RegisterClassEx 失败, err=" << GetLastError();
        return nullptr;
    }

    HWND hwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LowMemWallpaper", L"",
                               WS_POPUP | WS_VISIBLE, 0, 0, 0, 0,
                               nullptr, nullptr, hInstance, nullptr);
    LOG_INFO << "CreateWallpaperWindow: hwnd=" << hwnd;
    return hwnd;
}

static HWND FindDesktopShellView() {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    if (!hProgman) return nullptr;
    return FindWindowEx(hProgman, nullptr, L"SHELLDLL_DefView", nullptr);
}

// 在 Progman 下找壁纸 WorkerW（可见、全屏、无 SHELLDLL_DefView）
static HWND FindWallpaperHost(HWND hProgman) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    long long screenArea = static_cast<long long>(screenW) * screenH;

    HWND hChild = nullptr;
    while ((hChild = FindWindowEx(hProgman, hChild, L"WorkerW", nullptr)) != nullptr) {
        if (!IsWindowVisible(hChild)) continue;

        // 跳过包含 SHELLDLL_DefView 的 WorkerW（那是图标层）
        if (FindWindowEx(hChild, nullptr, L"SHELLDLL_DefView", nullptr))
            continue;

        RECT rc;
        if (!GetWindowRect(hChild, &rc)) continue;

        long long area = static_cast<long long>(rc.right - rc.left) * (rc.bottom - rc.top);
        if (area >= screenArea * 80 / 100)
            return hChild;
    }
    return nullptr;
}

bool EmbedDesktop(HWND hwnd) {
    LOG_INFO << "EmbedDesktop: 开始嵌入, hwnd=" << hwnd;

    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    LOG_INFO << "EmbedDesktop: Progman=" << hProgman;
    if (!hProgman) {
        LOG_ERR << "EmbedDesktop: 找不到 Progman 窗口";
        return false;
    }

    HWND hWorker = FindWallpaperHost(hProgman);
    HWND hTarget = hWorker ? hWorker : hProgman;
    bool hasWorkerW = (hWorker != nullptr);

    HWND hShellView = FindDesktopShellView();
    LOG_INFO << "EmbedDesktop: Progman=" << hProgman
             << " WorkerW=" << hWorker
             << " SHELLDLL_DefView=" << hShellView
             << " 目标=" << (hasWorkerW ? L"WorkerW" : L"Progman");

    if (!hasWorkerW) {
        // 发送 0x052C 激活 WorkerW 壁纸层（标准 Win11 动态壁纸做法）
        LOG_INFO << "EmbedDesktop: 发送 0x052C 激活壁纸 WorkerW";
        SendMessageTimeout(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
        hWorker = FindWallpaperHost(hProgman);
        hTarget = hWorker ? hWorker : hProgman;
        hasWorkerW = (hWorker != nullptr);
        LOG_INFO << "EmbedDesktop: 0x052C 后 WorkerW=" << hWorker;
    }

    LONG oldStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE,
        (oldStyle & ~WS_POPUP) | WS_CHILD | WS_VISIBLE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    LONG newStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
    LOG_INFO << "EmbedDesktop: style 0x" << std::hex << oldStyle << " -> 0x" << newStyle << std::dec;

    if (!SetParent(hwnd, hTarget)) {
        LOG_ERR << "EmbedDesktop: SetParent 失败, err=" << GetLastError();
        return false;
    }
    LOG_INFO << "EmbedDesktop: SetParent 成功, parent=" << hTarget;

    if (hasWorkerW) {
        if (!SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_ERR << "EmbedDesktop: SetWindowPos(HWND_BOTTOM) 失败, err=" << GetLastError();
            return false;
        }
        LOG_INFO << "EmbedDesktop: Z-order HWND_BOTTOM (WorkerW 内部)";
    } else {
        HWND hInsertAfter = hShellView ? hShellView : HWND_BOTTOM;
        if (!SetWindowPos(hwnd, hInsertAfter, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_ERR << "EmbedDesktop: SetWindowPos(Z-order) 失败, err=" << GetLastError();
            return false;
        }
        LOG_INFO << "EmbedDesktop: Z-order hInsertAfter=" << hInsertAfter << " (Progman 内)";
    }

    LOG_INFO << "EmbedDesktop: 嵌入成功";
    return true;
}

void SetFullscreen(HWND hwnd) {
    HWND hParent = GetParent(hwnd);
    if (!hParent || !IsWindow(hParent)) {
        DEVMODE dm{};
        dm.dmSize = sizeof(dm);
        EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
        SetWindowPos(hwnd, nullptr, 0, 0, dm.dmPelsWidth, dm.dmPelsHeight,
                     SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
        LOG_INFO << "SetFullscreen: 无 parent, 设为 " << dm.dmPelsWidth << "x" << dm.dmPelsHeight;
        return;
    }

    RECT rc;
    GetClientRect(hParent, &rc);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right, rc.bottom,
                 SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
    LOG_INFO << "SetFullscreen: parent client " << rc.right << "x" << rc.bottom;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
#ifdef _DEBUG
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_CONTEXTMENU:
            LOG_INFO << "WndProc: 壁纸窗口收到右键（层级异常），hwnd=" << hwnd;
            return 0;
#endif

        case WM_DESTROY:
            LOG_INFO << "WndProc: 壁纸窗口被销毁, hwnd=" << hwnd;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace win
