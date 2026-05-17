#include "media.h"
#include "log.h"
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
    mpv_set_option_string(ctx, "d3d11-adapter", "NVIDIA GeForce RTX 5070 Laptop GPU");
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
    if (mpv_get_property(ctx, "hwdec-current", MPV_FORMAT_STRING, &hwdec_str) >= 0) {
        LOG_INFO << "硬件解码: " << (hwdec_str ? hwdec_str : "软解");
        mpv_free(hwdec_str);
    } else {
        LOG_INFO << "硬件解码: 查询失败";
    }
}

void DestroyPlayer(mpv_handle* ctx) {
    mpv_terminate_destroy(ctx);
}

void LogPlayerInfo(mpv_handle* ctx) {
    if (!ctx) {
        LOG_WARN << "LogPlayerInfo: ctx 为空";
        return;
    }

    char* hwdec = nullptr;
    if (mpv_get_property(ctx, "hwdec-current", MPV_FORMAT_STRING, &hwdec) >= 0) {
        LOG_INFO << "mpv hwdec-current: " << (hwdec ? hwdec : "(null)");
        mpv_free(hwdec);
    }

    double fps = 0;
    if (mpv_get_property(ctx, "display-fps", MPV_FORMAT_DOUBLE, &fps) >= 0)
        LOG_INFO << "mpv display-fps: " << fps;

    double vfps = 0;
    if (mpv_get_property(ctx, "estimated-vf-fps", MPV_FORMAT_DOUBLE, &vfps) >= 0)
        LOG_INFO << "mpv estimated-vf-fps: " << vfps;

    char* vo = nullptr;
    if (mpv_get_property(ctx, "current-vo", MPV_FORMAT_STRING, &vo) >= 0) {
        LOG_INFO << "mpv current-vo: " << (vo ? vo : "(null)");
        mpv_free(vo);
    }

    int64_t dw = 0, dh = 0;
    if (mpv_get_property(ctx, "dwidth", MPV_FORMAT_INT64, &dw) >= 0)
        LOG_INFO << "mpv dwidth: " << dw;
    if (mpv_get_property(ctx, "dheight", MPV_FORMAT_INT64, &dh) >= 0)
        LOG_INFO << "mpv dheight: " << dh;

    char* vo_perf = nullptr;
    if (mpv_get_property(ctx, "vo-performance", MPV_FORMAT_STRING, &vo_perf) >= 0) {
        LOG_INFO << "mpv vo-performance: " << (vo_perf ? vo_perf : "(null)");
        mpv_free(vo_perf);
    }

    char* hwdec_int = nullptr;
    if (mpv_get_property(ctx, "hwdec-interop", MPV_FORMAT_STRING, &hwdec_int) >= 0) {
        LOG_INFO << "mpv hwdec-interop: " << (hwdec_int ? hwdec_int : "(null)");
        mpv_free(hwdec_int);
    }
}

void LogDisplayInfo() {
    DISPLAY_DEVICEW dd;
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); i++) {
        std::wstring devName(dd.DeviceName);
        std::wstring devStr(dd.DeviceString);
        LOG_INFO << "显示器[" << i << "]: " << devName
                 << " = " << devStr
                 << (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE ? " [主]" : "")
                 << (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP ? "" : " [未连接]");

        DEVMODEW dm;
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
            LOG_INFO << "  -> " << dm.dmPelsWidth << "x" << dm.dmPelsHeight
                     << " @" << dm.dmDisplayFrequency << "Hz"
                     << " bpp=" << dm.dmBitsPerPel;
        }

        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
    }
}

}
