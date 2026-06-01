#include "desktop_overlay/desktop_overlay.h"
#include "desktop_overlay/widget_config.h"
#include "config/config.h"
#include "logs/log.h"
#include "path/path.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace desktop_overlay {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define OVERLAY_CLASS  L"DynamicWallpaperDesktopOverlay"
#define HELPER_CLASS   L"DynamicWallpaperOverlayHelper"

#define TIMER_SHOWDESKTOP  1
#define INTERVAL_SHOWDESKTOP    250

#define ZPOS_FLAGS (SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING)

// ---------------------------------------------------------------------------
// Module-internal bridge for WinEventProc (cannot pass user data via hook)
// ---------------------------------------------------------------------------

static Context* s_ctx = nullptr;

// ---------------------------------------------------------------------------
// Win11 24H2 detection
// ---------------------------------------------------------------------------

static bool ShouldUseShellWindow() {
    return GetProcAddress(GetModuleHandleW(L"user32"), "GetCurrentMonitorTopologyId") != nullptr;
}

static HWND GetDefaultShellWindow() {
    return FindWindowW(L"Progman", nullptr);
}

static HWND GetDesktopIconsHostWindow() {
    HWND shellW = GetDefaultShellWindow();
    if (!shellW) return nullptr;

    if (ShouldUseShellWindow()) {
        HWND defView = FindWindowExW(shellW, nullptr, L"SHELLDLL_DefView", L"");
        if (defView) return shellW;
        return nullptr;
    }

    HWND defView = FindWindowExW(shellW, nullptr, L"SHELLDLL_DefView", L"");
    if (defView) return nullptr;

    HWND workerW = nullptr;
    while ((workerW = FindWindowExW(nullptr, workerW, L"WorkerW", L""))) {
        BOOL visible = IsWindowVisible(workerW);
        HWND dv = FindWindowExW(workerW, nullptr, L"SHELLDLL_DefView", L"");
        if (visible && dv) return workerW;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Check Show Desktop state
// ---------------------------------------------------------------------------

static bool CheckDesktopState(Context& ctx, HWND desktopIconsHostWindow) {
    HWND hwnd = nullptr;

    if (desktopIconsHostWindow && IsWindowVisible(desktopIconsHostWindow))
        hwnd = FindWindowExW(nullptr, desktopIconsHostWindow, HELPER_CLASS, nullptr);

    bool stateChanged = (hwnd && !ctx.show_desktop) || (!hwnd && ctx.show_desktop);
    if (stateChanged) {
        ctx.show_desktop = !ctx.show_desktop;
        LOG_INFO << "Overlay: Show Desktop 状态变化 -> " << (ctx.show_desktop ? "SHOW_DESKTOP" : "NORMAL");

        if (ctx.show_desktop) {
            SetWindowPos(ctx.hwnd, HWND_TOPMOST, 0, 0, 0, 0, ZPOS_FLAGS);
            SetTimer(ctx.hwnd, TIMER_SHOWDESKTOP, 100, nullptr);
        } else {
            SetWindowPos(ctx.hwnd, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);
            SetTimer(ctx.hwnd, TIMER_SHOWDESKTOP, INTERVAL_SHOWDESKTOP, nullptr);
        }
    }

    return stateChanged;
}

// ---------------------------------------------------------------------------
// Helper window procedure — permanent anchor at HWND_BOTTOM
// ---------------------------------------------------------------------------

static LRESULT CALLBACK HelperWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_WINDOWPOSCHANGING) {
        LPWINDOWPOS wp = reinterpret_cast<LPWINDOWPOS>(lParam);
        wp->flags |= SWP_NOZORDER;
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Event hook callback — foreground window change detection
// ---------------------------------------------------------------------------

static void CALLBACK WinEventProc(HWINEVENTHOOK hHook, DWORD event,
    HWND hwnd, LONG idObject, LONG idChild,
    DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (event != EVENT_SYSTEM_FOREGROUND) return;
    if (!s_ctx || !s_ctx->hwnd || !IsWindow(s_ctx->hwnd)) return;
    if (s_ctx->show_desktop) return;

    HWND desktopHost = GetDesktopIconsHostWindow();
    if (!desktopHost) return;

    if (ShouldUseShellWindow()) {
        HWND shellW = GetDefaultShellWindow();
        if (hwnd == shellW) {
            for (int i = 0; i < 5 && !CheckDesktopState(*s_ctx, desktopHost); ++i)
                Sleep(2);
        }
    } else {
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        if (wcscmp(cls, L"WorkerW") == 0) {
            HWND dv = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", L"");
            if (dv) {
                for (int i = 0; i < 5 && !CheckDesktopState(*s_ctx, desktopHost); ++i)
                    Sleep(2);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Resolve dynamic text based on layer kind
// ---------------------------------------------------------------------------

static std::wstring ResolveLayerText(const TextLayer& layer) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    switch (layer.kind) {
    case TextKind::Time: {
        wchar_t buf[16];
        swprintf(buf, 16, L"%02d:%02d", st.wHour, st.wMinute);
        return buf;
    }
    case TextKind::Date: {
        static const wchar_t* months[] = {
            L"January", L"February", L"March", L"April", L"May", L"June",
            L"July", L"August", L"September", L"October", L"November", L"December"
        };
        wchar_t buf[32];
        swprintf(buf, 32, L"%d %ls", st.wDay, months[st.wMonth - 1]);
        return buf;
    }
    case TextKind::Weekday: {
        static const wchar_t* days[] = {
            L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
            L"Thursday", L"Friday", L"Saturday"
        };
        return days[st.wDayOfWeek];
    }
    default:
        return layer.static_text;
    }
}

// ---------------------------------------------------------------------------
// Calculate absolute rectangle from percentage-based position
// ---------------------------------------------------------------------------

static Gdiplus::RectF CalcAnchorRect(double x_pct, double y_pct, double w_pct, double h_pct, TextAnchor anchor, int screenW, int screenH) {
    float centerX = screenW * x_pct / 100.0f;
    float centerY = screenH * y_pct / 100.0f;
    float width   = screenW * w_pct / 100.0f;
    float height  = screenH * h_pct / 100.0f;

    float left, top;

    switch (anchor) {
    case TextAnchor::TopLeft:
        left = centerX;
        top  = centerY;
        break;
    case TextAnchor::BottomLeft:
        left = centerX;
        top  = centerY - height;
        break;
    case TextAnchor::TopCenter:
        left = centerX - width / 2.0f;
        top  = centerY;
        break;
    case TextAnchor::BottomCenter:
        left = centerX - width / 2.0f;
        top  = centerY - height;
        break;
    case TextAnchor::Center:
    default:
        left = centerX - width / 2.0f;
        top  = centerY - height / 2.0f;
        break;
    }

    return Gdiplus::RectF(left, top, width, height);
}

// ---------------------------------------------------------------------------
// Draw a single text layer via GDI+
// ---------------------------------------------------------------------------

static void DrawTextLayer(Gdiplus::Graphics& graphics, const TextLayer& layer, int screenW, int screenH) {
    std::wstring text = ResolveLayerText(layer);
    if (text.empty()) return;

    Gdiplus::RectF rect = CalcAnchorRect(layer.x_percent, layer.y_percent, layer.width_percent, layer.height_percent, layer.anchor, screenW, screenH);

    Gdiplus::Font font(layer.font_family.c_str(), layer.font_size, layer.font_style, Gdiplus::UnitPixel);
    if (font.GetLastStatus() != Gdiplus::Ok) {
        LOG_ERR << "Overlay: 字体 '" << layer.font_family << "' 创建失败，跳过渲染";
        return;
    }

    Gdiplus::StringFormat format;
    format.SetAlignment(static_cast<Gdiplus::StringAlignment>(layer.align));
    format.SetLineAlignment(static_cast<Gdiplus::StringAlignment>(layer.line_align));

    // Shadow first
    if (layer.shadow.enabled) {
        Gdiplus::SolidBrush shadowBrush(
            Gdiplus::Color(layer.shadow.color.a, layer.shadow.color.r,
                          layer.shadow.color.g, layer.shadow.color.b));
        Gdiplus::RectF shadowRect = rect;
        shadowRect.X += layer.shadow.offset_x;
        shadowRect.Y += layer.shadow.offset_y;
        graphics.DrawString(text.c_str(), -1, &font, shadowRect, &format, &shadowBrush);
    }

    // Normal text
    Gdiplus::SolidBrush textBrush(
        Gdiplus::Color(layer.color.a, layer.color.r, layer.color.g, layer.color.b));
    graphics.DrawString(text.c_str(), -1, &font, rect, &format, &textBrush);
}

// ---------------------------------------------------------------------------
// Draw a single audio spectrum layer via GDI+
// ---------------------------------------------------------------------------

static void DrawAudioSpectrumLayer(Gdiplus::Graphics& graphics, const AudioSpectrumLayer& layer, int screenW, int screenH, const float* bands) {
    Gdiplus::RectF rect = CalcAnchorRect(layer.x_percent, layer.y_percent, layer.width_percent, layer.height_percent, layer.anchor, screenW, screenH);

    int numBands = std::min(layer.style.bands, kAudioBands);
    float barW = std::max(layer.style.radius * 2.0f, 3.0f);
    float gap = layer.style.gap;
    float minH = layer.style.min_height;
    float maxH = layer.style.max_height;

    Gdiplus::SolidBrush barBrush(Gdiplus::Color(layer.style.color.a, layer.style.color.r, layer.style.color.g, layer.style.color.b));

    float spacing = barW + gap;
    float totalWidth = numBands * spacing - gap;
    float startX = rect.X;

    switch (layer.anchor) {
    case TextAnchor::TopCenter:
    case TextAnchor::BottomCenter:
    case TextAnchor::Center:
        startX += (rect.Width - totalWidth) / 2.0f;
        break;
    default:
        break;
    }

    float baseline = rect.Y + rect.Height;
    float cornerR = std::min(barW / 2.0f, 4.0f);

    for (int i = 0; i < numBands; i++) {
        float magnitude = std::clamp(bands[i], 0.0f, 1.0f);
        float barH = minH + (maxH - minH) * magnitude;
        if (barH < 1.0f) barH = 1.0f;

        float x = startX + i * spacing;
        float y = baseline - barH;

        Gdiplus::GraphicsPath path;
        path.AddLine(x + cornerR, y, x + barW - cornerR, y);
        path.AddArc(x + barW - cornerR * 2.0f, y, cornerR * 2.0f, cornerR * 2.0f, 270.0f, 90.0f);
        path.AddLine(x + barW, y + cornerR, x + barW, baseline);
        path.AddLine(x, baseline, x, y + cornerR);
        path.AddArc(x, y, cornerR * 2.0f, cornerR * 2.0f, 180.0f, 90.0f);
        path.CloseFigure();

        graphics.FillPath(&barBrush, &path);
    }
}

// ---------------------------------------------------------------------------
// Full-screen render with all layers via UpdateLayeredWindow
// ---------------------------------------------------------------------------

static void RenderAndUpdate(Context& ctx) {
    if (!ctx.hwnd || !IsWindow(ctx.hwnd)) return;

    // Virtual screen coordinates (handles multi-monitor)
    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (screenW <= 0 || screenH <= 0) {
        screenX = 0;
        screenY = 0;
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
    }

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = screenW;
    bmi.bmiHeader.biHeight      = -screenH;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hbm) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    HGDIOBJ hOld = SelectObject(hdcMem, hbm);

    // GDI+ multi-layer render
    {
        Gdiplus::Graphics graphics(hdcMem);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        for (const auto& item : ctx.layers) {
            if (auto* text = std::get_if<TextLayer>(&item))
                DrawTextLayer(graphics, *text, screenW, screenH);
            else if (auto* audio = std::get_if<AudioSpectrumLayer>(&item))
                DrawAudioSpectrumLayer(graphics, *audio, screenW, screenH, ctx.audio_bands.data());
        }
    }

    // Update layered window (full virtual screen)
    SIZE szWindow = { screenW, screenH };
    POINT ptSrc   = { 0, 0 };
    POINT ptPos   = { screenX, screenY };
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    if (!UpdateLayeredWindow(ctx.hwnd, hdcScreen, &ptPos, &szWindow,
                             hdcMem, &ptSrc, 0, &blend, ULW_ALPHA))
        LOG_ERR << "Overlay: UpdateLayeredWindow 失败, err=" << GetLastError();

    SelectObject(hdcMem, hOld);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// ---------------------------------------------------------------------------
// Overlay window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_TIMER:
        if (wParam == TIMER_SHOWDESKTOP && s_ctx) {
            HWND host = GetDesktopIconsHostWindow();
            CheckDesktopState(*s_ctx, host);
        }
        return 0;

    case WM_WINDOWPOSCHANGING: {
        LPWINDOWPOS wp = reinterpret_cast<LPWINDOWPOS>(lParam);
        wp->flags |= SWP_NOZORDER;
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool Init(Context& ctx, const Config& cfg, HINSTANCE hInstance) {
    ctx.hInstance = hInstance;
    s_ctx = &ctx;

    // Load widget config from public/widgets/*.json
    {
        auto exeDir = path::GetExeDir();
        auto loaded = LoadWidgetConfig(exeDir, cfg.desktop_overlay_widgets_dir,
                                       cfg.desktop_overlay_widget_order);
        if (loaded.empty()) {
            LOG_ERR << "Overlay: 组件加载失败，终止初始化";
            s_ctx = nullptr;
            return false;
        }
        ctx.layers = std::move(loaded);
    }

    for (const auto& item : ctx.layers) {
        if (std::get_if<AudioSpectrumLayer>(&item)) {
            ctx.has_audio_spectrum = true;
            break;
        }
    }

    // GDI+ startup
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&ctx.gdiplus_token, &gdiplusInput, nullptr);
    if (gdiStatus != Gdiplus::Ok) {
        LOG_WARN << "Overlay: GDI+ 启动失败 (status=" << gdiStatus << ")";
        s_ctx = nullptr;
        return false;
    }

    // Register overlay window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInstance;
    wc.lpszClassName  = OVERLAY_CLASS;
    wc.hbrBackground  = nullptr;
    if (!RegisterClassW(&wc)) {
        LOG_WARN << "Overlay: 注册窗口类失败";
        Gdiplus::GdiplusShutdown(ctx.gdiplus_token);
        ctx.gdiplus_token = 0;
        s_ctx = nullptr;
        return false;
    }

    // Register helper window class
    WNDCLASSW hc = {};
    hc.lpfnWndProc   = HelperWndProc;
    hc.hInstance      = hInstance;
    hc.lpszClassName  = HELPER_CLASS;
    hc.hbrBackground  = nullptr;
    if (!RegisterClassW(&hc)) {
        LOG_WARN << "Overlay: 注册 helper 窗口类失败";
    }

    // Create overlay window
    ctx.hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        OVERLAY_CLASS,
        L"DynamicWallpaper Overlay",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr);

    if (!ctx.hwnd) {
        LOG_WARN << "Overlay: 创建窗口失败";
        Gdiplus::GdiplusShutdown(ctx.gdiplus_token);
        ctx.gdiplus_token = 0;
        s_ctx = nullptr;
        return false;
    }

    // Initialize last-known time so Tick() doesn't re-render immediately
    SYSTEMTIME st;
    GetLocalTime(&st);
    ctx.last_minute = st.wMinute;
    ctx.last_day = st.wDay;

    // First render
    RenderAndUpdate(ctx);

    // Create helper window
    ctx.helper_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        HELPER_CLASS, nullptr,
        WS_POPUP | WS_DISABLED,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr);

    if (ctx.helper_hwnd) {
        SetWindowPos(ctx.helper_hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    // Aero Peek exclusion
    BOOL bExclude = TRUE;
    DwmSetWindowAttribute(ctx.hwnd, DWMWA_EXCLUDED_FROM_PEEK, &bExclude, sizeof(bExclude));

    // Click-through (WS_EX_TRANSPARENT)
    LONG_PTR exStyle = GetWindowLongPtrW(ctx.hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(ctx.hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);

    // Initial Z-order
    SetWindowPos(ctx.hwnd, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);

    // WinEventHook for Show Desktop detection
    ctx.hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Timer for periodic fallback detection
    SetTimer(ctx.hwnd, TIMER_SHOWDESKTOP, INTERVAL_SHOWDESKTOP, nullptr);

    // Show window
    ShowWindow(ctx.hwnd, SW_SHOWNOACTIVATE);

    // Re-render after ShowWindow to ensure the window is visible first
    RenderAndUpdate(ctx);

    ctx.enabled = true;

    int vsW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vsH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    LOG_INFO << "Overlay: 初始化完成 (hwnd=0x" << std::hex << (ULONG_PTR)ctx.hwnd << std::dec
             << ") virtual_screen=" << vsW << "x" << vsH;
    return true;
}

void Tick(Context& ctx) {
    if (!ctx.enabled) return;

    SYSTEMTIME now;
    GetLocalTime(&now);

    bool needRedraw = false;

    if (now.wMinute != ctx.last_minute) {
        ctx.last_minute = now.wMinute;
        needRedraw = true;
    }

    if (now.wDay != ctx.last_day) {
        ctx.last_day = now.wDay;
        needRedraw = true;
    }

    if (ctx.has_audio_spectrum && ctx.audio_bands_updated) {
        ctx.audio_bands_updated = false;
        static int frame_count = 0;
        if (++frame_count >= 4) {
            frame_count = 0;
            needRedraw = true;
        }
    }

    if (needRedraw && ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

void Shutdown(Context& ctx) {
    if (!ctx.enabled) return;
    ctx.enabled = false;

    LOG_INFO << "Overlay: 开始清理";

    if (ctx.hwnd && IsWindow(ctx.hwnd))
        KillTimer(ctx.hwnd, TIMER_SHOWDESKTOP);

    if (ctx.hook) {
        UnhookWinEvent(ctx.hook);
        ctx.hook = nullptr;
    }

    if (ctx.helper_hwnd && IsWindow(ctx.helper_hwnd)) {
        DestroyWindow(ctx.helper_hwnd);
        ctx.helper_hwnd = nullptr;
    }

    if (ctx.hwnd && IsWindow(ctx.hwnd)) {
        DestroyWindow(ctx.hwnd);
        ctx.hwnd = nullptr;
    }

    if (ctx.gdiplus_token) {
        Gdiplus::GdiplusShutdown(ctx.gdiplus_token);
        ctx.gdiplus_token = 0;
    }

    s_ctx = nullptr;
    LOG_INFO << "Overlay: 清理完成";
}

void OnExplorerRestarted(Context& ctx) {
    if (!ctx.enabled) return;
    LOG_INFO << "Overlay: Explorer 重建，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

void OnDisplayChanged(Context& ctx, int width, int height) {
    if (!ctx.enabled) return;
    LOG_INFO << "Overlay: 显示器变化 (" << width << "x" << height << ")，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

void OnPowerResume(Context& ctx) {
    if (!ctx.enabled) return;
    LOG_INFO << "Overlay: 系统恢复，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

} // namespace desktop_overlay
