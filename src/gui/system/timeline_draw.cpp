// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/timeline_draw.hpp>

#include <algorithm>
#include <limits>

namespace thrystr::gui {
namespace {

int reserve_count(int current, std::size_t requested) {
    constexpr auto max = static_cast<std::size_t>(std::numeric_limits<int>::max());
    const std::size_t bounded = std::min(max, static_cast<std::size_t>(current) + requested);
    return static_cast<int>(bounded);
}

} // namespace

void reserve_timeline_geometry(ImDrawList* draw, std::size_t points, std::size_t labels) {
    if (!draw) {
        return;
    }
    draw->VtxBuffer.reserve(reserve_count(draw->VtxBuffer.Size, points * 4u + labels * 6u));
    draw->IdxBuffer.reserve(reserve_count(draw->IdxBuffer.Size, points * 6u + labels * 6u));
}

void stroke_polyline(ImDrawList* draw, std::span<const ImVec2> points, ImU32 color,
                     float thickness) {
    if (!draw || points.size() < 2u) {
        return;
    }
    constexpr auto chunk = static_cast<std::size_t>(std::numeric_limits<int>::max());
    for (std::size_t offset = 0u; offset < points.size(); offset += chunk - 1u) {
        const std::size_t count = std::min(chunk, points.size() - offset);
        draw->AddPolyline(points.data() + offset, static_cast<int>(count), color, ImDrawFlags_None,
                          thickness);
    }
}

} // namespace thrystr::gui
