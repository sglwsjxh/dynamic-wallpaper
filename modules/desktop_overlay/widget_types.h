#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

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
enum class TextAnchor { TopLeft, TopCenter, Center, BottomCenter, BottomLeft };

// =========================================================================
// Audio spectrum widget types
// =========================================================================

struct AudioSpectrumStyle {
    int bands = 96;
    float radius = 2.0f;
    float gap = 5.0f;
    float min_height = 2.0f;
    float max_height = 34.0f;
    float sensitivity = 2.0f;
    float smoothing = 0.78f;
    bool dotted = true;
    ColorRgba color;
};

struct AudioSpectrumLayer {
    std::wstring id;

    float x_percent = 49.0f;
    float y_percent = 70.0f;
    float width_percent = 47.0f;
    float height_percent = 6.0f;
    TextAnchor anchor = TextAnchor::BottomLeft;

    AudioSpectrumStyle style;
};

// =========================================================================
// Text widget types (unchanged)
// =========================================================================

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

using WidgetItem = std::variant<TextLayer, AudioSpectrumLayer>;

} // namespace desktop_overlay
