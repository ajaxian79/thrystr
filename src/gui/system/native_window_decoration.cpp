// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "native_window_backend.hpp"

#if defined(THRYSTR_HAS_X11) && defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>
#undef None
#undef Success
#endif

namespace thrystr::gui {

void hide_system_decoration(GLFWwindow* window) noexcept {
    if (!window) {
        return;
    }
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
#if defined(THRYSTR_HAS_X11) && defined(__linux__)
    if (glfwGetPlatform() != GLFW_PLATFORM_X11) {
        return;
    }
    Display* display = glfwGetX11Display();
    const Window xwindow = glfwGetX11Window(window);
    if (!display || xwindow == 0) {
        return;
    }
    constexpr unsigned long decorations_flag = 1UL << 1;
    unsigned long hints[5] = {decorations_flag, 0, 0, 0, 0};
    const Atom property = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    XChangeProperty(display, xwindow, property, property, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(hints), 5);
    XFlush(display);
#endif
}

} // namespace thrystr::gui
