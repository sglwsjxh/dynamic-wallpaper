#include "wallpaper/wallpaper.h"
#include "wallpaper/window.h"
#include "wallpaper/media.h"
#include "config/config.h"
#include "logs/log.h"

namespace wallpaper {

static void DestroyWallpaperRuntime(Context& ctx) {
    if (ctx.wallpaper_hwnd) {
        SetWindowLongPtr(ctx.wallpaper_hwnd, GWLP_USERDATA, 0);
    }
    if (ctx.mpv) {
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
    }
    if (ctx.wallpaper_hwnd) {
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
    }
}

static bool CreateWallpaperRuntime(Context& ctx) {
    ctx.wallpaper_hwnd = win::CreateWallpaperWindow(ctx.hInstance);
    if (!ctx.wallpaper_hwnd) {
        LOG_ERR << "CreateWallpaperRuntime: 壁纸窗口创建失败";
        return false;
    }

    if (!win::EmbedDesktop(ctx.wallpaper_hwnd)) {
        LOG_ERR << "CreateWallpaperRuntime: 嵌入桌面失败";
        DestroyWallpaperRuntime(ctx);
        return false;
    }
    win::SetFullscreen(ctx.wallpaper_hwnd);

    ctx.mpv = media::CreatePlayer();
    if (!ctx.mpv) {
        LOG_ERR << "CreateWallpaperRuntime: mpv 创建失败";
        DestroyWallpaperRuntime(ctx);
        return false;
    }

    media::ConfigureLowOverhead(ctx.mpv);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    media::AutoConfigureGPU(ctx.mpv, dm.dmPelsWidth, dm.dmPelsHeight, ctx.gpu_preference);

    if (!media::SetOutputWindow(ctx.mpv, ctx.wallpaper_hwnd)) {
        DestroyWallpaperRuntime(ctx);
        return false;
    }
    SetWindowLongPtr(ctx.wallpaper_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx.mpv));

    if (!media::InitPlayer(ctx.mpv)) {
        LOG_ERR << "CreateWallpaperRuntime: mpv 初始化失败";
        DestroyWallpaperRuntime(ctx);
        return false;
    }

    if (!media::LoadBackgroundVideo(ctx.mpv, ctx.background_path))
        LOG_WARN << "视频未找到或加载失败: " << ctx.background_path;

    media::VerifyHwdec(ctx.mpv);
    return true;
}

static bool RecreateWallpaper(Context& ctx) {
    LOG_INFO << "RecreateWallpaper: 开始重建";
    DestroyWallpaperRuntime(ctx);
    return CreateWallpaperRuntime(ctx);
}

bool Init(Context& ctx, const Config& cfg, HINSTANCE hInstance) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ctx.hInstance = hInstance;
    ctx.background_path = cfg.background_path;
    ctx.gpu_preference = cfg.wallpaper_gpu;
    ctx.enabled = true;
    ctx.need_recreate = false;
    ctx.next_recreate_tick = 0;

    if (!CreateWallpaperRuntime(ctx)) {
        LOG_WARN << "壁纸运行时创建失败，进入重试状态";
        ctx.need_recreate = true;
        ctx.next_recreate_tick = GetTickCount() + 1000;
        return true;
    }

    media::LogDisplayInfo();
    LOG_INFO << "壁纸模块初始化完成";
    return true;
}

void Tick(Context& ctx) {
    if (!ctx.enabled) return;

    if (ctx.mpv) {
        mpv_event* ev = mpv_wait_event(ctx.mpv, 0);
        if (ev->event_id == MPV_EVENT_FILE_LOADED) {
            LOG_INFO << "mpv 视频加载完成";
            media::LogPlayerInfo(ctx.mpv);
        } else if (ev->event_id == MPV_EVENT_SHUTDOWN) {
            LOG_WARN << "mpv 意外关闭，销毁旧实例并标记重建";
            media::DestroyPlayer(ctx.mpv);
            ctx.mpv = nullptr;
            ctx.need_recreate = true;
        }
    }

    if (ctx.need_recreate) {
        DWORD now = GetTickCount();
        if (ctx.next_recreate_tick != 0 && now < ctx.next_recreate_tick)
            return;

        HWND hProgman = FindWindow(L"Progman", L"Program Manager");
        if (!hProgman) {
            ctx.next_recreate_tick = now + 1000;
            return;
        }

        LOG_INFO << "检测到 need_recreate，开始重建壁纸";
        if (RecreateWallpaper(ctx)) {
            ctx.need_recreate = false;
            ctx.next_recreate_tick = 0;
            LOG_INFO << "重建成功";
        } else {
            ctx.next_recreate_tick = now + 1000;
            LOG_WARN << "重建失败，1 秒后重试";
        }
    }
}

void Shutdown(Context& ctx) {
    if (!ctx.enabled) return;
    DestroyWallpaperRuntime(ctx);
    ctx.enabled = false;
    LOG_INFO << "壁纸模块已卸载";
}

void OnExplorerRestarted(Context& ctx) {
    if (!ctx.enabled) return;
    LOG_INFO << "Wallpaper: Explorer 重建，标记重建";
    ctx.need_recreate = true;
    ctx.next_recreate_tick = 0;
}

void OnDisplayChanged(Context& ctx, int width, int height) {
    if (!ctx.enabled) return;
    LOG_INFO << "Wallpaper: 显示器变化 -> " << width << "x" << height;

    if (ctx.wallpaper_hwnd && IsWindow(ctx.wallpaper_hwnd))
        win::SetFullscreen(ctx.wallpaper_hwnd);
    else {
        ctx.need_recreate = true;
        ctx.next_recreate_tick = 0;
    }
}

void OnPowerResume(Context& ctx) {
    if (!ctx.enabled) return;
    LOG_INFO << "Wallpaper: 系统恢复，标记重建";
    ctx.need_recreate = true;
    ctx.next_recreate_tick = 0;
}

} // namespace wallpaper
