// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "splash_parts.hpp"

#include <thrystr/gui/palette.hpp>

namespace thrystr::gui {
namespace {

void draw_heading(ImDrawList* draw, ImVec2 position, const char* label, ImU32 accent) {
    draw->AddLine(ImVec2(position.x, position.y - 5.0f),
                  ImVec2(position.x + 24.0f, position.y - 5.0f), accent, 2.0f);
    draw->AddText(position, palette::ink::muted, label);
}

} // namespace

bool draw_splash_row_button(const char* id, ImVec2 position, ImVec2 size, ImU32 accent) {
    ImGui::SetCursorScreenPos(position);
    ImGui::InvisibleButton(id, size);
    if (ImGui::IsItemHovered()) {
        ImGui::GetWindowDrawList()->AddRectFilled(
            position, ImVec2(position.x + size.x, position.y + size.y),
            palette::with_alpha(accent, 0.12f), palette::radii::ctrl);
    }
    return ImGui::IsItemClicked();
}

void draw_splash_headings(ImDrawList* draw, const SplashColumns& columns, float band_y,
                          ImU32 accent) {
    draw_heading(draw, ImVec2(columns.recent_x, band_y + 28.0f), "RECENT", accent);
    draw_heading(draw, ImVec2(columns.action_x, band_y + 28.0f), "START", accent);
}

} // namespace thrystr::gui
