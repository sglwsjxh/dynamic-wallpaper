#include "shell/shell_controller.h"
#include "logs/log.h"

namespace shell {

static LRESULT CALLBACK ShellWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static const UINT uTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        LOG_INFO << "Shell: WM_NCCREATE, ctx=" << cs->lpCreateParams;
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    auto* ctx = reinterpret_cast<Context*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (msg == uTaskbarCreated) {
        LOG_INFO << "Shell: TaskbarCreated — Explorer 重启";
        if (ctx)
            ctx->explorer_recreated = true;
        return 0;
    }

    switch (msg) {
        case WM_DISPLAYCHANGE:
            if (ctx) {
                ctx->display_changed = true;
                ctx->display_width = LOWORD(lParam);
                ctx->display_height = HIWORD(lParam);
                LOG_INFO << "Shell: WM_DISPLAYCHANGE -> "
                         << ctx->display_width << "x" << ctx->display_height;
            }
            return 0;

        case WM_POWERBROADCAST:
            if (ctx && wParam == PBT_APMSUSPEND) {
                ctx->power_suspended = true;
                LOG_INFO << "Shell: 系统挂起";
            } else if (ctx && wParam == PBT_APMRESUMEAUTOMATIC) {
                ctx->power_resumed = true;
                LOG_INFO << "Shell: 系统恢复";
            }
            return 0;

        case WM_DESTROY:
            if (ctx && ctx->shutting_down) {
                LOG_INFO << "Shell: 控制器窗口正常销毁";
            } else {
                LOG_INFO << "Shell: 控制器窗口意外销毁，程序退出";
                PostQuitMessage(0);
            }
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Init(Context& ctx, HINSTANCE hInstance) {
    ctx.hInstance = hInstance;

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), 0, ShellWndProc, 0, 0,
                      hInstance, nullptr, nullptr, nullptr, nullptr,
                      L"DynamicWallpaperShellController", nullptr };

    ATOM atom = RegisterClassEx(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        LOG_ERR << "Shell: RegisterClassEx 失败, err=" << GetLastError();
        return false;
    }

    ctx.hwnd = CreateWindowEx(0, L"DynamicWallpaperShellController", L"",
                              WS_POPUP, 0, 0, 0, 0,
                              nullptr, nullptr, hInstance, &ctx);

    if (!ctx.hwnd) {
        LOG_ERR << "Shell: CreateWindowEx 失败, err=" << GetLastError();
        return false;
    }

    LOG_INFO << "Shell: 控制器窗口创建成功, hwnd=" << ctx.hwnd;
    return true;
}

void Shutdown(Context& ctx) {
    ctx.shutting_down = true;

    if (ctx.hwnd) {
        DestroyWindow(ctx.hwnd);
        ctx.hwnd = nullptr;
    }

    LOG_INFO << "Shell: 已关闭";
}

} // namespace shell
