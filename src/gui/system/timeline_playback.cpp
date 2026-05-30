// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/timeline_playback.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace thrystr::gui {

void reset_playback(TimelinePlayback& playback) {
    playback.playing = false;
    playback.dragging = false;
    playback.index = 0u;
    playback.fraction = 0.0;
}

void set_playhead(TimelinePlayback& playback, std::size_t sample_count, std::size_t index) {
    if (sample_count == 0u) {
        reset_playback(playback);
        return;
    }
    playback.index = std::min(index, sample_count - 1u);
    playback.fraction = 0.0;
}

void move_playhead(TimelinePlayback& playback, std::size_t sample_count, int delta) {
    if (sample_count == 0u || delta == 0) {
        return;
    }
    if (delta < 0) {
        const auto amount = static_cast<std::size_t>(-delta);
        set_playhead(playback, sample_count,
                     amount > playback.index ? 0u : playback.index - amount);
        return;
    }
    const auto amount = static_cast<std::size_t>(delta);
    const std::size_t max_index = sample_count - 1u;
    const std::size_t clamped = std::min(playback.index, max_index);
    set_playhead(playback, sample_count,
                 amount >= max_index - clamped ? max_index : clamped + amount);
}

void toggle_playback(TimelinePlayback& playback, std::size_t sample_count) {
    if (sample_count == 0u) {
        return;
    }
    playback.playing = !playback.playing;
    playback.dragging = false;
    playback.fraction = 0.0;
}

void advance_playback(TimelinePlayback& playback, std::size_t sample_count, double delta_seconds) {
    const double rate = std::max(0.0, playback.points_per_second);
    if (!playback.playing || sample_count == 0u || rate <= 0.0) {
        return;
    }
    playback.fraction += std::max(0.0, delta_seconds) * rate;
    const auto steps = static_cast<std::size_t>(playback.fraction);
    if (steps == 0u) {
        return;
    }
    playback.fraction -= static_cast<double>(steps);
    playback.index = std::min(sample_count - 1u, playback.index + steps);
    if (playback.index + 1u >= sample_count) {
        playback.playing = false;
        playback.fraction = 0.0;
    }
}

void select_preset_speed(TimelinePlayback& playback, double points_per_second) {
    playback.custom_speed = false;
    playback.points_per_second = std::max(0.0, points_per_second);
    playback.fraction = 0.0;
}

void begin_custom_speed(TimelinePlayback& playback) {
    if (!playback.custom_speed) {
        std::snprintf(playback.custom_speed_text.data(), playback.custom_speed_text.size(), "%.3g",
                      playback.points_per_second);
    }
    playback.custom_speed = true;
    apply_custom_speed(playback);
    playback.fraction = 0.0;
}

void apply_custom_speed(TimelinePlayback& playback) {
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(playback.custom_speed_text.data(), &end);
    while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
        ++end;
    }
    if (end != playback.custom_speed_text.data() && end != nullptr && *end == '\0' &&
        errno != ERANGE && std::isfinite(value) && value > 0.0) {
        playback.points_per_second = value;
    }
}

} // namespace thrystr::gui
