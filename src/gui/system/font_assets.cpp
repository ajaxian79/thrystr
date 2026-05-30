// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "font_assets.hpp"

#include <thrystr/gui/icons.hpp>

#include <cstdio>

namespace thrystr::gui {

bool font_file_exists(const std::string& path) {
    if (FILE* file = std::fopen(path.c_str(), "rb")) {
        std::fclose(file);
        return true;
    }
    return false;
}

FontPaths font_paths(const char* font_directory) {
    const std::string dir = font_directory;
    return {dir + "/Inter-Regular.ttf", dir + "/Inter-Medium.ttf",
            dir + "/NotoSansMono-Regular.ttf", dir + "/ThrystrIcons.ttf"};
}

ImFont* load_body_font(ImGuiIO& io, const std::string& body, const std::string& icon_path,
                       float size_px, bool merge_icons) {
    ImFontConfig body_config;
    body_config.OversampleH = 3;
    ImFont* font = io.Fonts->AddFontFromFileTTF(body.c_str(), size_px, &body_config);
    if (!font || !merge_icons || !font_file_exists(icon_path)) {
        return font;
    }
    ImFontConfig icon_config;
    icon_config.MergeMode = true;
    static const ImWchar ranges[] = {icons::kRangeLo, icons::kRangeHi, 0};
    io.Fonts->AddFontFromFileTTF(icon_path.c_str(), size_px, &icon_config, ranges);
    return font;
}

} // namespace thrystr::gui
