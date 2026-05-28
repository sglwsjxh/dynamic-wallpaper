#include "wallpaper/wallpaper.h"
#include "wallpaper/window.h"
#include "wallpaper/media.h"
#include "config/config.h"
#include "logs/log.h"

namespace wallpaper {

bool Init(Context& ctx, const Config& cfg) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ctx.hwnd = win::CreateWallpaperWindow(GetModuleHandle(nullptr));
    if (!ctx.hwnd) {
        LOG_ERR << "壁纸窗口创建失败";
        return false;
    }

    win::EmbedDesktop(ctx.hwnd);
    win::SetFullscreen(ctx.hwnd);

    ctx.mpv = media::CreatePlayer();
    if (!ctx.mpv) {
        LOG_ERR << "mpv 创建失败";
        DestroyWindow(ctx.hwnd);
        ctx.hwnd = nullptr;
        return false;
    }

    media::ConfigureLowOverhead(ctx.mpv);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    media::AutoConfigureGPU(ctx.mpv, dm.dmPelsWidth, dm.dmPelsHeight);

    media::SetOutputWindow(ctx.mpv, ctx.hwnd);
    SetWindowLongPtr(ctx.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx.mpv));

    if (!media::InitPlayer(ctx.mpv)) {
        LOG_ERR << "mpv 初始化失败";
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
        DestroyWindow(ctx.hwnd);
        ctx.hwnd = nullptr;
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
    mpv_event* ev = mpv_wait_event(ctx.mpv, 0);
    if (ev->event_id == MPV_EVENT_FILE_LOADED) {
        LOG_INFO << "mpv 视频加载完成";
        media::LogPlayerInfo(ctx.mpv);
    } else if (ev->event_id == MPV_EVENT_SHUTDOWN) {
        LOG_WARN << "mpv 意外关闭";
        return false;
    }
    return true;
}

void Shutdown(Context& ctx) {
    if (ctx.hwnd) {
        SetWindowLongPtr(ctx.hwnd, GWLP_USERDATA, 0);
    }
    if (ctx.mpv) {
        media::DestroyPlayer(ctx.mpv);
        ctx.mpv = nullptr;
    }
    if (ctx.hwnd) {
        DestroyWindow(ctx.hwnd);
        ctx.hwnd = nullptr;
    }
    LOG_INFO << "壁纸模块已卸载";
}

} // namespace wallpaper
