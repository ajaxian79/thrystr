// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/font_loader.hpp>

#include "font_assets.hpp"

#include <imgui.h>

#include <string>

namespace thrystr::gui {

FontSet load_fonts(ImGuiIO& io, const char* font_directory, float body_px, float mono_px,
                   float hero_px) {
    FontSet fonts;
    const FontPaths paths = font_paths(font_directory);
    if (!font_file_exists(paths.sans) || !font_file_exists(paths.mono)) {
        return fonts;
    }
    fonts.sans = load_body_font(io, paths.sans, paths.icons, body_px, true);
    fonts.sans_md = font_file_exists(paths.medium)
                        ? load_body_font(io, paths.medium, paths.icons, body_px, true)
                        : nullptr;
    fonts.mono = io.Fonts->AddFontFromFileTTF(paths.mono.c_str(), mono_px);
    const std::string& hero = font_file_exists(paths.medium) ? paths.medium : paths.sans;
    fonts.hero = load_body_font(io, hero, paths.icons, hero_px, false);
    fonts.loaded = fonts.sans != nullptr && fonts.mono != nullptr;
    io.FontDefault = fonts.sans;
    return fonts;
}

bool font_set_ready(const FontSet& fonts) noexcept { return fonts.loaded && fonts.sans != nullptr; }

FontSet load_fonts(ImGuiIO& io, const char* font_directory, FontRequest request) {
    return load_fonts(io, font_directory, request.body_px, request.mono_px, request.hero_px);
}

} // namespace thrystr::gui
