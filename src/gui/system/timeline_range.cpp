// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/timeline_draw.hpp>

#include <algorithm>
#include <cmath>

namespace thrystr::gui {

std::size_t SampleRange::count() const noexcept { return valid ? last - first + 1u : 0u; }

SampleRange visible_sample_range(const TimelineViewport& viewport) {
    if (viewport.sample_count == 0u || viewport.x_step <= 0.0f || viewport.width <= 0.0f) {
        return {};
    }
    const float left = std::max(0.0f, (viewport.scroll_x - viewport.margin_left) / viewport.x_step);
    const float right =
        std::min(static_cast<float>(viewport.sample_count - 1u),
                 (viewport.scroll_x + viewport.width - viewport.margin_left) / viewport.x_step);
    const auto first = static_cast<std::size_t>(std::floor(left));
    const auto last = static_cast<std::size_t>(std::ceil(std::max(left, right))) + 2u;
    return {true, std::min(viewport.sample_count - 1u, first),
            std::min(viewport.sample_count - 1u, last)};
}

std::size_t pixel_stride(float x_step, std::size_t target_pixels) {
    const double step = std::max(1.0, static_cast<double>(x_step));
    return std::max<std::size_t>(1u, static_cast<std::size_t>(std::ceil(target_pixels / step)));
}

} // namespace thrystr::gui
