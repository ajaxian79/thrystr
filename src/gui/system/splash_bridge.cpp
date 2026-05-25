// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/splash.hpp>

#include "splash_parts.hpp"

#include <thrystr/gui/palette.hpp>

namespace thrystr::gui {
namespace {

SplashColumns columns_for(ImVec2 size, float band_y) {
    constexpr float pad = 36.0f;
    constexpr float gap = 32.0f;
    const float width = (size.x - pad * 2.0f - gap * 2.0f) / 3.0f;
    return {pad + width + gap, pad + width * 2.0f + gap * 2.0f, width, band_y + 52.0f};
}

} // namespace

SplashChoice splash(std::string_view wordmark, std::string_view tagline,
                    std::span<const SplashRecent> recents, std::span<const SplashAction> actions,
                    ImTextureID hero_texture, ImVec2 hero_size, ImFont* hero_font) {
    SplashChoice choice;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float band_y = size.y * 0.62f;
    const ImU32 accent = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, size.y), palette::surface::deep);
    draw_hero_image(draw, hero_texture, hero_size, size);
    draw->AddRectFilled(ImVec2(0, band_y - 60.0f), ImVec2(size.x, size.y), palette::surface::deep);
    draw_wordmark(draw, wordmark, tagline, hero_font ? hero_font : ImGui::GetFont(), size, accent);

    const SplashColumns columns = columns_for(size, band_y);
    draw_splash_headings(draw, columns, band_y, accent);
    choice = draw_recent_rows(columns, recents, accent);
    const SplashChoice action = draw_action_rows(columns, actions, accent);
    choice = action.kind == SplashChoice::Kind::None ? choice : action;
    return choice;
}

} // namespace thrystr::gui
