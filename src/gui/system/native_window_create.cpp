// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_window.hpp>

#include "native_window_backend.hpp"

namespace thrystr::gui {

WindowHandle create_window(const WindowRequest& request) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, request.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, request.visible ? GLFW_TRUE : GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(request.size.width, request.size.height,
                                          request.title.data(), nullptr, nullptr);
    if (!window) {
        return nullptr;
    }
    hide_system_decoration(window);
    glfwSetWindowSizeLimits(window,
                            request.resizable ? request.minimum_size.width : request.size.width,
                            request.resizable ? request.minimum_size.height : request.size.height,
                            request.resizable ? GLFW_DONT_CARE : request.size.width,
                            request.resizable ? GLFW_DONT_CARE : request.size.height);
    center_on_primary_monitor(window);
    return erased_window(window);
}

} // namespace thrystr::gui
