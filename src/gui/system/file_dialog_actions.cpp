// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "file_dialog_parts.hpp"

#include <thrystr/gui/controls.hpp>

#include <imgui.h>

namespace thrystr::gui {

FileDialogResult draw_dialog_actions(FileDialogMode mode, FileDialogState& state) {
    ImGui::TextUnformatted("Filename");
    ImGui::SameLine(120.0f);
    ImGui::SetNextItemWidth(-170.0f);
    ImGui::InputText("##fname", state.filename, sizeof state.filename);
    ImGui::SameLine();
    if (ghost_button("Cancel")) {
        ImGui::CloseCurrentPopup();
        return FileDialogResult::Cancelled;
    }
    ImGui::SameLine();
    if (accent_button(dialog_action(mode))) {
        ImGui::CloseCurrentPopup();
        return FileDialogResult::Confirmed;
    }
    return FileDialogResult::None;
}

} // namespace thrystr::gui
