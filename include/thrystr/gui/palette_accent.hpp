// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

namespace thrystr::gui::palette {

namespace accent {
constexpr ImU32 chrome = IM_COL32(0x33, 0x5e, 0x9a, 0xff);
constexpr ImU32 violet = IM_COL32(0x55, 0x40, 0x82, 0xff);
constexpr ImU32 ember = IM_COL32(0xa3, 0x6c, 0x34, 0xff);
constexpr ImU32 gold = IM_COL32(0xa8, 0x7d, 0x33, 0xff);
constexpr ImU32 mint = IM_COL32(0x32, 0x77, 0x55, 0xff);
constexpr ImU32 coral = IM_COL32(0x96, 0x40, 0x4a, 0xff);
constexpr ImU32 tan = IM_COL32(0x82, 0x68, 0x47, 0xff);
constexpr ImU32 cyan = IM_COL32(0x33, 0x77, 0x82, 0xff);
} // namespace accent

namespace accents = accent;

namespace status {
constexpr ImU32 success = IM_COL32(0x4a, 0xa0, 0x68, 0xff);
constexpr ImU32 warning = IM_COL32(0xd6, 0xa0, 0x3f, 0xff);
constexpr ImU32 destructive = IM_COL32(0xd0, 0x7f, 0x7f, 0xff);
constexpr ImU32 info = IM_COL32(0x4d, 0xa0, 0xa8, 0xff);
} // namespace status

} // namespace thrystr::gui::palette
