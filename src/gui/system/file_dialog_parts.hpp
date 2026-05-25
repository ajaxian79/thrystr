// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/file_dialog.hpp>

namespace thrystr::gui {

const char* dialog_title(FileDialogMode mode) noexcept;
const char* dialog_action(FileDialogMode mode) noexcept;

void draw_dialog_fields(FileDialogState& state);
void draw_dialog_entry(FileDialogState& state, const FileDialogEntry& entry, int index);
void draw_dialog_listing(FileDialogState& state, std::span<const FileDialogEntry> entries);
FileDialogResult draw_dialog_actions(FileDialogMode mode, FileDialogState& state);

} // namespace thrystr::gui
