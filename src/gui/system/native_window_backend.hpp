// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/interface_types.hpp>
#include <thrystr/gui/window_types.hpp>

#include <GLFW/glfw3.h>

#include <optional>

namespace thrystr::gui {

inline GLFWwindow* native_window(WindowHandle window) noexcept {
    return static_cast<GLFWwindow*>(window);
}

inline WindowHandle erased_window(GLFWwindow* window) noexcept {
    return static_cast<WindowHandle>(window);
}

void hide_system_decoration(GLFWwindow* window) noexcept;
void center_on_primary_monitor(GLFWwindow* window) noexcept;
std::optional<WindowBounds> primary_work_area() noexcept;
bool move_window_natively(GLFWwindow* window, Point position) noexcept;
bool move_resize_window_natively(GLFWwindow* window, WindowBounds bounds) noexcept;

} // namespace thrystr::gui
