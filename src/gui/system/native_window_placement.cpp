// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"

#include <algorithm>
#include <cmath>

namespace thrystr::gui {
namespace {

int margin(int limit, double ratio) { return std::min(limit, static_cast<int>(std::floor(ratio))); }

} // namespace

void center_on_primary_monitor(GLFWwindow* window) noexcept {
    const auto area = primary_work_area();
    if (!window || !area) {
        return;
    }
    const Size size = window_size(erased_window(window));
    const int x = area->position.x + std::max(0, (area->size.width - size.width) / 2);
    const int y = area->position.y + std::max(0, (area->size.height - size.height) / 2);
    glfwSetWindowPos(window, x, y);
}

std::optional<WindowBounds> preferred_workspace_bounds(Size minimum_size) noexcept {
    const auto area = primary_work_area();
    if (!area) {
        return std::nullopt;
    }
    const int left = margin(50, area->size.width * 0.05);
    const int top = margin(50, area->size.height * 0.05);
    const int bottom = margin(150, area->size.height * 0.15);
    WindowBounds bounds{{area->position.x + left, area->position.y + top},
                        {std::max(minimum_size.width, area->size.width - left * 2),
                         std::max(minimum_size.height, area->size.height - top - bottom)}};
    return bounds;
}

} // namespace thrystr::gui
