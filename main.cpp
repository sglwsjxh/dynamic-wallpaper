#include <windows.h>

#include "logs/log.h"
#include "config/config.h"
#include "process/process.h"
#include "startup/startup.h"
#include "wallpaper/wallpaper.h"

int main() {
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

        while (wpCtx.running) {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    wpCtx.running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (!wpCtx.running) break;

            wallpaper::Tick(wpCtx);
            Sleep(8);
        }

        wallpaper::Shutdown(wpCtx);
    }

    LOG_INFO << "程序退出";
    return 0;
}
