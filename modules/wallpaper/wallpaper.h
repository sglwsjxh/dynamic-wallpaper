#pragma once
#include <windows.h>

struct mpv_handle;
struct Config;

namespace wallpaper {

struct Context {
    HWND hwnd = nullptr;
    mpv_handle* mpv = nullptr;
};

bool Init(Context& ctx, const Config& cfg);
bool Tick(Context& ctx);  // true=正常运行, false=请求退出
void Shutdown(Context& ctx);

} // namespace wallpaper
