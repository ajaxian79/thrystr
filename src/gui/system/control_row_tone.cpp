// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "control_rows_internal.hpp"

namespace thrystr::gui {

ImU32 status_tone_color(StatusTone tone) {
    switch (tone) {
    case StatusTone::Accent:
        return ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
    case StatusTone::Success:
        return palette::status::success;
    case StatusTone::Warning:
        return palette::status::warning;
    case StatusTone::Destructive:
        return palette::status::destructive;
    case StatusTone::Info:
        return palette::status::info;
    case StatusTone::Muted:
        return palette::ink::muted;
    }
    return palette::ink::muted;
}

} // namespace thrystr::gui
