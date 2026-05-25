// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "file_dialog_bridge.hpp"

#include <algorithm>
#include <iterator>

namespace thrystr::gui {

skald::FileDialogState vendor_state_from(const FileDialogState& state) {
    skald::FileDialogState vendor_state;
    std::copy(std::begin(state.cwd), std::end(state.cwd), std::begin(vendor_state.cwd));
    std::copy(std::begin(state.filter), std::end(state.filter), std::begin(vendor_state.filter));
    std::copy(std::begin(state.filename), std::end(state.filename),
              std::begin(vendor_state.filename));
    vendor_state.row_sel = state.row_sel;
    return vendor_state;
}

void copy_vendor_state(skald::FileDialogState source, FileDialogState& target) {
    std::copy(std::begin(source.cwd), std::end(source.cwd), std::begin(target.cwd));
    std::copy(std::begin(source.filter), std::end(source.filter), std::begin(target.filter));
    std::copy(std::begin(source.filename), std::end(source.filename), std::begin(target.filename));
    target.row_sel = source.row_sel;
}

std::vector<skald::FileDialogEntry> vendor_entries_from(std::span<const FileDialogEntry> entries) {
    std::vector<skald::FileDialogEntry> out;
    out.reserve(entries.size());
    for (const FileDialogEntry& entry : entries) {
        out.push_back({entry.name, entry.size, entry.modified, entry.is_folder});
    }
    return out;
}

} // namespace thrystr::gui
