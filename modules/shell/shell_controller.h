#pragma once
#include <windows.h>

namespace shell {

struct Context {
    HWND hwnd = nullptr;
    HINSTANCE hInstance = nullptr;

    bool explorer_recreated = false;
    bool display_changed = false;
    bool power_resumed = false;
    bool power_suspended = false;

    int display_width = 0;
    int display_height = 0;
};

bool Init(Context& ctx, HINSTANCE hInstance);
void Shutdown(Context& ctx);

} // namespace shell
