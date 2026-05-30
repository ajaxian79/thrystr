// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/style.hpp>

#include "style_parts.hpp"

#include <imgui.h>

namespace thrystr::gui {

void apply_dark_theme(ImGuiStyle& style) {
    apply_style_geometry(style);
    apply_style_surfaces(style);
    apply_style_controls(style);
    apply_style_borders(style);
}

void apply_defaults(ImU32 accent) {
    apply_dark_theme(ImGui::GetStyle());
    apply_accent(ImGui::GetStyle(), accent);
}

StyleGuard::StyleGuard(ImGuiStyle& style) : style_(style) {
    apply_dark_theme(style_);
    apply_accent(style_, palette::accent::chrome);
}

StyleGuard::~StyleGuard() {
    apply_dark_theme(style_);
    apply_accent(style_, palette::accent::chrome);
}

} // namespace thrystr::gui
