#pragma once
#include <windows.h>

#include "config/config.h"
#include "shell/shell_controller.h"
#include "wallpaper/wallpaper.h"

namespace app {

struct Context {
    HINSTANCE hInstance = nullptr;
    bool running = true;

    shell::Context shell;
    wallpaper::Context wallpaper;
};

bool Init(Context& ctx, const Config& cfg);
void Run(Context& ctx);
void Shutdown(Context& ctx);

} // namespace app
