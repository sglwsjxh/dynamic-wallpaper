#include "app/app.h"
#include "logs/log.h"

namespace app {

bool Init(Context& ctx, const Config& cfg) {
    ctx.hInstance = GetModuleHandle(nullptr);
    ctx.running = true;

    if (!shell::Init(ctx.shell, ctx.hInstance)) {
        LOG_ERR << "App: shell 控制器初始化失败";
        return false;
    }

    if (cfg.wallpaper_enabled) {
        if (!wallpaper::Init(ctx.wallpaper, cfg, ctx.hInstance))
            LOG_WARN << "App: 壁纸模块初始化失败，继续运行 shell 主控";
    } else {
        LOG_INFO << "App: 壁纸功能已禁用";
    }

    LOG_INFO << "App: 初始化完成";
    return true;
}

void Run(Context& ctx) {
    MSG msg{};

    while (ctx.running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                ctx.running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!ctx.running) break;

        // 统一分发 shell 系统事件给各业务模块
        // 后续新增 dock/taskbar 模块时，在这里分发同一批事件

        if (ctx.shell.explorer_recreated) {
            LOG_INFO << "App: Explorer 重建，通知各模块";
            wallpaper::OnExplorerRestarted(ctx.wallpaper);
            ctx.shell.explorer_recreated = false;
        }

        if (ctx.shell.display_changed) {
            LOG_INFO << "App: 显示器变化，通知各模块";
            wallpaper::OnDisplayChanged(ctx.wallpaper,
                                        ctx.shell.display_width,
                                        ctx.shell.display_height);
            ctx.shell.display_changed = false;
        }

        if (ctx.shell.power_resumed) {
            LOG_INFO << "App: 系统恢复，通知各模块";
            wallpaper::OnPowerResume(ctx.wallpaper);
            ctx.shell.power_resumed = false;
        }

        if (ctx.shell.power_suspended) {
            // wallpaper 模块无需处理挂起，仅清标志
            ctx.shell.power_suspended = false;
        }

        wallpaper::Tick(ctx.wallpaper);
        Sleep(8);
    }
}

void Shutdown(Context& ctx) {
    wallpaper::Shutdown(ctx.wallpaper);
    shell::Shutdown(ctx.shell);
    LOG_INFO << "App: 已关闭";
}

} // namespace app
