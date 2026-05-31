#pragma once
#include <windows.h>

namespace desktop_overlay {

struct Context {
    HINSTANCE hInstance = nullptr;
    HWND hwnd = nullptr;
    HWND helper_hwnd = nullptr;
    HWINEVENTHOOK hook = nullptr;
    ULONG_PTR gdiplus_token = 0;
    bool enabled = false;
    bool show_desktop = false;
};

bool Init(Context& ctx, HINSTANCE hInstance);
void Tick(Context& ctx);
void Shutdown(Context& ctx);

void OnExplorerRestarted(Context& ctx);
void OnDisplayChanged(Context& ctx, int width, int height);
void OnPowerResume(Context& ctx);

}
