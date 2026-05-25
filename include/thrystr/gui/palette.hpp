// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/palette_accent.hpp>
#include <thrystr/gui/palette_geometry.hpp>
#include <thrystr/gui/palette_helpers.hpp>
#include <thrystr/gui/palette_surface.hpp>

namespace thrystr::gui {

// Palette is the public Thrystr style vocabulary.
// It intentionally replaces the old vendor-branded
// token namespace in app-facing code. The values are
// kept stable in this slice so screenshots and user
// workflows remain comparable while the implementation
// underneath is integrated a subsystem at a time.
namespace theme_palette = palette;

inline ImVec4 color_vec(ImU32 color) { return palette::to_vec4(color); }

inline ImU32 color_with_alpha(ImU32 color, float alpha) {
    return palette::with_alpha(color, alpha);
}

} // namespace thrystr::gui
