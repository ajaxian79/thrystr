// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/controls.hpp>
#include <thrystr/gui/palette.hpp>

namespace thrystr::gui {

// Private row utilities for inspector-style label/value output. The
// public API stays small while row positioning and status-tone mapping
// stay shared between key-value rows and state rows.
ImU32 status_tone_color(StatusTone tone);
void draw_row_text(std::string_view label, std::string_view value, ImU32 value_color);

} // namespace thrystr::gui
