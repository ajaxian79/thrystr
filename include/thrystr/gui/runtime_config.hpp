// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

struct ImGuiIO;

namespace thrystr::gui {

struct RuntimeConfig {
    bool persist_ini = false;
    bool blink_text_cursor = false;
    bool debug_id_conflicts = false;
    float circle_error_px = 0.9f;
};

RuntimeConfig optimized_runtime_config() noexcept;
void apply_runtime_config(ImGuiIO& io, RuntimeConfig config = optimized_runtime_config());

} // namespace thrystr::gui
