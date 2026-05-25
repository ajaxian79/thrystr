// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "style_parts.hpp"

namespace thrystr::gui {

void apply_style_geometry(ImGuiStyle& style) {
    style.WindowPadding = palette::pad::window;
    style.FramePadding = palette::pad::standard;
    style.CellPadding = palette::pad::cell;
    style.ItemSpacing = palette::pad::item_sp;
    style.ItemInnerSpacing = palette::pad::inner_sp;
    style.WindowRounding = palette::radii::none;
    style.FrameRounding = palette::radii::ctrl;
    style.PopupRounding = palette::radii::modal;
    style.ScrollbarSize = palette::sizes::scrollbar;
    style.GrabMinSize = palette::sizes::grab_min;
}

void apply_style_borders(ImGuiStyle& style) {
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
}

} // namespace thrystr::gui
