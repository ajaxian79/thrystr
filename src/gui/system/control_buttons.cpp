// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/controls.hpp>

#include <skald/widgets.h>

namespace thrystr::gui {
namespace {

std::string_view button_text(std::string_view text) { return text; }

ImVec2 requested_size(ImVec2 size) { return size; }

} // namespace

bool accent_button(std::string_view label, ImVec2 size) {
    return skald::AccentButton(button_text(label), requested_size(size));
}

bool ghost_button(std::string_view label, ImVec2 size) {
    return skald::GhostButton(button_text(label), requested_size(size));
}

bool icon_button(std::string_view text, std::string_view tip, float size_px) {
    return skald::IconButton(button_text(text), tip, size_px);
}

void tooltip(std::string_view text) { skald::Tooltip(text); }

bool pill_toggle(std::string_view label, bool* value) { return skald::PillToggle(label, value); }

} // namespace thrystr::gui
