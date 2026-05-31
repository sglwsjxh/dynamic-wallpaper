#pragma once
#include <cstdint>
#include <string>

namespace desktop_overlay {

struct ColorRgba {
    uint8_t a = 255;
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
};

struct TextShadow {
    bool enabled = false;
    float offset_x = 1.0f;
    float offset_y = 1.0f;
    ColorRgba color;
};

enum class TextKind { StaticText, Time, Date, Weekday };
enum class TextAnchor { TopLeft, TopCenter, Center, BottomCenter };

struct TextLayer {
    std::wstring id;
    TextKind kind = TextKind::StaticText;

    float x_percent = 50.0f;
    float y_percent = 50.0f;
    float width_percent = 30.0f;
    float height_percent = 10.0f;
    TextAnchor anchor = TextAnchor::Center;

    std::wstring static_text;
    float font_size = 32.0f;
    int font_style = 0;          // Gdiplus::FontStyleRegular = 0
    std::wstring font_family = L"Microsoft YaHei UI";

    ColorRgba color;
    TextShadow shadow;

    int align = 1;               // Gdiplus::StringAlignmentCenter = 1
    int line_align = 1;          // Gdiplus::StringAlignmentCenter = 1
};

} // namespace desktop_overlay
