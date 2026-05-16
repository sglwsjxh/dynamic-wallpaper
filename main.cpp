#include <windows.h>

#include "modules/log.h"
#include "modules/process.h"
#include "modules/startup.h"
#include "modules/window.h"
#include "modules/media.h"

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

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    media::DestroyPlayer(ctx);
    LOG_INFO << "程序退出";
    return 0;
}