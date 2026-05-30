// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "control_style.hpp"

namespace thrystr::gui {

std::string control_id(std::string_view text) { return std::string(text); }

ImVec4 control_color(ImU32 value) { return palette::to_vec4(value); }

void push_flat_button(ImU32 hover, ImU32 active) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, control_color(hover));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, control_color(active));
}

void pop_flat_button(int style_vars) {
    ImGui::PopStyleVar(style_vars);
    ImGui::PopStyleColor(3);
}

} // namespace thrystr::gui
