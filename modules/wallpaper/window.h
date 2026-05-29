#pragma once
#include <windows.h>

struct mpv_handle;

namespace wallpaper { struct Context; }

namespace win {
    HWND CreateWallpaperWindow(HINSTANCE hInstance);
    HWND CreateControllerWindow(HINSTANCE hInstance, wallpaper::Context* ctx);
    bool EmbedDesktop(HWND hwnd);
    void SetFullscreen(HWND hwnd);
    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK ControllerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
