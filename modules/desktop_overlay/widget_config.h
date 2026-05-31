#pragma once
#include <string>
#include <vector>

#include "desktop_overlay/widget_types.h"

namespace desktop_overlay {

// Load all enabled widgets from exeDir/widgetsDir/*.json.
// Returns empty vector on any error (directory missing, empty, parse failure).
// Caller must treat empty result as a fatal error.
std::vector<TextLayer> LoadWidgetConfig(const std::wstring& exeDir, const std::string& widgetsDir);

} // namespace desktop_overlay
