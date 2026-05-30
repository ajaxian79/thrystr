// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/controls.hpp>

#include "control_style.hpp"

namespace thrystr::gui {

void tooltip(std::string_view text) {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, control_color(palette::surface::panel_alt));
    if (ImGui::BeginTooltip()) {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor();
}

bool pill_toggle(std::string_view label, bool* value) {
    const auto id = control_id(label);
    return ImGui::Checkbox(id.c_str(), value);
}

} // namespace thrystr::gui
