// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "native_window_backend.hpp"

#if defined(THRYSTR_HAS_X11) && defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#undef None
#undef Success
#endif

namespace thrystr::gui {

bool move_resize_window_natively(GLFWwindow* window, WindowBounds bounds) noexcept {
#if defined(THRYSTR_HAS_X11) && defined(__linux__)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display* display = glfwGetX11Display();
        const Window xwindow = glfwGetX11Window(window);
        if (display && xwindow != 0) {
            XMoveResizeWindow(display, xwindow, bounds.position.x, bounds.position.y,
                              static_cast<unsigned>(bounds.size.width),
                              static_cast<unsigned>(bounds.size.height));
            XFlush(display);
            return true;
        }
    }
#endif
    return false;
}

} // namespace thrystr::gui
