// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/file_dialog.hpp>

#include "file_dialog_parts.hpp"

#include <thrystr/gui/controls.hpp>
#include <thrystr/gui/palette.hpp>

#include <imgui.h>

namespace thrystr::gui {

void open_file_dialog(const char* popup_id) { ImGui::OpenPopup(popup_id); }

FileDialogResult begin_file_dialog(const char* popup_id, FileDialogMode mode,
                                   FileDialogState& state,
                                   std::span<const FileDialogEntry> entries) {
    FileDialogResult result = FileDialogResult::None;
    ImGui::SetNextWindowSize(ImVec2(680.0f, 480.0f), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, palette::to_vec4(palette::surface::panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    const bool open = ImGui::BeginPopupModal(popup_id, nullptr, ImGuiWindowFlags_NoTitleBar);
    if (open) {
        section_header(dialog_title(mode));
        draw_dialog_fields(state);
        draw_dialog_listing(state, entries);
        result = draw_dialog_actions(mode, state);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return result;
}

} // namespace thrystr::gui
