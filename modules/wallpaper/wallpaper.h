#pragma once
#include <windows.h>
#include <string>

struct mpv_handle;
struct Config;

namespace wallpaper {

struct Context {
    HWND wallpaper_hwnd = nullptr;   // 壁纸窗口（嵌入桌面，承载 mpv）
    mpv_handle* mpv = nullptr;
    HINSTANCE hInstance = nullptr;

    bool enabled = false;
    bool need_recreate = false;
    DWORD next_recreate_tick = 0;
    std::string background_path;
};

bool Init(Context& ctx, const Config& cfg, HINSTANCE hInstance);
void Tick(Context& ctx);
void Shutdown(Context& ctx);

void OnExplorerRestarted(Context& ctx);
void OnDisplayChanged(Context& ctx, int width, int height);
void OnPowerResume(Context& ctx);

} // namespace wallpaper
