// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/runtime_config.hpp>
#include <thrystr/gui/timeline_draw.hpp>
#include <thrystr/gui/timeline_playback.hpp>

#include <imgui.h>

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

void test_visible_range() {
    const thrystr::gui::TimelineViewport view{1000u, 250.0f, 500.0f, 50.0f, 2.0f};
    const thrystr::gui::SampleRange range = thrystr::gui::visible_sample_range(view);
    assert(range.valid);
    assert(range.first == 100u);
    assert(range.last == 352u);
    assert(range.count() == 253u);
}

void test_stride() {
    assert(thrystr::gui::pixel_stride(0.25f, 58u) == 58u);
    assert(thrystr::gui::pixel_stride(4.0f, 58u) == 15u);
    assert(thrystr::gui::pixel_stride(200.0f, 58u) == 1u);
}

void test_runtime_config() {
    ImGui::CreateContext();
    thrystr::gui::apply_runtime_config(ImGui::GetIO());
    assert(ImGui::GetIO().IniFilename == nullptr);
    assert(ImGui::GetIO().LogFilename == nullptr);
    assert(!ImGui::GetIO().ConfigInputTextCursorBlink);
    ImGui::DestroyContext();
}

void test_playback_navigation() {
    thrystr::gui::TimelinePlayback playback;
    thrystr::gui::set_playhead(playback, 100u, 10u);
    assert(playback.index == 10u);
    thrystr::gui::move_playhead(playback, 100u, -12);
    assert(playback.index == 0u);
    thrystr::gui::move_playhead(playback, 100u, 150);
    assert(playback.index == 99u);
}

void test_playback_advance() {
    thrystr::gui::TimelinePlayback playback;
    playback.points_per_second = 60.0;
    thrystr::gui::toggle_playback(playback, 8u);
    thrystr::gui::advance_playback(playback, 8u, 0.05);
    assert(playback.index == 3u);
    thrystr::gui::advance_playback(playback, 8u, 1.0);
    assert(playback.index == 7u);
    assert(!playback.playing);
}

void test_custom_speed_parse() {
    thrystr::gui::TimelinePlayback playback;
    std::snprintf(playback.custom_speed_text.data(), playback.custom_speed_text.size(), "24.5");
    thrystr::gui::apply_custom_speed(playback);
    assert(std::abs(playback.points_per_second - 24.5) < 0.001);
    std::snprintf(playback.custom_speed_text.data(), playback.custom_speed_text.size(), "bad");
    thrystr::gui::apply_custom_speed(playback);
    assert(std::abs(playback.points_per_second - 24.5) < 0.001);
}

} // namespace

int main() {
    test_visible_range();
    test_stride();
    test_runtime_config();
    test_playback_navigation();
    test_playback_advance();
    test_custom_speed_parse();
    return 0;
}
