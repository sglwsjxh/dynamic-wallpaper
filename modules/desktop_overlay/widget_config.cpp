#include "desktop_overlay/widget_config.h"
#include "logs/log.h"

#include <windows.h>  // required before gdiplus.h (MinGW: defines PROPID etc.)
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <gdiplus.h>
#include <nlohmann/json.hpp>

namespace desktop_overlay {

static std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";

    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

// =========================================================================
// CSS-like color parser
//   Supported: #RRGGBB, #AARRGGBB, rgb(r,g,b), rgba(r,g,b,a),
//              white, black, transparent
//   Returns nullopt on invalid input.
// =========================================================================

static std::optional<ColorRgba> ParseCssColor(const std::wstring& value) {
    if (value.empty()) return std::nullopt;

    // Named colors
    if (value == L"white")      return ColorRgba{255, 255, 255, 255};
    if (value == L"black")      return ColorRgba{255, 0,   0,   0  };
    if (value == L"transparent") return ColorRgba{0,   0,   0,   0  };

    // #RRGGBB or #AARRGGBB
    if (value[0] == L'#') {
        size_t len = value.length() - 1;
        if (len != 6 && len != 8) return std::nullopt;

        // Validate hex chars
        for (size_t i = 1; i < value.length(); ++i)
            if (!iswxdigit(value[i])) return std::nullopt;

        ColorRgba result;
        unsigned long val = std::wcstoul(value.c_str() + 1, nullptr, 16);

        if (len == 6) {
            result.a = 255;
            result.r = (uint8_t)((val >> 16) & 0xFF);
            result.g = (uint8_t)((val >> 8) & 0xFF);
            result.b = (uint8_t)(val & 0xFF);
        } else {
            result.a = (uint8_t)((val >> 24) & 0xFF);
            result.r = (uint8_t)((val >> 16) & 0xFF);
            result.g = (uint8_t)((val >> 8) & 0xFF);
            result.b = (uint8_t)(val & 0xFF);
        }
        return result;
    }

    // rgb() / rgba()
    if (value.size() >= 4 && value[0] == L'r' && value[1] == L'g' && value[2] == L'b') {
        auto parenOpen  = value.find(L'(');
        auto parenClose = value.find(L')');
        if (parenOpen == std::wstring::npos || parenClose == std::wstring::npos)
            return std::nullopt;

        std::wstring inner = value.substr(parenOpen + 1, parenClose - parenOpen - 1);
        if (inner.empty()) return std::nullopt;

        // Split by comma
        std::vector<std::wstring> parts;
        size_t start = 0;
        while (start < inner.size()) {
            auto end = inner.find(L',', start);
            if (end == std::wstring::npos) {
                // Trim
                auto trimmed = inner.substr(start);
                while (!trimmed.empty() && trimmed[0] == L' ') trimmed.erase(0, 1);
                while (!trimmed.empty() && trimmed.back() == L' ') trimmed.pop_back();
                if (!trimmed.empty()) parts.push_back(trimmed);
                break;
            }
            auto trimmed = inner.substr(start, end - start);
            while (!trimmed.empty() && trimmed[0] == L' ') trimmed.erase(0, 1);
            while (!trimmed.empty() && trimmed.back() == L' ') trimmed.pop_back();
            if (!trimmed.empty()) parts.push_back(trimmed);
            start = end + 1;
        }

        if (parts.size() < 3) return std::nullopt;

        // Validate each part is numeric
        ColorRgba result;
        for (size_t i = 0; i < parts.size() && i < 4; ++i) {
            wchar_t* end = nullptr;
            double val = std::wcstod(parts[i].c_str(), &end);
            if (end && *end != L'\0') return std::nullopt; // non-numeric content
            if (i < 3) {
                auto ival = (uint8_t)std::min(std::max((int)val, 0), 255);
                if (i == 0) result.r = ival;
                if (i == 1) result.g = ival;
                if (i == 2) result.b = ival;
            }
            if (i == 3) {
                double a = val;
                if (a <= 1.0) a *= 255.0;
                result.a = (uint8_t)std::min(std::max((int)a, 0), 255);
            }
        }
        if (parts.size() < 4) result.a = 255;
        return result;
    }

    return std::nullopt;
}

// =========================================================================
// Enum / value mapping helpers
// =========================================================================

static std::optional<TextKind> ParseTextKind(const std::string& s) {
    if (s == "static")  return TextKind::StaticText;
    if (s == "time")    return TextKind::Time;
    if (s == "date")    return TextKind::Date;
    if (s == "weekday") return TextKind::Weekday;
    return std::nullopt;
}

static std::optional<TextAnchor> ParseAnchor(const std::string& s) {
    if (s == "top_left")      return TextAnchor::TopLeft;
    if (s == "top_center")    return TextAnchor::TopCenter;
    if (s == "center")        return TextAnchor::Center;
    if (s == "bottom_center") return TextAnchor::BottomCenter;
    return std::nullopt;
}

static std::optional<int> ParseFontStyle(const std::string& s) {
    if (s == "regular")     return Gdiplus::FontStyleRegular;
    if (s == "bold")        return Gdiplus::FontStyleBold;
    if (s == "italic")      return Gdiplus::FontStyleItalic;
    if (s == "bold_italic") return Gdiplus::FontStyleBoldItalic;
    return std::nullopt;
}

static std::optional<int> ParseAlignment(const std::string& s) {
    if (s == "near")   return Gdiplus::StringAlignmentNear;
    if (s == "center") return Gdiplus::StringAlignmentCenter;
    if (s == "far")    return Gdiplus::StringAlignmentFar;
    return std::nullopt;
}

// =========================================================================
// Parse a single widget JSON file into a TextLayer
// =========================================================================

static std::optional<TextLayer> ParseWidgetJson(
    const nlohmann::json& j, const std::filesystem::path& filePath)
{
    TextLayer layer;

    // --- id (required) ---
    {
        auto it = j.find("id");
        if (it == j.end() || !it->is_string()) {
            LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'id'";
            return std::nullopt;
        }
        std::string id = *it;
        if (id.empty()) {
            LOG_ERR << "Overlay: " << filePath << " 'id' 为空";
            return std::nullopt;
        }
        layer.id = Utf8ToWide(id);
    }

    // --- type (required) ---
    {
        auto it = j.find("type");
        if (it == j.end() || !it->is_string()) {
            LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'type'";
            return std::nullopt;
        }
        std::string typeStr = *it;
        auto kind = ParseTextKind(typeStr);
        if (!kind.has_value()) {
            LOG_ERR << "Overlay: " << filePath << " 非法 type '" << typeStr << "'";
            return std::nullopt;
        }
        layer.kind = *kind;
    }

    // --- position / style / static_text (wrapped for type exceptions) ---
    // json::value() throws if key exists but type mismatches (e.g. "50" instead of 50).
    // We catch and log instead of letting it propagate as unhandled exception.
    try {
    // --- position (required) ---
    {
        auto it = j.find("position");
        if (it == j.end() || !it->is_object()) {
            LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'position'";
            return std::nullopt;
        }
        const auto& pos = *it;

        layer.x_percent     = pos.value("x_percent", 50.0f);
        layer.y_percent     = pos.value("y_percent", 50.0f);
        layer.width_percent = pos.value("width_percent", 30.0f);
        layer.height_percent = pos.value("height_percent", 10.0f);

        auto anchorStr = pos.value("anchor", std::string("center"));
        auto anchor = ParseAnchor(anchorStr);
        if (!anchor.has_value()) {
            LOG_ERR << "Overlay: " << filePath << " 非法 position.anchor '" << anchorStr << "'";
            return std::nullopt;
        }
        layer.anchor = *anchor;
    }

    // --- style (required) ---
    {
        auto it = j.find("style");
        if (it == j.end() || !it->is_object()) {
            LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'style'";
            return std::nullopt;
        }
        const auto& style = *it;

        // font_family (required)
        {
            auto ffIt = style.find("font_family");
            if (ffIt == style.end() || !ffIt->is_string()) {
                LOG_ERR << "Overlay: " << filePath << " 缺少 'style.font_family'";
                return std::nullopt;
            }
            std::string ff = *ffIt;
            if (ff.empty()) {
                LOG_ERR << "Overlay: " << filePath << " 'style.font_family' 为空";
                return std::nullopt;
            }
            layer.font_family = Utf8ToWide(ff);
        }

        // font_size (required)
        {
            auto fsIt = style.find("font_size");
            if (fsIt == style.end() || !fsIt->is_number()) {
                LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'style.font_size'";
                return std::nullopt;
            }
            layer.font_size = fsIt->get<float>();
        }

        // font_style (required)
        {
            auto fsIt = style.find("font_style");
            if (fsIt == style.end() || !fsIt->is_string()) {
                LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'style.font_style'";
                return std::nullopt;
            }
            std::string fst = *fsIt;
            auto fs = ParseFontStyle(fst);
            if (!fs.has_value()) {
                LOG_ERR << "Overlay: " << filePath << " 非法 font_style '" << fst << "'";
                return std::nullopt;
            }
            layer.font_style = *fs;
        }

        // color (required)
        {
            auto cIt = style.find("color");
            if (cIt == style.end() || !cIt->is_string()) {
                LOG_ERR << "Overlay: " << filePath << " 缺少或无效的 'style.color'";
                return std::nullopt;
            }
            std::string colorStr = *cIt;
            auto wcolor = Utf8ToWide(colorStr);
            auto color = ParseCssColor(wcolor);
            if (!color.has_value()) {
                LOG_ERR << "Overlay: " << filePath << " 非法 color '" << colorStr << "'";
                return std::nullopt;
            }
            layer.color = *color;
        }

        // align (optional, default center)
        layer.align = Gdiplus::StringAlignmentCenter;
        {
            auto aIt = style.find("align");
            if (aIt != style.end()) {
                if (!aIt->is_string()) {
                    LOG_ERR << "Overlay: " << filePath << " 'style.align' 类型错误";
                    return std::nullopt;
                }
                std::string as = *aIt;
                auto al = ParseAlignment(as);
                if (!al.has_value()) {
                    LOG_ERR << "Overlay: " << filePath << " 非法 align '" << as << "'";
                    return std::nullopt;
                }
                layer.align = *al;
            }
        }

        // line_align (optional, default center)
        layer.line_align = Gdiplus::StringAlignmentCenter;
        {
            auto laIt = style.find("line_align");
            if (laIt != style.end()) {
                if (!laIt->is_string()) {
                    LOG_ERR << "Overlay: " << filePath << " 'style.line_align' 类型错误";
                    return std::nullopt;
                }
                std::string las = *laIt;
                auto la = ParseAlignment(las);
                if (!la.has_value()) {
                    LOG_ERR << "Overlay: " << filePath << " 非法 line_align '" << las << "'";
                    return std::nullopt;
                }
                layer.line_align = *la;
            }
        }

        // shadow (optional)
        {
            auto shIt = style.find("shadow");
            if (shIt != style.end()) {
                if (!shIt->is_object()) {
                    LOG_ERR << "Overlay: " << filePath << " 'style.shadow' 类型错误";
                    return std::nullopt;
                }
                const auto& shadow = *shIt;
                layer.shadow.enabled  = shadow.value("enabled", false);
                layer.shadow.offset_x = shadow.value("offset_x", 1.0f);
                layer.shadow.offset_y = shadow.value("offset_y", 1.0f);

                auto scIt = shadow.find("color");
                if (scIt != shadow.end()) {
                    if (!scIt->is_string()) {
                        LOG_ERR << "Overlay: " << filePath << " 'shadow.color' 类型错误";
                        return std::nullopt;
                    }
                    std::string scs = *scIt;
                    auto wscs = Utf8ToWide(scs);
                    auto sc = ParseCssColor(wscs);
                    if (!sc.has_value()) {
                        LOG_ERR << "Overlay: " << filePath << " 非法 shadow.color '" << scs << "'";
                        return std::nullopt;
                    }
                    layer.shadow.color = *sc;
                }
            }
        }
    }

    // --- static_text (optional, only meaningful for type=static) ---
    {
        auto it = j.find("static_text");
        if (it != j.end() && it->is_string()) {
            std::string st = *it;
            layer.static_text = Utf8ToWide(st);
        }
    }
    } catch (const std::exception& e) {
        LOG_ERR << "Overlay: " << filePath << " 配置解析异常: " << e.what();
        return std::nullopt;
    }

    return layer;
}

// =========================================================================
// Load all widget JSON files from the widgets directory
// =========================================================================

std::vector<TextLayer> LoadWidgetConfig(
    const std::wstring& exeDir, const std::string& widgetsDir,
    const std::vector<std::string>& order)
{
    std::vector<TextLayer> layers;
    std::unordered_map<std::wstring, TextLayer> byId;

    auto dir = std::filesystem::path(exeDir) / widgetsDir;

    // Directory must exist
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        LOG_ERR << "Overlay: widgets 目录不存在: " << dir;
        return layers; // empty
    }

