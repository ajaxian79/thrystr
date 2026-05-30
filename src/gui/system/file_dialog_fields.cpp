// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "file_dialog_parts.hpp"

#include <thrystr/gui/palette.hpp>

#include <imgui.h>

namespace thrystr::gui {

const char* dialog_title(FileDialogMode mode) noexcept {
    return mode == FileDialogMode::Open ? "Open file" : "Save file";
}

const char* dialog_action(FileDialogMode mode) noexcept {
    return mode == FileDialogMode::Open ? "Open" : "Save";
}

void draw_dialog_fields(FileDialogState& state) {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, palette::to_vec4(palette::surface::input));
    ImGui::SetNextItemWidth(-220.0f);
    ImGui::InputTextWithHint("##cwd", "path", state.cwd, sizeof state.cwd);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##filter", "filter", state.filter, sizeof state.filter);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 8));
}

} // namespace thrystr::gui
