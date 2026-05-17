#pragma once
#include <windows.h>
#include <mpv/client.h>

namespace media {
    mpv_handle* CreatePlayer();
    void ConfigureLowOverhead(mpv_handle* ctx);
    void SetOutputWindow(mpv_handle* ctx, HWND hwnd);
    bool InitPlayer(mpv_handle* ctx);
    bool LoadBackgroundVideo(mpv_handle* ctx);
    void VerifyHwdec(mpv_handle* ctx);
    void DestroyPlayer(mpv_handle* ctx);

    void LogPlayerInfo(mpv_handle* ctx);
    void LogDisplayInfo();
    void AutoConfigureGPU(mpv_handle* ctx, int screenW, int screenH);
}
