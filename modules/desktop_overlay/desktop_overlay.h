#pragma once
#include <windows.h>
#include <vector>

#include "desktop_overlay/widget_types.h"

struct Config;

namespace desktop_overlay {

struct Context {
    HINSTANCE hInstance = nullptr;
    HWND hwnd = nullptr;
    HWND helper_hwnd = nullptr;
    HWINEVENTHOOK hook = nullptr;
    ULONG_PTR gdiplus_token = 0;
    bool enabled = false;
    bool show_desktop = false;
    int last_minute = -1;
    int last_day = -1;
    std::vector<TextLayer> layers;
};

bool Init(Context& ctx, const Config& cfg, HINSTANCE hInstance);
void Tick(Context& ctx);
void Shutdown(Context& ctx);

void OnExplorerRestarted(Context& ctx);
void OnDisplayChanged(Context& ctx, int width, int height);
void OnPowerResume(Context& ctx);

}
