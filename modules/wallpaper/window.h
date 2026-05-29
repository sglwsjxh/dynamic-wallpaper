#pragma once
#include <windows.h>

namespace win {
    HWND CreateWallpaperWindow(HINSTANCE hInstance);
    bool EmbedDesktop(HWND hwnd);
    void SetFullscreen(HWND hwnd);
    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
