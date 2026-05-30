// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

#include <string>

namespace thrystr::gui {

struct FontPaths {
    std::string sans;
    std::string medium;
    std::string mono;
    std::string icons;
};

bool font_file_exists(const std::string& path);
FontPaths font_paths(const char* font_directory);
ImFont* load_body_font(ImGuiIO& io, const std::string& body, const std::string& icon_path,
                       float size_px, bool merge_icons);

} // namespace thrystr::gui
