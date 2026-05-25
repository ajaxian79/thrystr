// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

#include <string_view>

namespace thrystr::gui {

enum class StatusTone {
    Muted,
    Accent,
    Success,
    Warning,
    Destructive,
    Info,
};

void section_header(std::string_view label);
void muted_separator(float top_pad = 4.0f, float bottom_pad = 4.0f);

bool accent_button(std::string_view label, ImVec2 size = ImVec2(0.0f, 0.0f));
bool ghost_button(std::string_view label, ImVec2 size = ImVec2(0.0f, 0.0f));
bool icon_button(std::string_view text, std::string_view tooltip = {}, float size_px = 28.0f);

void tooltip(std::string_view text);
bool pill_toggle(std::string_view label, bool* value);

void key_value_row(std::string_view label, const char* value_format, ...) IM_FMTARGS(2);
void status_row(std::string_view label, std::string_view value, StatusTone tone);

} // namespace thrystr::gui
