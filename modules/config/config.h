#pragma once
#include <string>
#include <vector>

struct Config {
    bool auto_startup = true;
    bool wallpaper_enabled = true;
    std::string background_path = "background.mp4";
    std::string wallpaper_gpu = "auto";
    bool desktop_overlay_enabled = false;
    std::string desktop_overlay_widgets_dir = "public/widgets";
    std::vector<std::string> desktop_overlay_widget_order;
};

Config LoadConfig();
