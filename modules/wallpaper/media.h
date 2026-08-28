#pragma once
#include <windows.h>
#include <string>
#include <mpv/client.h>

namespace media {
    mpv_handle* CreatePlayer();
    void ConfigureLowOverhead(mpv_handle* ctx);
    bool SetOutputWindow(mpv_handle* ctx, HWND hwnd);
    bool InitPlayer(mpv_handle* ctx);
    bool LoadBackgroundVideo(mpv_handle* ctx, const std::string& videoPath);
    void VerifyHwdec(mpv_handle* ctx);
    void DestroyPlayer(mpv_handle* ctx);

    void LogPlayerInfo(mpv_handle* ctx);
    void LogDisplayInfo();
    void AutoConfigureGPU(mpv_handle* ctx, int screenW, int screenH, const std::string& gpuPreference);
    inline void AutoConfigureGPU(mpv_handle* ctx, int screenW, int screenH) {
        AutoConfigureGPU(ctx, screenW, screenH, "auto");
    }
}
