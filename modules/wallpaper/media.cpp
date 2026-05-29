#include "wallpaper/media.h"
#include "logs/log.h"
#include "path/path.h"
#include <filesystem>
#include <iostream>
#include <dxgi.h>

namespace {

bool SetOption(mpv_handle* ctx, const char* key, const char* value) {
    int ret = mpv_set_option_string(ctx, key, value);
    if (ret < 0)
        LOG_WARN << "mpv option 设置失败: " << key << "=" << value << " ret=" << ret;
    return ret >= 0;
}

}

namespace media {

mpv_handle* CreatePlayer() {
    return mpv_create();
}

void ConfigureLowOverhead(mpv_handle* ctx) {
    SetOption(ctx, "audio", "no");
    SetOption(ctx, "ao", "null");
    SetOption(ctx, "hwdec", "d3d11va");
    SetOption(ctx, "vo", "gpu");
    SetOption(ctx, "gpu-context", "d3d11");
    SetOption(ctx, "gpu-api", "d3d11");
    SetOption(ctx, "scale", "bilinear");
    SetOption(ctx, "dscale", "bilinear");
    SetOption(ctx, "cscale", "bilinear");
    SetOption(ctx, "interpolation", "no");
    SetOption(ctx, "dither", "no");
    SetOption(ctx, "gpu-shader-cache-size", "0");
    SetOption(ctx, "demuxer-max-bytes", "8MiB");
    SetOption(ctx, "video-sync", "display-vdrop");
    SetOption(ctx, "loop", "inf");
    SetOption(ctx, "panscan", "1.0");
    SetOption(ctx, "d3d11-output-format", "bgra8");
}

bool SetOutputWindow(mpv_handle* ctx, HWND hwnd) {
    int64_t wid = reinterpret_cast<intptr_t>(hwnd);
    int ret = mpv_set_option(ctx, "wid", MPV_FORMAT_INT64, &wid);
    if (ret < 0) {
        LOG_ERR << "mpv wid 设置失败, hwnd=" << hwnd << " ret=" << ret;
        return false;
    }
    return true;
}

bool InitPlayer(mpv_handle* ctx) {
    return mpv_initialize(ctx) >= 0;
}

bool LoadBackgroundVideo(mpv_handle* ctx, const std::string& videoPath) {
    std::string fullPath;

    // 检测是否为 Windows 绝对路径（如 C:\... 或 \\...）
    if ((videoPath.size() > 1 && videoPath[1] == ':') ||
        (!videoPath.empty() && (videoPath[0] == '/' || videoPath[0] == '\\'))) {
        fullPath = videoPath;
    } else {
        // 相对路径，基于 exe 目录拼接
        auto exeDir = path::GetExeDir();
        auto pathObj = std::filesystem::path(exeDir) / videoPath;
        auto pathUtf8 = pathObj.u8string();
        fullPath.assign(reinterpret_cast<const char*>(pathUtf8.data()), pathUtf8.size());
    }

    if (!std::filesystem::exists(std::filesystem::path(fullPath))) {
        LOG_ERR << "视频文件不存在: " << fullPath;
        return false;
    }

    const char* args[] = { "loadfile", fullPath.c_str(), nullptr };
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

void AutoConfigureGPU(mpv_handle* ctx, int screenW, int screenH) {
    bool hasDedicated = false;

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(IID_IDXGIFactory1, (void**)&factory);
    if (SUCCEEDED(hr) && factory) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                std::wstring descStr(desc.Description);
                LOG_INFO << "检测到 GPU[" << i << "]: " << descStr;

                if (!hasDedicated &&
                    (descStr.find(L"NVIDIA") != std::wstring::npos ||
                     descStr.find(L"AMD") != std::wstring::npos ||
                     descStr.find(L"Radeon") != std::wstring::npos)) {
                    hasDedicated = true;
                    char narrowName[256] = {};
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                                       narrowName, sizeof(narrowName), nullptr, nullptr);
                    SetOption(ctx, "d3d11-adapter", narrowName);
                    LOG_INFO << "使用独显渲染: " << narrowName;
                }
            }
            adapter->Release();
        }
        factory->Release();
    }

    if (!hasDedicated) {
        LOG_INFO << "仅检测到集成显卡，启用集显优化";
        SetOption(ctx, "d3d11va-zero-copy", "yes");
        SetOption(ctx, "correct-downscaling", "no");
        SetOption(ctx, "linear-downscaling", "no");
    }

    if (screenW > 0 && screenH > 0) {
        char vf[64];
        snprintf(vf, sizeof(vf), "scale=%d:%d", screenW, screenH);
        SetOption(ctx, "vf", vf);
        LOG_INFO << "视频缩放至: " << screenW << "x" << screenH;
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
