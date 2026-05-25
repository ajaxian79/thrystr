// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "file_dialog_parts.hpp"

#include <thrystr/gui/icons.hpp>

#include <cstdio>

#include <imgui.h>

namespace thrystr::gui {
namespace {

void copy_filename(FileDialogState& state, std::string_view name) {
    std::snprintf(state.filename, sizeof state.filename, "%.*s", static_cast<int>(name.size()),
                  name.data());
}

} // namespace

void draw_dialog_entry(FileDialogState& state, const FileDialogEntry& entry, int index) {
    const bool selected = index == state.row_sel;
    char label[320];
    std::snprintf(label, sizeof label, "%s  %.*s", entry.is_folder ? icons::kFolder : icons::kFile,
                  static_cast<int>(entry.name.size()), entry.name.data());
    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns)) {
        state.row_sel = index;
        if (!entry.is_folder) {
            copy_filename(state, entry.name);
        }
    }
}

} // namespace thrystr::gui
