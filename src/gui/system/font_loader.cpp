// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/font_loader.hpp>

#include <skald/fonts.h>

namespace thrystr::gui {
namespace {

FontRequest request_from(float body_px, float mono_px, float hero_px) {
    return {body_px, mono_px, hero_px};
}

} // namespace

FontSet load_fonts(ImGuiIO& io, const char* font_directory, float body_px, float mono_px,
                   float hero_px) {
    const FontRequest request = request_from(body_px, mono_px, hero_px);
    const skald::Fonts loaded =
        skald::LoadFonts(io, font_directory, request.body_px, request.mono_px, request.hero_px);
    return {loaded.sans, loaded.sans_md, loaded.mono, loaded.hero, loaded.ok};
}

bool font_set_ready(const FontSet& fonts) noexcept { return fonts.loaded && fonts.sans != nullptr; }

FontSet load_fonts(ImGuiIO& io, const char* font_directory, FontRequest request) {
    return load_fonts(io, font_directory, request.body_px, request.mono_px, request.hero_px);
}

} // namespace thrystr::gui
