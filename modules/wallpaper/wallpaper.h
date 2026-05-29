#pragma once
#include <windows.h>
#include <string>

struct mpv_handle;
struct Config;

namespace wallpaper {

struct Context {
    HWND wallpaper_hwnd = nullptr;   // 壁纸窗口（嵌入桌面，承载 mpv）
    HWND ctrl_hwnd = nullptr;        // 控制器窗口（隐藏，生命周期跟随程序）
    mpv_handle* mpv = nullptr;
    HINSTANCE hInstance = nullptr;
    bool running = true;
    bool need_recreate = false;
    std::string background_path;
};

bool Init(Context& ctx, const Config& cfg);
bool Tick(Context& ctx);  // true=正常运行, false=请求退出
void Shutdown(Context& ctx);

} // namespace wallpaper
