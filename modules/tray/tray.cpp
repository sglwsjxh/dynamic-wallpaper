#include "tray/tray.h"
#include "logs/log.h"

#include <shellapi.h>
#include <windowsx.h>
#include <cwchar>
#include <cstdint>
#include <vector>

namespace tray {

namespace {

constexpr UINT ID_TRAY_RECREATE_WALLPAPER = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;

static void CopyTip(wchar_t* dest, size_t count, const wchar_t* text) {
    if (!dest || count == 0) return;
    wcsncpy(dest, text, count - 1);
    dest[count - 1] = L'\0';
}

static bool AddIcon(Context& ctx) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ctx.owner;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = ctx.icon;
    CopyTip(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), L"Dynamic Wallpaper");

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        LOG_ERR << "Tray: Shell_NotifyIconW(NIM_ADD) 失败, err=" << GetLastError();
        return false;
    }

    nid.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &nid))
        LOG_WARN << "Tray: NIM_SETVERSION 失败, err=" << GetLastError();

    ctx.added = true;
    LOG_INFO << "Tray: 托盘图标已添加";
    return true;
}

static void ShowMenu(Context& ctx, int x, int y) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        LOG_ERR << "Tray: CreatePopupMenu 失败";
        return;
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_RECREATE_WALLPAPER, L"重建壁纸");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出");

    SetForegroundWindow(ctx.owner);

    UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                              x, y, 0, ctx.owner, nullptr);

    PostMessage(ctx.owner, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (cmd == ID_TRAY_RECREATE_WALLPAPER) {
        LOG_INFO << "Tray: 菜单 — 重建壁纸";
        ctx.pending_command = Command::RecreateWallpaper;
    } else if (cmd == ID_TRAY_EXIT) {
        LOG_INFO << "Tray: 菜单 — 退出";
        ctx.pending_command = Command::Exit;
    }
}

static HICON CreateGeneratedIcon() {
    const int cx = 32;
    const int cy = 32;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return nullptr;

    void* bits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);

    if (!hbmColor || !bits) {
        if (hbmColor) DeleteObject(hbmColor);
        return nullptr;
    }

    auto* pixels = static_cast<uint32_t*>(bits);
    memset(pixels, 0, (size_t)cx * cy * sizeof(uint32_t));

    const int m = 2;
    const int cr = 6;

    for (int y = 0; y < cy; y++) {
        for (int x = 0; x < cx; x++) {
            if (x < m || x >= cx - m || y < m || y >= cy - m) continue;

            // 四角圆角裁剪（坐标在圆角半径外则跳过）
            bool skip = false;
            if (x < m + cr && y < m + cr) {
                int dx = x - (m + cr), dy = y - (m + cr);
                if (dx * dx + dy * dy > cr * cr) skip = true;
            } else if (x >= cx - m - cr && y < m + cr) {
                int dx = x - (cx - m - cr), dy = y - (m + cr);
                if (dx * dx + dy * dy > cr * cr) skip = true;
            } else if (x < m + cr && y >= cy - m - cr) {
                int dx = x - (m + cr), dy = y - (cy - m - cr);
                if (dx * dx + dy * dy > cr * cr) skip = true;
            } else if (x >= cx - m - cr && y >= cy - m - cr) {
                int dx = x - (cx - m - cr), dy = y - (cy - m - cr);
                if (dx * dx + dy * dy > cr * cr) skip = true;
            }
            if (skip) continue;

            float t = (float)(x + y) / (float)(cx + cy - 2);
            uint8_t rr = (uint8_t)(20 + (76 - 20) * t);
            uint8_t gg = (uint8_t)(30 + (29 - 30) * t);
            uint8_t bb = (uint8_t)(60 + (149 - 60) * t);
            pixels[y * cx + x] = 0xFF000000 | (bb << 16) | (gg << 8) | rr;
        }
    }

    const int sx = 7, sy = 10, sw = 18, sh = 13;
    for (int y = sy; y < sy + sh && y < cy; y++) {
        for (int x = sx; x < sx + sw && x < cx; x++) {
            bool edge = (y == sy || y == sy + sh - 1 || x == sx || x == sx + sw - 1);
            pixels[y * cx + x] = edge ? 0xFFB0B0C0 : 0xFF1A1A2E;
        }
    }

    const int tl = 14, tt = 14, tb = 22, tTip = 22;
    const int tMid = (tt + tb) / 2;
    for (int y = tt; y <= tb && y < cy; y++) {
        int width;
        if (y < tMid) {
            width = (tTip - tl) * (y - tt) / (tMid - tt);
        } else {
            width = (tTip - tl) * (tb - y) / (tb - tMid);
        }
        for (int x = tl; x <= tl + width && x < cx; x++) {
            pixels[y * cx + x] = 0xFFFFFFFF;
        }
    }

    // 单色 mask 全 0，32bpp alpha 决定透明区域
    std::vector<uint8_t> maskBits((size_t)cx * cy / 8, 0);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, maskBits.data());
    if (!hbmMask) {
        DeleteObject(hbmColor);
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = hbmColor;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbmMask);
    DeleteObject(hbmColor);

    return hIcon;
}

} // anonymous namespace

bool Init(Context& ctx, HWND owner) {
    if (!owner) {
        LOG_ERR << "Tray: owner hwnd 为空";
        return false;
    }

    ctx.owner = owner;
    ctx.icon = CreateGeneratedIcon();
    ctx.owns_icon = (ctx.icon != nullptr);

    if (!ctx.icon) {
        LOG_WARN << "Tray: 自定义图标创建失败，使用系统默认图标";
        ctx.icon = LoadIcon(nullptr, IDI_APPLICATION);
        ctx.owns_icon = false;
    }

    ctx.added = false;
    ctx.pending_command = Command::None;

    return AddIcon(ctx);
}

void Shutdown(Context& ctx) {
    if (ctx.added) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = ctx.owner;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        ctx.added = false;
        LOG_INFO << "Tray: 托盘图标已删除";
    }

    if (ctx.owns_icon && ctx.icon) {
        DestroyIcon(ctx.icon);
        ctx.icon = nullptr;
        ctx.owns_icon = false;
    }
}

void OnExplorerRestarted(Context& ctx) {
    if (!ctx.owner) return;

    LOG_INFO << "Tray: Explorer 重建，重新添加托盘图标";
    ctx.added = false;
    AddIcon(ctx);
}

void HandleMessage(Context& ctx, WPARAM wParam, LPARAM lParam) {
    UINT event = LOWORD(lParam);

    if (event == WM_CONTEXTMENU) {
        int x = GET_X_LPARAM(wParam);
        int y = GET_Y_LPARAM(wParam);
        ShowMenu(ctx, x, y);
    } else if (event == NIN_SELECT || event == NIN_KEYSELECT) {
        POINT pt{};
        GetCursorPos(&pt);
        ShowMenu(ctx, pt.x, pt.y);
    }
}

Command ConsumeCommand(Context& ctx) {
    Command cmd = ctx.pending_command;
    ctx.pending_command = Command::None;
    return cmd;
}

} // namespace tray
