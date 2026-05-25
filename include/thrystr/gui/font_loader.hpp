// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/interface_types.hpp>

struct ImGuiIO;

namespace thrystr::gui {

// Loads the Thrystr UI font stack into the active interface atlas.
// The implementation currently preserves the old font metrics so
// screenshot parity survives while the font code is brought in-house.
FontSet load_fonts(ImGuiIO& io, const char* font_directory, float body_px = 14.0f,
                   float mono_px = 13.0f, float hero_px = 28.0f);

bool font_set_ready(const FontSet& fonts) noexcept;

struct FontRequest {
    float body_px = 14.0f;
    float mono_px = 13.0f;
    float hero_px = 28.0f;
};

FontSet load_fonts(ImGuiIO& io, const char* font_directory, FontRequest request);

} // namespace thrystr::gui
