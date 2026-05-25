// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/palette.hpp>

struct ImGuiStyle;

namespace thrystr::gui {

// These calls own the application's current ImGui style contract.
// They intentionally hide the older design-library names while the
// implementation is integrated into Thrystr-owned style modules.
void apply_dark_theme(ImGuiStyle& style);
void apply_accent(ImGuiStyle& style, ImU32 accent);
void apply_defaults(ImU32 accent = palette::accent::chrome);

struct StyleGuard {
    explicit StyleGuard(ImGuiStyle& style);
    ~StyleGuard();

    StyleGuard(const StyleGuard&) = delete;
    StyleGuard& operator=(const StyleGuard&) = delete;

  private:
    ImGuiStyle& style_;
};

} // namespace thrystr::gui
