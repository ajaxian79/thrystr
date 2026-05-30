// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/timeline_playback.hpp>

#include <thrystr/gui/controls.hpp>
#include <thrystr/gui/palette.hpp>

#include <imgui.h>

#include <cmath>
#include <cstdio>

namespace thrystr::gui {

namespace {

bool speed_selected(const TimelinePlayback& playback, double preset) {
    return !playback.custom_speed && std::abs(playback.points_per_second - preset) < 0.001;
}

void draw_preset_button(TimelinePlayback& playback, double preset) {
    char label[16] = {};
    std::snprintf(label, sizeof(label), "%.0f", preset);
    if (selected_button(label, speed_selected(playback, preset), ImVec2(42.0f, 0.0f))) {
        select_preset_speed(playback, preset);
    }
}

} // namespace

void draw_playback_speed_controls(TimelinePlayback& playback) {
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "speed");
    for (double preset : kPlaybackSpeedPresets) {
        ImGui::SameLine();
        draw_preset_button(playback, preset);
    }

    ImGui::SameLine();
    if (selected_button("custom", playback.custom_speed, ImVec2(74.0f, 0.0f),
                        "Use a typed playback rate")) {
        begin_custom_speed(playback);
    }

    if (!playback.custom_speed) {
        return;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(78.0f);
    if (ImGui::InputText("##custom_playback_pps", playback.custom_speed_text.data(),
                         playback.custom_speed_text.size())) {
        apply_custom_speed(playback);
        playback.fraction = 0.0;
    }
}

} // namespace thrystr::gui
