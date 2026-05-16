#pragma once
#include <windows.h>

struct mpv_handle;

namespace win {
    HWND CreateWallpaperWindow(HINSTANCE hInstance);
    void EmbedDesktop(HWND hwnd);
    void SetFullscreen(HWND hwnd);
    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
