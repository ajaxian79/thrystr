// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"

namespace thrystr::gui {

void move_window(WindowHandle window, Point position) noexcept {
    if (move_window_natively(native_window(window), position)) {
        return;
    }
    glfwSetWindowPos(native_window(window), position.x, position.y);
}

void resize_window(WindowHandle window, Size size) noexcept {
    glfwSetWindowSize(native_window(window), size.width, size.height);
}

void move_resize_window(WindowHandle window, WindowBounds bounds) noexcept {
    if (move_resize_window_natively(native_window(window), bounds)) {
        return;
    }
    move_window(window, bounds.position);
    resize_window(window, bounds.size);
}

void minimize_window(WindowHandle window) noexcept { glfwIconifyWindow(native_window(window)); }

void toggle_maximized_window(WindowHandle window) noexcept {
    if (is_maximized(window)) {
        glfwRestoreWindow(native_window(window));
        return;
    }
    glfwMaximizeWindow(native_window(window));
}

void set_cursor(WindowHandle window, void* cursor) noexcept {
    glfwSetCursor(native_window(window), static_cast<GLFWcursor*>(cursor));
}

} // namespace thrystr::gui
