// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <array>
#include <cstddef>

namespace thrystr::gui {

inline constexpr double kDefaultPlaybackPointsPerSecond = 60.0;
inline constexpr std::array<double, 4> kPlaybackSpeedPresets = {12.0, 24.0, 30.0, 60.0};

struct TimelinePlayback {
    std::size_t index = 0u;
    double fraction = 0.0;
    double points_per_second = kDefaultPlaybackPointsPerSecond;
    bool custom_speed = false;
    std::array<char, 32> custom_speed_text = {'6', '0', '\0'};
    bool playing = false;
    bool dragging = false;
};

void reset_playback(TimelinePlayback& playback);
void set_playhead(TimelinePlayback& playback, std::size_t sample_count, std::size_t index);
void move_playhead(TimelinePlayback& playback, std::size_t sample_count, int delta);
void toggle_playback(TimelinePlayback& playback, std::size_t sample_count);
void advance_playback(TimelinePlayback& playback, std::size_t sample_count, double delta_seconds);
void select_preset_speed(TimelinePlayback& playback, double points_per_second);
void begin_custom_speed(TimelinePlayback& playback);
void apply_custom_speed(TimelinePlayback& playback);
void draw_playback_speed_controls(TimelinePlayback& playback);

} // namespace thrystr::gui
