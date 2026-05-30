// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/splash.hpp>

namespace thrystr::gui {

struct SplashColumns {
    float recent_x = 0.0f;
    float action_x = 0.0f;
    float width = 0.0f;
    float row_y = 0.0f;
};

void draw_hero_image(ImDrawList* draw, ImTextureID texture, ImVec2 image, ImVec2 size);
void draw_wordmark(ImDrawList* draw, std::string_view wordmark, std::string_view tagline,
                   ImFont* font, ImVec2 size, ImU32 accent);
bool draw_splash_row_button(const char* id, ImVec2 position, ImVec2 size, ImU32 accent);
void draw_splash_headings(ImDrawList* draw, const SplashColumns& columns, float band_y,
                          ImU32 accent);
SplashChoice draw_recent_rows(const SplashColumns& columns, std::span<const SplashRecent> recents,
                              ImU32 accent);
SplashChoice draw_action_rows(const SplashColumns& columns, std::span<const SplashAction> actions,
                              ImU32 accent);

} // namespace thrystr::gui
