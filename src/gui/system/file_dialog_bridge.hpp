// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/file_dialog.hpp>

#include <skald/widgets.h>

#include <span>
#include <vector>

namespace thrystr::gui {

// Private transition helpers while the dialog implementation is moved
// from the previous toolkit surface into Thrystr-owned controls. These
// keep the public header free of vendor names and make the copy boundary
// explicit instead of depending on layout casts.
skald::FileDialogMode vendor_mode(FileDialogMode mode);
FileDialogResult result_from(skald::FileDialogResult result);

skald::FileDialogState vendor_state_from(const FileDialogState& state);
void copy_vendor_state(skald::FileDialogState source, FileDialogState& target);

std::vector<skald::FileDialogEntry> vendor_entries_from(std::span<const FileDialogEntry> entries);

} // namespace thrystr::gui
