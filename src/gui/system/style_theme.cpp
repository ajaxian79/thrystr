// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/style.hpp>

#include <imgui.h>
#include <skald/theme.h>

namespace thrystr::gui {
namespace {

void apply_theme(ImGuiStyle& style, ImU32 accent) {
    skald::ApplyDarkTheme(style);
    skald::ApplyAccent(style, accent);
}

} // namespace

void apply_dark_theme(ImGuiStyle& style) { skald::ApplyDarkTheme(style); }

void apply_accent(ImGuiStyle& style, ImU32 accent) { skald::ApplyAccent(style, accent); }

void apply_defaults(ImU32 accent) { skald::ApplyDefaults(accent); }

StyleGuard::StyleGuard(ImGuiStyle& style) : style_(style) {
    apply_theme(style_, palette::accent::chrome);
}

StyleGuard::~StyleGuard() { apply_theme(style_, palette::accent::chrome); }

} // namespace thrystr::gui
