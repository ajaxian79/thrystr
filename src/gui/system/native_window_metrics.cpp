// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"

namespace thrystr::gui {

Point window_position(WindowHandle window) noexcept {
    Point position;
    glfwGetWindowPos(native_window(window), &position.x, &position.y);
    return position;
}

Size window_size(WindowHandle window) noexcept {
    Size size;
    glfwGetWindowSize(native_window(window), &size.width, &size.height);
    return size;
}

CursorPoint cursor_position(WindowHandle window) noexcept {
    CursorPoint position;
    glfwGetCursorPos(native_window(window), &position.x, &position.y);
    return position;
}

bool is_iconified(WindowHandle window) noexcept {
    return glfwGetWindowAttrib(native_window(window), GLFW_ICONIFIED) == GLFW_TRUE;
}

bool is_maximized(WindowHandle window) noexcept {
    return glfwGetWindowAttrib(native_window(window), GLFW_MAXIMIZED) == GLFW_TRUE;
}

} // namespace thrystr::gui
