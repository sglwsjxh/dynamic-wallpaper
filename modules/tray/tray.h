#pragma once
#include <windows.h>

namespace tray {

constexpr UINT WM_TRAY = WM_APP + 100;

enum class Command {
    None,
    RecreateWallpaper,
    Exit,
};

struct Context {
    HWND owner = nullptr;
    HICON icon = nullptr;
    bool added = false;
    Command pending_command = Command::None;
};

bool Init(Context& ctx, HWND owner);
void Shutdown(Context& ctx);

void OnExplorerRestarted(Context& ctx);
void HandleMessage(Context& ctx, WPARAM wParam, LPARAM lParam);

Command ConsumeCommand(Context& ctx);

} // namespace tray
