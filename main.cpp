#include <windows.h>

#include "logs/log.h"
#include "process/process.h"
#include "startup/startup.h"
#include "wallpaper/window.h"
#include "wallpaper/media.h"

int main() {
    logm::Init();
    LOG_INFO << "Dynamic Wallpaper 启动";

    proc::CloseOldInstances();
    startup::EnsureTask();

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HWND hwnd = win::CreateWallpaperWindow(GetModuleHandle(nullptr));
    win::EmbedDesktop(hwnd);
    win::SetFullscreen(hwnd);

    mpv_handle* ctx = media::CreatePlayer();
    if (!ctx) {
        LOG_ERR << "mpv 创建失败";
        return 1;
    }

    media::ConfigureLowOverhead(ctx);

    DEVMODE dm{};
    dm.dmSize = sizeof(dm);
    EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm);
    media::AutoConfigureGPU(ctx, dm.dmPelsWidth, dm.dmPelsHeight);

    media::SetOutputWindow(ctx, hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

    if (!media::InitPlayer(ctx)) {
        LOG_ERR << "mpv 初始化失败";
        media::DestroyPlayer(ctx);
        return 1;
    }

    if (!media::LoadBackgroundVideo(ctx))
        LOG_WARN << "background.mp4 未找到，壁纸未加载";

    media::VerifyHwdec(ctx);
    media::LogDisplayInfo();

    MSG msg;
    for (;;) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                goto exit_loop;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        mpv_event* ev = mpv_wait_event(ctx, 0);
        if (ev->event_id == MPV_EVENT_FILE_LOADED) {
            LOG_INFO << "mpv 视频加载完成";
            media::LogPlayerInfo(ctx);
        } else if (ev->event_id == MPV_EVENT_SHUTDOWN) {
            break;
        }
        Sleep(8);
    }

exit_loop:
    media::DestroyPlayer(ctx);
    LOG_INFO << "程序退出";
    return 0;
}