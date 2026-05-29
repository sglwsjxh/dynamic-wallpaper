#pragma once
#include <windows.h>

#include "config/config.h"
#include "shell/shell_controller.h"
#include "tray/tray.h"
#include "wallpaper/wallpaper.h"

namespace app {

struct Context {
    HINSTANCE hInstance = nullptr;
    bool running = true;

    shell::Context shell;
    tray::Context tray;
    wallpaper::Context wallpaper;
};

bool Init(Context& ctx, const Config& cfg);
void Run(Context& ctx);
void Shutdown(Context& ctx);

} // namespace app
