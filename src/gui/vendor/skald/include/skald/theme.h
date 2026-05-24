#pragma once

// skald::theme — the public entry points apps call once at host
// bring-up to skin every ImGui surface with the dark/flat/intentional
// palette from skald::tokens. Re-callable; safe after font-atlas
// rebuild or hot-reload.
//
// Typical host bring-up:
//
//     ImGui::CreateContext();
//     skald::ApplyDarkTheme(ImGui::GetStyle());
//     // optional, for multi-mode apps:
//     skald::ApplyAccent(ImGui::GetStyle(), skald::tokens::accents::violet);
//     // …backend init…
//
// If your app changes mode at runtime (Composer ↔ Render ↔ Recorder),
// call ApplyAccent again with the new mode's token. ApplyDarkTheme
// resets every base colour; ApplyAccent only touches the eight
// colours that are accent-driven (CheckMark, SliderGrab, NavHighlight,
// SeparatorHovered/Active, ScrollbarGrabHovered/Active, TextSelectedBg).

#include <imgui.h>

#include <skald/tokens.h>

namespace skald {

// Apply the dark theme (palette + style vars + radii + paddings).
// Safe to call more than once.
void ApplyDarkTheme(ImGuiStyle& s);

// Repaint only the accent-driven colour slots in `s`. Call after
// ApplyDarkTheme; can be called again at runtime to swap accents
// (per-mode apps).
void ApplyAccent(ImGuiStyle& s, ImU32 accent);

// Convenience: apply ApplyDarkTheme to the current global style and
// optionally an accent. Equivalent to:
//     ApplyDarkTheme(ImGui::GetStyle());
//     if (accent) ApplyAccent(ImGui::GetStyle(), *accent);
void ApplyDefaults(ImU32 accent = tokens::accents::chrome);

}  // namespace skald
