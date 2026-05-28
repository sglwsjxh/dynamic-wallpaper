#include "wallpaper/window.h"
#include "logs/log.h"
#include "wallpaper/media.h"

namespace win {

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

    return CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LowMemWallpaper", L"",
                          WS_POPUP | WS_VISIBLE, 0, 0, 0, 0,
                          nullptr, nullptr, hInstance, nullptr);
}

// SHELLDLL_DefView handles desktop icon clicks and right-click context menus.
// Lives as a direct child of Progman (Win11) or inside a WorkerW (older Win10).
static HWND FindDesktopShellView() {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    if (!hProgman) return nullptr;
    return FindWindowEx(hProgman, nullptr, L"SHELLDLL_DefView", nullptr);
}

static HWND FindWallpaperHost(HWND hProgman) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    long long screenArea = static_cast<long long>(screenW) * screenH;

    HWND hChild = nullptr;
    while ((hChild = FindWindowEx(hProgman, hChild, L"WorkerW", nullptr)) != nullptr) {
        if (!IsWindowVisible(hChild)) continue;

        RECT rc;
        if (!GetWindowRect(hChild, &rc)) continue;

        long long area = static_cast<long long>(rc.right - rc.left) * (rc.bottom - rc.top);
        if (area >= screenArea * 80 / 100)
            return hChild;
    }
    return nullptr;
}

bool EmbedDesktop(HWND hwnd) {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    if (!hProgman) {
        LOG_ERR << "EmbedDesktop: 找不到 Progman 窗口";
        return false;
    }

    HWND hWorker = FindWallpaperHost(hProgman);
    HWND hTarget = hWorker ? hWorker : hProgman;
    bool hasWorkerW = (hWorker != nullptr);

    LOG_INFO << "EmbedDesktop: 嵌入目标=" << (hasWorkerW ? L"WorkerW" : L"Progman");

    // 转为 WS_CHILD 并嵌入
    SetWindowLongPtr(hwnd, GWL_STYLE,
        (GetWindowLongPtr(hwnd, GWL_STYLE) & ~WS_POPUP) | WS_CHILD | WS_VISIBLE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);

    if (!SetParent(hwnd, hTarget)) {
        LOG_ERR << "EmbedDesktop: SetParent 失败, err=" << GetLastError();
        return false;
    }

    // Z-order：确保壁纸在图标层下方
    // 嵌入 WorkerW 时：WorkerW 已在图标层下方，用 HWND_BOTTOM 即可（同 parent 安全）
    // 直连 Progman 时：放在 SHELLDLL_DefView 后面（二者是 Progman 的 sibling）
    if (hasWorkerW) {
        if (!SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_ERR << "EmbedDesktop: SetWindowPos(HWND_BOTTOM) 失败, err=" << GetLastError();
            return false;
        }
    } else {
        HWND hShellView = FindDesktopShellView();
        HWND hInsertAfter = hShellView ? hShellView : HWND_BOTTOM;
        if (!SetWindowPos(hwnd, hInsertAfter, 0, 0, 0, 0,
                          SWP_NOSIZE | SWP_NOACTIVATE)) {
            LOG_ERR << "EmbedDesktop: SetWindowPos(Z-order) 失败, err=" << GetLastError();
            return false;
        }
    }

    LOG_INFO << "EmbedDesktop: 嵌入成功"
             << " (hwnd=" << hwnd << ", target=" << hTarget << ")";
    return true;
}

void SetFullscreen(HWND hwnd) {
    HWND hParent = GetParent(hwnd);
    if (!hParent) {
        DEVMODE dm{};
        dm.dmSize = sizeof(dm);
        EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
        SetWindowPos(hwnd, nullptr, 0, 0, dm.dmPelsWidth, dm.dmPelsHeight,
                     SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    RECT rc;
    GetClientRect(hParent, &rc);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right, rc.bottom,
                 SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
#ifdef _DEBUG
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_CONTEXTMENU: {
            HWND hShellView = FindDesktopShellView();
            if (hShellView) PostMessage(hShellView, msg, wParam, lParam);
            LOG_INFO << "WndProc: 转发右键消息到桌面";
            return 0;
        }
#endif

        case WM_DISPLAYCHANGE: {
            int newW = LOWORD(lParam), newH = HIWORD(lParam);
            LOG_INFO << "WM_DISPLAYCHANGE: 分辨率变化 -> " << newW << "x" << newH;
            SetFullscreen(hwnd);

            mpv_handle* ctx = reinterpret_cast<mpv_handle*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (ctx) media::LogPlayerInfo(ctx);
            break;
        }

        case WM_SIZE:
            break;

        case WM_POWERBROADCAST:
            if (wParam == PBT_APMRESUMEAUTOMATIC) {
                EmbedDesktop(hwnd);
                SetFullscreen(hwnd);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

}
