#include "desktop_overlay/desktop_overlay.h"
#include "logs/log.h"

#include <dwmapi.h>
#include <gdiplus.h>
#include <cstdio>

namespace desktop_overlay {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define OVERLAY_CLASS  L"DynamicWallpaperDesktopOverlay"
#define HELPER_CLASS   L"DynamicWallpaperOverlayHelper"
#define OVERLAY_TEXT   L"\u8FD9\u662F\u4E00\u4E2A\u6D4B\u8BD5\u3002"  // 这是一个测试。
#define TEXT_X         200
#define TEXT_Y         200
#define TEXT_W         200
#define TEXT_H         60

#define TIMER_SHOWDESKTOP  1
#define INTERVAL_SHOWDESKTOP    250

#define ZPOS_FLAGS (SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING)

// ---------------------------------------------------------------------------
// Module-internal bridge for WinEventProc (cannot pass user data via hook)
// ---------------------------------------------------------------------------

static Context* s_ctx = nullptr;

// ---------------------------------------------------------------------------
// Win11 24H2 detection (same as test.cpp / Rainmeter approach)
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
        return 0;
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
// Render overlay text via GDI+ and UpdateLayeredWindow
// ---------------------------------------------------------------------------

static void RenderAndUpdate(Context& ctx) {
    if (!ctx.hwnd || !IsWindow(ctx.hwnd)) return;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = TEXT_W;
    bmi.bmiHeader.biHeight      = -TEXT_H;
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

    // GDI+ draw
    {
        Gdiplus::Graphics graphics(hdcMem);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        Gdiplus::FontFamily fontFamily(L"Microsoft YaHei UI");
        Gdiplus::Font font(&fontFamily, 28.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::RectF rect(0, 0, (Gdiplus::REAL)TEXT_W, (Gdiplus::REAL)TEXT_H);
        graphics.DrawString(OVERLAY_TEXT, -1, &font, rect, &format, &brush);
    }

    SIZE szWindow = { TEXT_W, TEXT_H };
    POINT ptSrc   = { 0, 0 };
    POINT ptPos   = { TEXT_X, TEXT_Y };
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;

    UpdateLayeredWindow(ctx.hwnd, hdcScreen, &ptPos, &szWindow,
                        hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

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
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        // 不调用 PostQuitMessage — 主程序退出由 app 层控制
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool Init(Context& ctx, HINSTANCE hInstance) {
    ctx.hInstance = hInstance;
    s_ctx = &ctx;

    // --- GDI+ startup (once for the module lifetime) ---
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::Status gdiStatus = Gdiplus::GdiplusStartup(&ctx.gdiplus_token, &gdiplusInput, nullptr);
    if (gdiStatus != Gdiplus::Ok) {
        LOG_WARN << "Overlay: GDI+ 启动失败 (status=" << gdiStatus << ")";
        s_ctx = nullptr;
        return false;
    }

    // --- Register overlay window class ---
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

    // --- Register helper window class ---
    WNDCLASSW hc = {};
    hc.lpfnWndProc   = HelperWndProc;
    hc.hInstance      = hInstance;
    hc.lpszClassName  = HELPER_CLASS;
    hc.hbrBackground  = nullptr;
    if (!RegisterClassW(&hc)) {
        LOG_WARN << "Overlay: 注册 helper 窗口类失败";
        // 不返回 false — overlay 窗口仍可用，只是没有 Show Desktop 检测
    }

    // --- Create overlay window ---
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

    // --- First render (after hwnd is assigned) ---
    RenderAndUpdate(ctx);

    // --- Create helper window (permanent Z-order anchor) ---
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

    // --- Aero Peek exclusion ---
    BOOL bExclude = TRUE;
    DwmSetWindowAttribute(ctx.hwnd, DWMWA_EXCLUDED_FROM_PEEK, &bExclude, sizeof(bExclude));

    // --- Click-through (WS_EX_TRANSPARENT) ---
    LONG_PTR exStyle = GetWindowLongPtrW(ctx.hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(ctx.hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);

    // --- Initial Z-order ---
    SetWindowPos(ctx.hwnd, HWND_BOTTOM, 0, 0, 0, 0, ZPOS_FLAGS);

    // --- WinEventHook for Show Desktop detection ---
    ctx.hook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc,
        0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // --- Timer for periodic fallback detection ---
    SetTimer(ctx.hwnd, TIMER_SHOWDESKTOP, INTERVAL_SHOWDESKTOP, nullptr);

    // --- Show window ---
    ShowWindow(ctx.hwnd, SW_SHOWNOACTIVATE);

    LOG_INFO << "Overlay: 初始化完成 (hwnd=0x" << std::hex << (ULONG_PTR)ctx.hwnd << std::dec << ")";
    return true;
}

void Tick(Context& ctx) {
    // Tick 中无需额外操作 — Timer 和 WinEventHook 驱动状态检测。
    // 保留 Tick 接口以保持与其他模块一致。
}

void Shutdown(Context& ctx) {
    LOG_INFO << "Overlay: 开始清理";

    if (ctx.hwnd && IsWindow(ctx.hwnd)) {
        KillTimer(ctx.hwnd, TIMER_SHOWDESKTOP);
    }

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
    LOG_INFO << "Overlay: Explorer 重建，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

void OnDisplayChanged(Context& ctx, int width, int height) {
    LOG_INFO << "Overlay: 显示器变化 (" << width << "x" << height << ")，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

void OnPowerResume(Context& ctx) {
    LOG_INFO << "Overlay: 系统恢复，重新渲染";
    if (ctx.hwnd && IsWindow(ctx.hwnd))
        RenderAndUpdate(ctx);
}

} // namespace desktop_overlay
