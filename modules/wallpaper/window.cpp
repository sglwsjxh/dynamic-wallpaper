#include "wallpaper/window.h"
#include "logs/log.h"
#include "wallpaper/media.h"

namespace win {

HWND CreateWallpaperWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0,
                      hInstance, nullptr, nullptr,
                      (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr,
                      L"LowMemWallpaper", nullptr };
    RegisterClassEx(&wc);

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

// 诊断发现 (Win11 24H2):
//   Progman (visible)
//     ├── SHELLDLL_DefView (图标层)
//     └── WorkerW           (壁纸层！Progman 的直接子窗口，可见且全屏)
//   0x052C 在此系统上无效 (不产生新 WorkerW)
//   15 个顶层 WorkerW 均为小窗口且不可见 (136x39)
bool EmbedDesktop(HWND hwnd) {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    if (!hProgman) {
        LOG_ERR << "EmbedDesktop: 找不到 Progman 窗口";
        return false;
    }

    // Plan A: 嵌入 Progman 的子 WorkerW（诊断确认存在且可见）
    HWND hChildWorker = FindWindowEx(hProgman, nullptr, L"WorkerW", nullptr);
    HWND hTarget = hChildWorker ? hChildWorker : hProgman;

    LOG_INFO << "EmbedDesktop: 嵌入目标=" << (hChildWorker ? L"Progman的子WorkerW" : L"Progman(直连)");

    // 转为 WS_CHILD 并嵌入
    SetWindowLongPtr(hwnd, GWL_STYLE,
        (GetWindowLongPtr(hwnd, GWL_STYLE) & ~WS_POPUP) | WS_CHILD | WS_VISIBLE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);

    if (!SetParent(hwnd, hTarget)) {
        LOG_ERR << "EmbedDesktop: SetParent 失败, err=" << GetLastError();
        return false;
    }

    // 放在 SHELLDLL_DefView 后面（图标层之后）
    HWND hInsertAfter = HWND_BOTTOM;
    HWND hShellView = FindDesktopShellView();
    if (hShellView)
        hInsertAfter = hShellView;

    if (!SetWindowPos(hwnd, hInsertAfter, 0, 0, 0, 0,
                      SWP_NOSIZE | SWP_NOACTIVATE)) {
        LOG_ERR << "EmbedDesktop: SetWindowPos(Z-order) 失败, err=" << GetLastError();
        return false;
    }

    LOG_INFO << "EmbedDesktop: 嵌入成功"
             << " (hwnd=" << hwnd << ", target=" << hTarget << ")";
    return true;
}

void SetFullscreen(HWND hwnd) {
    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    SetWindowPos(hwnd, nullptr, 0, 0, dm.dmPelsWidth, dm.dmPelsHeight,
                 SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_CONTEXTMENU: {
            // 安全网：若不小心在图标层前面，把右键转发给桌面
            HWND hShellView = FindDesktopShellView();
            if (hShellView) PostMessage(hShellView, msg, wParam, lParam);
            return 0;
        }

        case WM_DISPLAYCHANGE: {
            int newW = LOWORD(lParam), newH = HIWORD(lParam);
            LOG_INFO << "WM_DISPLAYCHANGE: 分辨率变化 -> " << newW << "x" << newH;
            SetFullscreen(hwnd);

            mpv_handle* ctx = reinterpret_cast<mpv_handle*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (ctx) media::LogPlayerInfo(ctx);
            break;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam), h = HIWORD(lParam);
            if (w > 0 && h > 0)
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, w, h, SWP_NOACTIVATE);
            break;
        }

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
