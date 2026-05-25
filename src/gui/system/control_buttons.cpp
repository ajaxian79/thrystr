// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/controls.hpp>

#include "control_style.hpp"

namespace thrystr::gui {

bool accent_button(std::string_view label, ImVec2 size) {
    const auto id = control_id(label);
    ImGui::PushStyleColor(ImGuiCol_Button, control_color(palette::surface::control));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, control_color(palette::accent::chrome));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, control_color(palette::accent::chrome));
    ImGui::PushStyleColor(ImGuiCol_Text, control_color(palette::accent::chrome));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, palette::pad::loose);
    const bool clicked = ImGui::Button(id.c_str(), size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

bool ghost_button(std::string_view label, ImVec2 size) {
    const auto id = control_id(label);
    push_flat_button(palette::surface::control, palette::surface::control_hi);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool clicked = ImGui::Button(id.c_str(), size);
    pop_flat_button(1);
    return clicked;
}

bool icon_button(std::string_view text, std::string_view tip, float size_px) {
    const auto id = control_id(text);
    push_flat_button(palette::surface::control, palette::surface::control_hi);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    const bool clicked = ImGui::Button(id.c_str(), ImVec2(size_px, size_px));
    pop_flat_button(1);
    if (!tip.empty() && ImGui::IsItemHovered()) {
        tooltip(tip);
    }
    return clicked;
}

} // namespace thrystr::gui
