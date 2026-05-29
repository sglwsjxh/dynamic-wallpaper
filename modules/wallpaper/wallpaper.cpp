#include "wallpaper/wallpaper.h"
#include "wallpaper/window.h"
#include "wallpaper/media.h"
#include "config/config.h"
#include "logs/log.h"

namespace wallpaper {

static void RecreateWallpaper(Context& ctx);

bool Init(Context& ctx, const Config& cfg) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ctx.hInstance = GetModuleHandle(nullptr);
    ctx.background_path = cfg.background_path;
    ctx.running = true;
    ctx.need_recreate = false;

    ctx.ctrl_hwnd = win::CreateControllerWindow(ctx.hInstance, &ctx);
    if (!ctx.ctrl_hwnd) {
        LOG_ERR << "控制器窗口创建失败";
        return false;
    }
    LOG_INFO << "控制器窗口创建成功, ctrl_hwnd=" << ctx.ctrl_hwnd;

    ctx.wallpaper_hwnd = win::CreateWallpaperWindow(ctx.hInstance);
    if (!ctx.wallpaper_hwnd) {
        LOG_ERR << "壁纸窗口创建失败";
        DestroyWindow(ctx.ctrl_hwnd);
        ctx.ctrl_hwnd = nullptr;
        return false;
    }

    if (!win::EmbedDesktop(ctx.wallpaper_hwnd)) {
        LOG_ERR << "壁纸嵌入桌面失败";
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        DestroyWindow(ctx.ctrl_hwnd);
        ctx.ctrl_hwnd = nullptr;
        return false;
    }
    win::SetFullscreen(ctx.wallpaper_hwnd);

    ctx.mpv = media::CreatePlayer();
    if (!ctx.mpv) {
        LOG_ERR << "mpv 创建失败";
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        DestroyWindow(ctx.ctrl_hwnd);
        ctx.ctrl_hwnd = nullptr;
        return false;
    }

    media::ConfigureLowOverhead(ctx.mpv);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    media::AutoConfigureGPU(ctx.mpv, dm.dmPelsWidth, dm.dmPelsHeight);

    media::SetOutputWindow(ctx.mpv, ctx.wallpaper_hwnd);
    SetWindowLongPtr(ctx.wallpaper_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx.mpv));

    if (!media::InitPlayer(ctx.mpv)) {
        LOG_ERR << "mpv 初始化失败";
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        DestroyWindow(ctx.ctrl_hwnd);
        ctx.ctrl_hwnd = nullptr;
        return false;
    }

    if (!media::LoadBackgroundVideo(ctx.mpv, cfg.background_path))
        LOG_WARN << "视频未找到或加载失败: " << cfg.background_path;

    media::VerifyHwdec(ctx.mpv);
    media::LogDisplayInfo();

    LOG_INFO << "壁纸模块初始化完成";
    return true;
}

bool Tick(Context& ctx) {
    if (ctx.mpv) {
        mpv_event* ev = mpv_wait_event(ctx.mpv, 0);
        if (ev->event_id == MPV_EVENT_FILE_LOADED) {
            LOG_INFO << "mpv 视频加载完成";
            media::LogPlayerInfo(ctx.mpv);
        } else if (ev->event_id == MPV_EVENT_SHUTDOWN) {
            LOG_WARN << "mpv 意外关闭，标记重建";
            ctx.need_recreate = true;
        }
    }

    if (ctx.need_recreate) {
        // 检查 Progman 是否已重建（Explorer 可能还没完全启动）
        HWND hProgman = FindWindow(L"Progman", L"Program Manager");
        if (hProgman) {
            LOG_INFO << "检测到 need_recreate，开始重建壁纸";
            RecreateWallpaper(ctx);
            ctx.need_recreate = false;
        }
        // Progman 不存在时保留 need_recreate，下次 Tick 再试
    }

    return ctx.running;
}

void Shutdown(Context& ctx) {
    // 必须按顺序：先解绑 GWLP_USERDATA 避免 WndProc 访问悬空指针
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
    if (ctx.ctrl_hwnd) {
        DestroyWindow(ctx.ctrl_hwnd);
        ctx.ctrl_hwnd = nullptr;
    }
    LOG_INFO << "壁纸模块已卸载";
}

static void RecreateWallpaper(Context& ctx) {
    LOG_INFO << "RecreateWallpaper: 开始重建";

    if (ctx.wallpaper_hwnd) {
        SetWindowLongPtr(ctx.wallpaper_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        LOG_INFO << "RecreateWallpaper: 旧壁纸窗口已销毁";
    }

    if (ctx.mpv) {
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
        LOG_INFO << "RecreateWallpaper: 旧 mpv 已销毁";
    }

    ctx.wallpaper_hwnd = win::CreateWallpaperWindow(ctx.hInstance);
    if (!ctx.wallpaper_hwnd) {
        LOG_ERR << "RecreateWallpaper: 壁纸窗口创建失败";
        ctx.running = false;
        return;
    }
    LOG_INFO << "RecreateWallpaper: 新壁纸窗口创建成功";

    if (!win::EmbedDesktop(ctx.wallpaper_hwnd)) {
        LOG_ERR << "RecreateWallpaper: 嵌入桌面失败";
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        return;
    }
    win::SetFullscreen(ctx.wallpaper_hwnd);

    ctx.mpv = media::CreatePlayer();
    if (!ctx.mpv) {
        LOG_ERR << "RecreateWallpaper: mpv 创建失败";
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        return;
    }

    media::ConfigureLowOverhead(ctx.mpv);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    media::AutoConfigureGPU(ctx.mpv, dm.dmPelsWidth, dm.dmPelsHeight);

    media::SetOutputWindow(ctx.mpv, ctx.wallpaper_hwnd);
    SetWindowLongPtr(ctx.wallpaper_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx.mpv));

    if (!media::InitPlayer(ctx.mpv)) {
        LOG_ERR << "RecreateWallpaper: mpv 初始化失败";
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
        DestroyWindow(ctx.wallpaper_hwnd);
        ctx.wallpaper_hwnd = nullptr;
        return;
    }

    if (!media::LoadBackgroundVideo(ctx.mpv, ctx.background_path))
        LOG_WARN << "RecreateWallpaper: 视频未找到: " << ctx.background_path;

    media::VerifyHwdec(ctx.mpv);
    LOG_INFO << "RecreateWallpaper: 重建完成";
}

} // namespace wallpaper
