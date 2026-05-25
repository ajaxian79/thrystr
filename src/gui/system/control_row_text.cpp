// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "control_rows_internal.hpp"

namespace thrystr::gui {

void draw_row_text(std::string_view label, std::string_view value, ImU32 value_color) {
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 size = ImGui::CalcTextSize(value.data(), value.data() + value.size());
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::SameLine(width - size.x);
    ImGui::PushStyleColor(ImGuiCol_Text, palette::to_vec4(value_color));
    ImGui::TextUnformatted(value.data(), value.data() + value.size());
    ImGui::PopStyleColor();
}

} // namespace thrystr::gui
