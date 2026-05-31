#pragma once
#include <string>

struct Config {
    bool auto_startup = true;
    bool wallpaper_enabled = true;
    std::string background_path = "background.mp4";
    bool desktop_overlay_enabled = false;
};

Config LoadConfig();
