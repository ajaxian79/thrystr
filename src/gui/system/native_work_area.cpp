// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "native_window_backend.hpp"

namespace thrystr::gui {
namespace {

bool has_size(WindowBounds bounds) noexcept {
    return bounds.size.width > 0 && bounds.size.height > 0;
}

} // namespace

std::optional<WindowBounds> primary_work_area() noexcept {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        return std::nullopt;
    }

    WindowBounds bounds;
    glfwGetMonitorWorkarea(monitor, &bounds.position.x, &bounds.position.y, &bounds.size.width,
                           &bounds.size.height);
    if (!has_size(bounds)) {
        return std::nullopt;
    }
    return bounds;
}

} // namespace thrystr::gui