    // Collect all .json files
    std::vector<std::filesystem::path> files;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".json")
            files.push_back(entry.path());
    }

    if (files.empty()) {
        LOG_ERR << "Overlay: widgets 目录中没有 .json 文件: " << dir;
        return layers; // empty
    }

    for (auto& filePath : files) {
        // Open
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_ERR << "Overlay: 无法打开组件文件: " << filePath;
            return {}; // empty
        }

        // Parse JSON
        nlohmann::json j;
        try {
            file >> j;
        } catch (const std::exception& e) {
            LOG_ERR << "Overlay: " << filePath << " JSON 解析失败: " << e.what();
            return {}; // empty
        }

        // Skip if not enabled
        if (!j.value("enabled", true))
            continue;

        // Parse into TextLayer
        auto layer = ParseWidgetJson(j, filePath);
        if (!layer.has_value())
            return {}; // error already logged by ParseWidgetJson

        if (order.empty()) {
            // No ordering — push in collection order
            layers.push_back(std::move(*layer));
        } else {
            // Buffer by id for order-based sorting
                byId[layer->id] = std::move(*layer);
        }
    }

    if (order.empty()) {
        if (layers.empty())
            LOG_ERR << "Overlay: 没有启用的组件 (所有 enabled=false)";
        return layers;
    }

    // Order defines top-to-bottom: first id = topmost (drawn last)
    layers.clear();
    layers.reserve(order.size());
    for (const auto& id : order) {
        auto wideId = Utf8ToWide(id);
        auto entry = byId.find(wideId);
        if (entry == byId.end()) {
            LOG_ERR << "Overlay: order 中指定的 id '" << id << "' 不存在";
            return {}; // empty
        }
        layers.push_back(std::move(entry->second));
        byId.erase(entry);
    }

    // Append any widgets not mentioned in order
    for (auto& pair : byId)
        layers.push_back(std::move(pair.second));

    return layers;
}

} // namespace desktop_overlay
