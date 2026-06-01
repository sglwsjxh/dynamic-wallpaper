#pragma once
#include <string>
#include <vector>

#include "desktop_overlay/widget_types.h"

namespace desktop_overlay {

// Load all enabled widgets from exeDir/widgetsDir/*.json.
// If order is non-empty, layers are arranged according to the id list.
// Returns empty vector on any error (directory missing, empty, parse failure).
// Caller must treat empty result as a fatal error.
std::vector<WidgetItem> LoadWidgetConfig(
    const std::wstring& exeDir, const std::string& widgetsDir,
    const std::vector<std::string>& order = {});

} // namespace desktop_overlay
