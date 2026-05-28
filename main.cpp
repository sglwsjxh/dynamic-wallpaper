#include <windows.h>
#include <cstdlib>

#include "logs/log.h"
#include "config/config.h"
#include "process/process.h"
#include "startup/startup.h"
#include "wallpaper/wallpaper.h"

static void OnExit() {
    LOG_INFO << "程序退出 (atexit)";
}

int main() {
    std::atexit(OnExit);

    logm::Init();
    LOG_INFO << "Dynamic Wallpaper 启动";

    auto cfg = LoadConfig();

    if (cfg.auto_startup)
        startup::EnsureTask();
    else
        startup::RemoveTask();

    proc::CloseOldInstances();

    wallpaper::Context wpCtx;
    bool wpOk = false;

    if (cfg.wallpaper_enabled) {
        wpOk = wallpaper::Init(wpCtx, cfg);
        if (!wpOk)
            LOG_WARN << "壁纸模块初始化失败，跳过";
    } else {
        LOG_INFO << "壁纸功能已禁用，跳过";
    }

    if (wpOk) {
        MSG msg{};
        bool running = true;

        while (running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (!running) break;

            if (!wallpaper::Tick(wpCtx))
                running = false;
            Sleep(8);
        }

        wallpaper::Shutdown(wpCtx);
    }

    LOG_INFO << "程序退出";
    return 0;
}
