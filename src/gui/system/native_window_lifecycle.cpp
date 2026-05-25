// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"

namespace thrystr::gui {

void destroy_window(WindowHandle& window) noexcept {
    if (!window) {
        return;
    }
    glfwDestroyWindow(native_window(window));
    window = nullptr;
}

void show_window(WindowHandle window) noexcept { glfwShowWindow(native_window(window)); }

void poll_events() noexcept { glfwPollEvents(); }

bool should_close(WindowHandle window) noexcept {
    return glfwWindowShouldClose(native_window(window)) == GLFW_TRUE;
}

void request_close(WindowHandle window) noexcept {
    glfwSetWindowShouldClose(native_window(window), GLFW_TRUE);
}

} // namespace thrystr::gui
