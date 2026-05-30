// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/style.hpp>

#include <imgui.h>

namespace thrystr::gui {

// The style pipeline is split by responsibility so callers still use
// apply_dark_theme while individual color and geometry groups remain
// independently testable. These functions are private to the GUI
// system target and are intentionally not exported through public
// headers.
void set_theme_color(ImGuiStyle& style, ImGuiCol slot, ImU32 color);
void apply_style_geometry(ImGuiStyle& style);
void apply_style_surfaces(ImGuiStyle& style);
void apply_style_controls(ImGuiStyle& style);
void apply_style_borders(ImGuiStyle& style);
void apply_accent(ImGuiStyle& style, ImU32 accent);

} // namespace thrystr::gui
