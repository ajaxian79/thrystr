// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"
#include "native_window_glfw_platform.hpp"

#if defined(THRYSTR_HAS_X11) && defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#undef None
#undef Success
#endif

namespace thrystr::gui {

CursorPoint global_cursor_position(WindowHandle window, CursorPoint local) noexcept {
#if defined(THRYSTR_HAS_X11) && defined(__linux__)
    if (glfw_platform_is_x11()) {
        Display* display = glfwGetX11Display();
        const Window xwindow = glfwGetX11Window(native_window(window));
        if (display && xwindow != 0) {
            Window root = 0;
            Window child = 0;
            int rx = 0, ry = 0, wx = 0, wy = 0;
            unsigned int mask = 0;
            if (XQueryPointer(display, xwindow, &root, &child, &rx, &ry, &wx, &wy, &mask)) {
                return {static_cast<double>(rx), static_cast<double>(ry)};
            }
        }
    }
#endif
    const Point origin = window_position(window);
    return {static_cast<double>(origin.x) + local.x, static_cast<double>(origin.y) + local.y};
}

} // namespace thrystr::gui
