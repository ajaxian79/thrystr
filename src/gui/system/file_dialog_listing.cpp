// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "file_dialog_parts.hpp"

#include <imgui.h>

namespace thrystr::gui {

void draw_dialog_listing(FileDialogState& state, std::span<const FileDialogEntry> entries) {
    if (!ImGui::BeginTable("##fdlist", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                           ImVec2(0, 270.0f))) {
        return;
    }
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableHeadersRow();
    for (int index = 0; index < static_cast<int>(entries.size()); ++index) {
        const FileDialogEntry& entry = entries[index];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        draw_dialog_entry(state, entry, index);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(entry.size.data(), entry.size.data() + entry.size.size());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(entry.modified.data(),
                               entry.modified.data() + entry.modified.size());
    }
    ImGui::EndTable();
    ImGui::Dummy(ImVec2(0, 12));
}

} // namespace thrystr::gui
