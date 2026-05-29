#include "tray/tray.h"
#include "logs/log.h"

#include <shellapi.h>
#include <windowsx.h>

namespace tray {

namespace {

constexpr UINT ID_TRAY_RECREATE_WALLPAPER = 1001;
constexpr UINT ID_TRAY_EXIT = 1002;

static bool AddIcon(Context& ctx) {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ctx.owner;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = ctx.icon;
    wcscpy_s(nid.szTip, L"Dynamic Wallpaper");

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

} // anonymous namespace

bool Init(Context& ctx, HWND owner) {
    if (!owner) {
        LOG_ERR << "Tray: owner hwnd 为空";
        return false;
    }

    ctx.owner = owner;
    ctx.icon = LoadIcon(nullptr, IDI_APPLICATION);
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
