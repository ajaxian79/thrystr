// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "style_parts.hpp"

namespace thrystr::gui {

void set_theme_color(ImGuiStyle& style, ImGuiCol slot, ImU32 color) {
    style.Colors[slot] = palette::to_vec4(color);
}

void apply_style_surfaces(ImGuiStyle& style) {
    set_theme_color(style, ImGuiCol_WindowBg, palette::surface::window);
    set_theme_color(style, ImGuiCol_ChildBg, palette::surface::window);
    set_theme_color(style, ImGuiCol_PopupBg, palette::surface::deep);
    set_theme_color(style, ImGuiCol_MenuBarBg, palette::surface::panel);
    set_theme_color(style, ImGuiCol_Border, palette::border::default_);
    set_theme_color(style, ImGuiCol_Separator, palette::border::separator);
    set_theme_color(style, ImGuiCol_Text, palette::ink::primary);
    set_theme_color(style, ImGuiCol_TextDisabled, palette::ink::dim);
}

} // namespace thrystr::gui
