#include <skald/fonts.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

namespace skald {

namespace {

bool file_exists(const std::string& p) {
    if (FILE* f = std::fopen(p.c_str(), "rb")) {
        std::fclose(f);
        return true;
    }
    return false;
}

ImFont* load_with_icons(ImGuiIO&            io,
                        const std::string&  body_ttf,
                        const std::string&  icons_ttf,
                        float               px,
                        bool                merge_icons) {
    ImFontConfig body_cfg;
    body_cfg.OversampleH = 3;
    body_cfg.OversampleV = 1;
    body_cfg.PixelSnapH  = false;

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        body_ttf.c_str(), px, &body_cfg);
    if (!font) return nullptr;

    if (merge_icons && file_exists(icons_ttf)) {
        ImFontConfig icon_cfg;
        icon_cfg.MergeMode    = true;
        icon_cfg.PixelSnapH   = true;
        icon_cfg.GlyphOffset  = ImVec2(0.0f, 1.0f);   // optical-centre on body baseline
        icon_cfg.GlyphMinAdvanceX = px;               // square cell
        static const ImWchar icon_ranges[] = {
            static_cast<ImWchar>(skald::icons::kRangeLo),
            static_cast<ImWchar>(skald::icons::kRangeHi),
            0,
        };
        io.Fonts->AddFontFromFileTTF(
            icons_ttf.c_str(), px, &icon_cfg, icon_ranges);
    }
    return font;
}

}  // namespace

Fonts LoadFonts(ImGuiIO&    io,
                const char* font_dir,
                float       body_px,
                float       mono_px,
                float       hero_px) {
    Fonts out;

    const std::string dir = font_dir;
    const std::string sans_reg  = dir + "/Inter-Regular.ttf";
    const std::string sans_med  = dir + "/Inter-Medium.ttf";
    const std::string mono_reg  = dir + "/NotoSansMono-Regular.ttf";
    const std::string icons_ttf = dir + "/SkaldIcons.ttf";

    if (!file_exists(sans_reg) || !file_exists(mono_reg)) {
        return out;  // ok stays false; host falls back to ImGui default
    }

    out.sans    = load_with_icons(io, sans_reg, icons_ttf, body_px, true);
    out.sans_md = file_exists(sans_med)
                ? load_with_icons(io, sans_med, icons_ttf, body_px, true)
                : nullptr;

    ImFontConfig mono_cfg;
    mono_cfg.OversampleH = 3;
    mono_cfg.OversampleV = 1;
    out.mono = io.Fonts->AddFontFromFileTTF(
        mono_reg.c_str(), mono_px, &mono_cfg);

    out.hero = file_exists(sans_med)
        ? load_with_icons(io, sans_med, icons_ttf, hero_px, false)
        : load_with_icons(io, sans_reg, icons_ttf, hero_px, false);

    out.ok = (out.sans != nullptr) && (out.mono != nullptr);
    io.FontDefault = out.sans;
    return out;
}

}  // namespace skald
