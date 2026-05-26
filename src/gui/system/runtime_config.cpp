// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/runtime_config.hpp>

#include <imgui.h>

namespace thrystr::gui {

RuntimeConfig optimized_runtime_config() noexcept { return {}; }

void apply_runtime_config(ImGuiIO& io, RuntimeConfig config) {
    if (!config.persist_ini) {
        io.IniFilename = nullptr;
    }
    io.LogFilename = nullptr;
    io.ConfigInputTextCursorBlink = config.blink_text_cursor;
    io.ConfigDebugHighlightIdConflicts = config.debug_id_conflicts;
    ImGui::GetStyle().CircleTessellationMaxError = config.circle_error_px;
}

} // namespace thrystr::gui
