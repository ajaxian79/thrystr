// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "splash_parts.hpp"

#include <thrystr/gui/palette.hpp>

#include <cstdio>
#include <string>

namespace thrystr::gui {

SplashChoice draw_action_rows(const SplashColumns& columns, std::span<const SplashAction> actions,
                              ImU32 accent) {
    SplashChoice choice;
    for (int index = 0; index < static_cast<int>(actions.size()); ++index) {
        const float y = columns.row_y + static_cast<float>(index) * 38.0f;
        char id[32];
        std::snprintf(id, sizeof id, "##action_%d", index);
        if (draw_splash_row_button(id, ImVec2(columns.action_x - 4.0f, y - 4.0f),
                                   ImVec2(columns.width, 32.0f), accent)) {
            choice = {SplashChoice::Kind::Action, index};
        }
        ImGui::GetWindowDrawList()->AddText(ImVec2(columns.action_x, y + 6.0f),
                                            palette::ink::primary,
                                            std::string(actions[index].label).c_str());
    }
    return choice;
}

} // namespace thrystr::gui
