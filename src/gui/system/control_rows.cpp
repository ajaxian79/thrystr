// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/controls.hpp>

#include "control_rows_internal.hpp"

#include <cstdarg>
#include <cstdio>

namespace thrystr::gui {

void section_header(std::string_view label) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::to_vec4(palette::ink::muted));
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopStyleColor();
    muted_separator(2.0f, 6.0f);
}

void muted_separator(float top_pad, float bottom_pad) {
    ImGui::Dummy(ImVec2(0.0f, top_pad));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, bottom_pad));
}

void key_value_row(std::string_view label, const char* value_format, ...) {
    char value[512];
    va_list args;
    va_start(args, value_format);
    std::vsnprintf(value, sizeof(value), value_format, args);
    va_end(args);
    draw_row_text(label, value, palette::ink::muted);
}

void status_row(std::string_view label, std::string_view value, StatusTone tone) {
    draw_row_text(label, value, status_tone_color(tone));
}

} // namespace thrystr::gui
