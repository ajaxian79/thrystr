// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

namespace thrystr::gui::palette {

namespace radii {
constexpr float none = 0.0f;
constexpr float ctrl = 4.0f;
constexpr float pill = 4.0f;
constexpr float card = 4.0f;
constexpr float modal = 5.0f;
constexpr float circle = 9999.0f;
} // namespace radii

namespace pad {
constexpr ImVec2 tight = ImVec2(6.0f, 4.0f);
constexpr ImVec2 standard = ImVec2(10.0f, 6.0f);
constexpr ImVec2 loose = ImVec2(14.0f, 8.0f);
constexpr ImVec2 window = ImVec2(14.0f, 12.0f);
constexpr ImVec2 cell = ImVec2(10.0f, 8.0f);
constexpr ImVec2 item_sp = ImVec2(8.0f, 6.0f);
constexpr ImVec2 inner_sp = ImVec2(6.0f, 4.0f);
} // namespace pad

namespace sizes {
constexpr float scrollbar = 12.0f;
constexpr float grab_min = 12.0f;
constexpr float row_h = 26.0f;
constexpr float section_h = 26.0f;
constexpr float chip_h = 20.0f;
constexpr float toolbar_h = 36.0f;
} // namespace sizes

constexpr float kFontPx = 14.0f;
constexpr float kFontPxMono = 13.0f;
constexpr float kFontPxHero = 28.0f;

} // namespace thrystr::gui::palette
