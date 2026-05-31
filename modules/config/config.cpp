#include "config/config.h"
#include "logs/log.h"
#include "path/path.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

Config LoadConfig() {
    auto exeDir = path::GetExeDir();
    auto configPath = std::filesystem::path(exeDir) / L"config.json";

    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOG_ERR << "config.json 未找到";
        std::exit(1);
    }

    try {
        nlohmann::json j;
        file >> j;

        Config cfg;
        cfg.auto_startup = j.value("auto_startup", true);

        auto dw = j.value("dynamic_wallpaper", nlohmann::json::object());
        cfg.wallpaper_enabled = dw.value("enabled", true);
        cfg.background_path   = dw.value("background_path", "background.mp4");

        auto overlay = j.value("desktop_overlay", nlohmann::json::object());
        cfg.desktop_overlay_enabled = overlay.value("enabled", false);
        cfg.desktop_overlay_widgets_dir = overlay.value("widgets_dir", "public/widgets");

        LOG_INFO << "配置: auto_startup=" << (cfg.auto_startup ? "true" : "false")
                 << " wallpaper_enabled=" << (cfg.wallpaper_enabled ? "true" : "false")
                 << " path=" << cfg.background_path
                 << " overlay=" << (cfg.desktop_overlay_enabled ? "true" : "false")
                 << " widgets_dir=" << cfg.desktop_overlay_widgets_dir;

        return cfg;
    } catch (const std::exception& e) {
        LOG_ERR << "config.json 解析失败: " << e.what();
        std::exit(1);
    }
}
