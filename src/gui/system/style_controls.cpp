// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "style_parts.hpp"

namespace thrystr::gui {

void apply_style_controls(ImGuiStyle& style) {
    set_theme_color(style, ImGuiCol_FrameBg, palette::surface::input);
    set_theme_color(style, ImGuiCol_FrameBgHovered, palette::surface::control_hi);
    set_theme_color(style, ImGuiCol_FrameBgActive, palette::surface::control_act);
    set_theme_color(style, ImGuiCol_Button, palette::surface::control);
    set_theme_color(style, ImGuiCol_ButtonHovered, palette::surface::control_hi);
    set_theme_color(style, ImGuiCol_ButtonActive, palette::surface::control_act);
    set_theme_color(style, ImGuiCol_Header, palette::surface::panel_alt);
    set_theme_color(style, ImGuiCol_HeaderHovered, palette::surface::control_hi);
    set_theme_color(style, ImGuiCol_HeaderActive, palette::surface::control_act);
}

} // namespace thrystr::gui
