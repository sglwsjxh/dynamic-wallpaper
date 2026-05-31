#pragma once
#include <string>

struct Config {
    bool auto_startup = true;
    bool wallpaper_enabled = true;
    std::string background_path = "background.mp4";
    bool desktop_overlay_enabled = false;
    std::string desktop_overlay_widgets_dir = "public/widgets";
};

Config LoadConfig();
