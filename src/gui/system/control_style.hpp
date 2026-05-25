// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/controls.hpp>
#include <thrystr/gui/palette.hpp>

#include <string>

namespace thrystr::gui {

// Shared control drawing helpers. They keep individual widgets small
// while preserving a single implementation of the palette and style
// stack rules used by buttons, icon buttons, and tooltips.
std::string control_id(std::string_view text);
ImVec4 control_color(ImU32 value);
void push_flat_button(ImU32 hover, ImU32 active);
void pop_flat_button(int style_vars);

} // namespace thrystr::gui
