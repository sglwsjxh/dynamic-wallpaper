#include <windows.h>

#include "logs/log.h"
#include "config/config.h"
#include "process/process.h"
#include "startup/startup.h"
#include "app/app.h"

int main() {
    logm::Init();
    LOG_INFO << "Dynamic Wallpaper 启动";

    auto cfg = LoadConfig();

    if (cfg.auto_startup)
        startup::EnsureTask();
    else
        startup::RemoveTask();

    proc::CloseOldInstances();

    app::Context appCtx;
    if (!app::Init(appCtx, cfg)) {
        LOG_ERR << "App 初始化失败";
        app::Shutdown(appCtx);
        return 1;
    }

    app::Run(appCtx);
    app::Shutdown(appCtx);

    LOG_INFO << "程序退出";
    return 0;
}
