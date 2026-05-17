#include "window.h"
#include "log.h"
#include "media.h"

namespace win {

HWND CreateWallpaperWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0,
                      hInstance, nullptr, nullptr,
                      (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr,
                      L"LowMemWallpaper", nullptr };
    RegisterClassEx(&wc);

    return CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LowMemWallpaper", L"",
                          WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
                          nullptr, nullptr, hInstance, nullptr);
}

void EmbedDesktop(HWND hwnd) {
    HWND hProgman = FindWindow(L"Progman", L"Program Manager");
    SendMessage(hProgman, 0x052C, 0, 0);

    HWND hWorkerW = nullptr;
    auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL {
        if (FindWindowEx(hwnd, nullptr, L"SHELLDLL_DefView", nullptr)) {
            *(HWND*)lParam = FindWindowEx(nullptr, hwnd, L"WorkerW", nullptr);
            return FALSE;
        }
        return TRUE;
    };
    EnumWindows(enumProc, reinterpret_cast<LPARAM>(&hWorkerW));

    if (hWorkerW) SetParent(hwnd, hWorkerW);
}

void SetFullscreen(HWND hwnd) {
    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, dm.dmPelsWidth, dm.dmPelsHeight, SWP_SHOWWINDOW);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
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
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, w, h, SWP_NOZORDER);
            break;
        }

        case WM_POWERBROADCAST:
            if (wParam == PBT_APMSUSPEND) {
            }
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
