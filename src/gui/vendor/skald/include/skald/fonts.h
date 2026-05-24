#pragma once

// skald::fonts — load the bundled Inter Regular + Noto Sans Mono +
// SkaldIcons fonts into an ImGui atlas. The icon font is merged onto
// the sans body so any text that contains an icon glyph (PUA range
// U+E000–U+E0FF) renders inline without a separate font push.
//
// Usage at host bring-up:
//
//     IMGUI_CHECKVERSION();
//     ImGui::CreateContext();
//     skald::ApplyDefaults();
//     skald::Fonts fonts = skald::LoadFonts(
//         ImGui::GetIO(),
//         "/path/to/skald/fonts");   // dir containing the .ttf files
//
// Then use:
//     ImGui::PushFont(fonts.mono);   // monospace numerics
//     ImGui::Text("%5.2f ms", t);
//     ImGui::PopFont();
//
// Or for inline icons (default font):
//     ImGui::TextUnformatted(skald::icons::kCog " Settings");

#include <imgui.h>

#include <skald/icons.gen.h>

namespace skald {

struct Fonts {
    ImFont* sans      = nullptr;   // body, default
    ImFont* sans_md   = nullptr;   // medium weight (titles, KPI values)
    ImFont* mono      = nullptr;   // numerics
    ImFont* hero      = nullptr;   // splash wordmark + large titles
    bool    ok        = false;
};

// `font_dir` is the directory containing the bundled .ttf files
// (Inter-Regular.ttf, Inter-Medium.ttf, NotoSansMono-Regular.ttf,
// SkaldIcons.ttf). Returns a Fonts struct with `ok=false` if any
// required file is missing — host should fall back to the ImGui
// default font in that case.
Fonts LoadFonts(ImGuiIO&     io,
                const char*  font_dir,
                float        body_px = 14.0f,
                float        mono_px = 13.0f,
                float        hero_px = 28.0f);

}  // namespace skald
