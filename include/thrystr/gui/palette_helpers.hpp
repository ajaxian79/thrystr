// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

namespace thrystr::gui::palette {

inline ImVec4 to_vec4(ImU32 color) {
    const float red = ((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f;
    const float green = ((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f;
    const float blue = ((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f;
    const float alpha = ((color >> IM_COL32_A_SHIFT) & 0xff) / 255.0f;
    return ImVec4(red, green, blue, alpha);
}

inline ImU32 with_alpha(ImU32 color, float alpha) {
    const auto red = (color >> IM_COL32_R_SHIFT) & 0xff;
    const auto green = (color >> IM_COL32_G_SHIFT) & 0xff;
    const auto blue = (color >> IM_COL32_B_SHIFT) & 0xff;
    const auto adjusted = static_cast<unsigned>(alpha * 255.0f) & 0xff;
    return IM_COL32(red, green, blue, adjusted);
}

inline ImU32 invisible() { return IM_COL32(0, 0, 0, 0); }

} // namespace thrystr::gui::palette
