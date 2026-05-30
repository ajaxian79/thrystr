// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

namespace thrystr::gui {

struct TimelineViewport {
    std::size_t sample_count = 0;
    float scroll_x = 0.0f;
    float width = 0.0f;
    float margin_left = 0.0f;
    float x_step = 1.0f;
};

struct SampleRange {
    bool valid = false;
    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t count() const noexcept;
};

struct TimelinePlotArea {
    float plot_left = 0.0f;
    float plot_bottom = 0.0f;
    float x_step = 1.0f;
    ImVec2 clip_min{};
    ImVec2 clip_max{};
};

struct TimelineTextLabel {
    std::size_t index = 0u;
    std::string text;
};

struct TimelineTicker {
    std::size_t current_index = 0u;
    std::string_view current_text;
    std::span<const TimelineTextLabel> labels;
    ImFont* current_font = nullptr;
    float current_font_scale = 1.5f;
};

SampleRange visible_sample_range(const TimelineViewport& viewport);
std::size_t pixel_stride(float x_step, std::size_t target_pixels);
void reserve_timeline_geometry(ImDrawList* draw, std::size_t points, std::size_t labels);
void stroke_polyline(ImDrawList* draw, std::span<const ImVec2> points, ImU32 color,
                     float thickness);
void draw_timeline_ticker(ImDrawList* draw, const TimelinePlotArea& area,
                          const TimelineTicker& ticker);

} // namespace thrystr::gui
