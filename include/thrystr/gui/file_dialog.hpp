// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <span>
#include <string_view>

namespace thrystr::gui {

enum class FileDialogMode {
    Open,
    Save,
};

enum class FileDialogResult {
    None,
    Confirmed,
    Cancelled,
};

struct FileDialogEntry {
    std::string_view name;
    std::string_view size;
    std::string_view modified;
    bool is_folder = false;
};

struct FileDialogState {
    char cwd[512] = "/home/user";
    char filter[128] = "";
    char filename[256] = "";
    int row_sel = -1;
};

void open_file_dialog(const char* popup_id);
FileDialogResult begin_file_dialog(const char* popup_id, FileDialogMode mode,
                                   FileDialogState& state,
                                   std::span<const FileDialogEntry> entries);

} // namespace thrystr::gui
