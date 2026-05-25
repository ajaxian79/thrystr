// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "style_parts.hpp"

namespace thrystr::gui {

void apply_accent(ImGuiStyle& style, ImU32 accent) {
    const ImVec4 value = palette::to_vec4(accent);
    const ImVec4 dim(value.x, value.y, value.z, 0.35f);
    style.Colors[ImGuiCol_CheckMark] = value;
    style.Colors[ImGuiCol_SliderGrab] = value;
    style.Colors[ImGuiCol_SliderGrabActive] = value;
    style.Colors[ImGuiCol_TextSelectedBg] = dim;
    style.Colors[ImGuiCol_DragDropTarget] = value;
}

} // namespace thrystr::gui
