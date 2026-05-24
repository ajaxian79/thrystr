#pragma once

// skald::tokens — the single source of truth for every surface colour,
// ink (text) colour, accent colour, radius, padding, and spacing the
// theme uses. Apps consume these directly; they never define their
// own IM_COL32 constants.
//
// v0.1 palette philosophy (long form: design/principles.md):
//   - Surfaces are very dark and close to black. Five stops, each
//     ~5–7 % lighter than the last, never above ~22% luminance.
//   - 3 ink stops (primary / muted / dim) on the high end.
//   - 1 accent (configurable per app or per app-mode) — used only on
//     focusable / actionable / measurable elements. Accent at rest is
//     subdued (≤ 50% saturation); full saturation appears only on
//     hover / pressed / active.
//   - 4 status colours (success / warning / destructive / info),
//     reserved for state.
//   - White-alpha borders only. Never saturated strokes.
//   - Corner radii live in 2–5 px range. The "pill" is gone — it
//     fought the Adobe-grade restraint we want.
//
// v0.1 changed surfaces ~30% darker, accents deepened (less saturated,
// closer to black-with-a-hint-of-colour), pill/card radii capped at 4 px.
// Old tokens are removed — apps that referenced them re-target.

#include <imgui.h>

namespace skald::tokens {

// ── Surfaces ────────────────────────────────────────────────────
// Five stops, dark to light. Pick the lowest stop that gives the
// element enough contrast against its parent.
namespace surface {
constexpr ImU32 deep        = IM_COL32(0x07, 0x08, 0x0c, 0xff);  // popovers, menus
constexpr ImU32 window      = IM_COL32(0x0d, 0x0e, 0x12, 0xff);  // app background
constexpr ImU32 panel       = IM_COL32(0x12, 0x14, 0x19, 0xff);  // panel default
constexpr ImU32 panel_alt   = IM_COL32(0x18, 0x1a, 0x20, 0xff);  // raised panel / row
constexpr ImU32 control     = IM_COL32(0x1f, 0x22, 0x29, 0xff);  // button rest
constexpr ImU32 control_hi  = IM_COL32(0x28, 0x2c, 0x35, 0xff);  // button hover
constexpr ImU32 control_act = IM_COL32(0x32, 0x37, 0x42, 0xff);  // button pressed
constexpr ImU32 input       = IM_COL32(0x09, 0x0a, 0x0e, 0xff);  // text fields
constexpr ImU32 modal       = IM_COL32(0x0a, 0x0d, 0x14, 0xff);  // modal scrim
constexpr ImU32 row_alt     = IM_COL32(0xff, 0xff, 0xff, 0x05);  // table zebra
}  // namespace surface

// ── Borders ─────────────────────────────────────────────────────
// White-alpha only. Saturated borders read as branded chrome and
// fight the accent for attention.
namespace border {
constexpr ImU32 hairline    = IM_COL32(0xff, 0xff, 0xff, 0x0c);
constexpr ImU32 default_    = IM_COL32(0xff, 0xff, 0xff, 0x12);
constexpr ImU32 strong      = IM_COL32(0xff, 0xff, 0xff, 0x22);
constexpr ImU32 separator   = IM_COL32(0xff, 0xff, 0xff, 0x08);
}  // namespace border

// ── Ink (text) ──────────────────────────────────────────────────
namespace ink {
constexpr ImU32 primary     = IM_COL32(0xe6, 0xe8, 0xeb, 0xff);  // ~90% white — backed off from #eeeff2 to lower 'glow'
constexpr ImU32 muted       = IM_COL32(0x82, 0x87, 0x91, 0xff);  // ~55%
constexpr ImU32 dim         = IM_COL32(0x5a, 0x60, 0x6a, 0xff);  // ~32% — disabled
constexpr ImU32 link        = IM_COL32(0x5a, 0x86, 0xc4, 0xff);  // calmer link
}  // namespace ink

// ── Accents ─────────────────────────────────────────────────────
// Deepened from v0 — closer to black-with-a-hint-of-colour. Used
// only on focusable / actionable / measurable elements. Accent at
// rest is subdued; full saturation appears on hover / pressed.
namespace accents {
constexpr ImU32 chrome      = IM_COL32(0x33, 0x5e, 0x9a, 0xff);  // calm steel-blue   (default)
constexpr ImU32 violet      = IM_COL32(0x55, 0x40, 0x82, 0xff);  // composer / authoring
constexpr ImU32 ember       = IM_COL32(0xa3, 0x6c, 0x34, 0xff);  // launchpad / nav
constexpr ImU32 gold        = IM_COL32(0xa8, 0x7d, 0x33, 0xff);  // render / output
constexpr ImU32 mint        = IM_COL32(0x32, 0x77, 0x55, 0xff);  // recorder / capture
constexpr ImU32 coral       = IM_COL32(0x96, 0x40, 0x4a, 0xff);  // export / commit
constexpr ImU32 tan         = IM_COL32(0x82, 0x68, 0x47, 0xff);  // ads / sponsored
constexpr ImU32 cyan        = IM_COL32(0x33, 0x77, 0x82, 0xff);  // info / inspect
}  // namespace accents

// ── Status ──────────────────────────────────────────────────────
// Reserved for state, not style. Slightly desaturated from v0 to
// match the new restraint while keeping AA legibility on bare panel.
namespace status {
constexpr ImU32 success     = IM_COL32(0x4a, 0xa0, 0x68, 0xff);  // ~5.4:1 on panel
constexpr ImU32 warning     = IM_COL32(0xd6, 0xa0, 0x3f, 0xff);
constexpr ImU32 destructive = IM_COL32(0xd0, 0x7f, 0x7f, 0xff);  // ~4.7:1 on panel
constexpr ImU32 info        = IM_COL32(0x4d, 0xa0, 0xa8, 0xff);
}  // namespace status

// ── Geometry ────────────────────────────────────────────────────
// 2–5 px range, full stop. The old 'pill' (10 px) and 'card' (14 px)
// radii fought the Adobe restraint we want. Both are now 4 px.
namespace radii {
constexpr float none   = 0.0f;
constexpr float ctrl   = 4.0f;     // buttons, frames, inputs
constexpr float pill   = 4.0f;     // toggles, badges (no longer pill-shaped)
constexpr float card   = 4.0f;     // panels
constexpr float modal  = 5.0f;     // popups, dropdowns
constexpr float circle = 9999.0f;  // dots, status pucks
}  // namespace radii

// ── Spacing ─────────────────────────────────────────────────────
namespace pad {
constexpr ImVec2 tight    = ImVec2( 6.0f,  4.0f);
constexpr ImVec2 standard = ImVec2(10.0f,  6.0f);
constexpr ImVec2 loose    = ImVec2(14.0f,  8.0f);
constexpr ImVec2 window   = ImVec2(14.0f, 12.0f);
constexpr ImVec2 cell     = ImVec2(10.0f,  8.0f);
constexpr ImVec2 item_sp  = ImVec2( 8.0f,  6.0f);
constexpr ImVec2 inner_sp = ImVec2( 6.0f,  4.0f);
}  // namespace pad

// ── Sizes ───────────────────────────────────────────────────────
namespace sizes {
constexpr float scrollbar = 12.0f;
constexpr float grab_min  = 12.0f;
constexpr float row_h     = 26.0f;
constexpr float section_h = 26.0f;
constexpr float chip_h    = 20.0f;
constexpr float toolbar_h = 36.0f;
constexpr float wsbar_h   = 30.0f;        // workspace tab strip
constexpr float doctab_h  = 30.0f;        // document tab strip
}  // namespace sizes

// ── Typography ──────────────────────────────────────────────────
constexpr float kFontPx     = 14.0f;
constexpr float kFontPxMono = 13.0f;
constexpr float kFontPxIcon = 16.0f;
constexpr float kFontPxHero = 28.0f;       // splash / large titles

// ── Helpers ─────────────────────────────────────────────────────
inline ImVec4 to_vec4(ImU32 col) {
    const float r = ((col >> IM_COL32_R_SHIFT) & 0xff) / 255.f;
    const float g = ((col >> IM_COL32_G_SHIFT) & 0xff) / 255.f;
    const float b = ((col >> IM_COL32_B_SHIFT) & 0xff) / 255.f;
    const float a = ((col >> IM_COL32_A_SHIFT) & 0xff) / 255.f;
    return ImVec4(r, g, b, a);
}

inline ImU32 with_alpha(ImU32 col, float a) {
    const auto r = (col >> IM_COL32_R_SHIFT) & 0xff;
    const auto g = (col >> IM_COL32_G_SHIFT) & 0xff;
    const auto b = (col >> IM_COL32_B_SHIFT) & 0xff;
    const auto ai = static_cast<unsigned>(a * 255.f) & 0xff;
    return IM_COL32(r, g, b, ai);
}

}  // namespace skald::tokens
