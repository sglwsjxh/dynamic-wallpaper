#include "media.h"
#include "path.h"
#include <filesystem>
#include <iostream>

namespace media {

mpv_handle* CreatePlayer() {
    return mpv_create();
}

void ConfigureLowOverhead(mpv_handle* ctx) {
    mpv_set_option_string(ctx, "audio", "no");
    mpv_set_option_string(ctx, "ao", "null");
    mpv_set_option_string(ctx, "hwdec", "d3d11va");
    mpv_set_option_string(ctx, "vo", "gpu");
    mpv_set_option_string(ctx, "gpu-context", "d3d11");
    mpv_set_option_string(ctx, "gpu-api", "d3d11");
    mpv_set_option_string(ctx, "scale", "bilinear");
    mpv_set_option_string(ctx, "dscale", "bilinear");
    mpv_set_option_string(ctx, "cscale", "bilinear");
    mpv_set_option_string(ctx, "interpolation", "no");
    mpv_set_option_string(ctx, "dither", "no");
    mpv_set_option_string(ctx, "gpu-shader-cache-size", "0");
    mpv_set_option_string(ctx, "demuxer-max-bytes", "8MiB");
    mpv_set_option_string(ctx, "video-sync", "display-vdrop");
    mpv_set_option_string(ctx, "loop", "inf");
    mpv_set_option_string(ctx, "panscan", "1.0");
}

void SetOutputWindow(mpv_handle* ctx, HWND hwnd) {
    int64_t wid = reinterpret_cast<intptr_t>(hwnd);
    mpv_set_option(ctx, "wid", MPV_FORMAT_INT64, &wid);
}

bool InitPlayer(mpv_handle* ctx) {
    return mpv_initialize(ctx) >= 0;
}

bool LoadBackgroundVideo(mpv_handle* ctx) {
    const auto pathUtf8 = (std::filesystem::path(path::GetExeDir()) / "background.mp4").u8string();

    const std::string path(
        reinterpret_cast<const char*>(pathUtf8.data()),
        pathUtf8.size()
    );

    const char* args[] = { "loadfile", path.c_str(), nullptr };
    return mpv_command(ctx, args) >= 0;
}

void VerifyHwdec(mpv_handle* ctx) {
    char* hwdec_str = nullptr;
    mpv_get_property(ctx, "hwdec-current", MPV_FORMAT_STRING, &hwdec_str);
    std::cout << "硬件解码: " << (hwdec_str ? hwdec_str : "软解") << "\n";
    if (hwdec_str) mpv_free(hwdec_str);
}

void DestroyPlayer(mpv_handle* ctx) {
    mpv_terminate_destroy(ctx);
}

}
