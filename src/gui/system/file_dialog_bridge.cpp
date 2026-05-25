// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/file_dialog.hpp>

#include "file_dialog_bridge.hpp"

namespace thrystr::gui {

skald::FileDialogMode vendor_mode(FileDialogMode mode) {
    return mode == FileDialogMode::Save ? skald::FileDialogMode::Save : skald::FileDialogMode::Open;
}

FileDialogResult result_from(skald::FileDialogResult result) {
    if (result == skald::FileDialogResult::Confirmed) {
        return FileDialogResult::Confirmed;
    }
    return result == skald::FileDialogResult::Cancelled ? FileDialogResult::Cancelled
                                                        : FileDialogResult::None;
}

void open_file_dialog(const char* popup_id) { skald::OpenFileDialog(popup_id); }

FileDialogResult begin_file_dialog(const char* popup_id, FileDialogMode mode,
                                   FileDialogState& state,
                                   std::span<const FileDialogEntry> entries) {
    skald::FileDialogState vendor_state = vendor_state_from(state);
    const std::vector<skald::FileDialogEntry> vendor_entries = vendor_entries_from(entries);
    const auto result =
        skald::BeginFileDialog(popup_id, vendor_mode(mode), vendor_state, vendor_entries);
    copy_vendor_state(vendor_state, state);
    return result_from(result);
}

} // namespace thrystr::gui
