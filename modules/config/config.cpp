#include "config/config.h"
#include "logs/log.h"
#include "path/path.h"

#include <algorithm>
#include <cctype>
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

        // gpu preference: auto / integrated / discrete / explicit adapter name
        if (dw.contains("gpu") && !dw["gpu"].is_string()) {
            LOG_WARN << "config: gpu 类型错误，回退为 auto";
            cfg.wallpaper_gpu = "auto";
        } else {
            std::string raw = dw.value("gpu", "auto");
            // trim 首尾空白
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t\n\r");
                if (start == std::string::npos) {
                    s.clear();
                    return;
                }
                size_t end = s.find_last_not_of(" \t\n\r");
                s = s.substr(start, end - start + 1);
            };
            trim(raw);
            // 转小写仅用于比较
            std::string lower = raw;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower == "auto" || lower == "integrated" || lower == "discrete") {
                cfg.wallpaper_gpu = lower;
            } else if (raw.empty()) {
                LOG_WARN << "config: gpu 为空，回退为 auto";
                cfg.wallpaper_gpu = "auto";
            } else {
                // 显式适配器名：保留原大小写，仅 trim
                cfg.wallpaper_gpu = raw;
            }
        }

        auto overlay = j.value("desktop_overlay", nlohmann::json::object());
        cfg.desktop_overlay_enabled = overlay.value("enabled", false);
        cfg.desktop_overlay_widgets_dir = overlay.value("widgets_dir", "public/widgets");

        if (overlay.contains("order") && overlay["order"].is_array()) {
            for (auto& id : overlay["order"]) {
                if (id.is_string())
                    cfg.desktop_overlay_widget_order.push_back(id.get<std::string>());
            }
        }

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
