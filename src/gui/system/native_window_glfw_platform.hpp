// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <GLFW/glfw3.h>

namespace thrystr::gui {

inline bool glfw_platform_is_x11() noexcept {
#if defined(GLFW_PLATFORM_X11)
    return glfwGetPlatform() == GLFW_PLATFORM_X11;
#else
    return true;
#endif
}

} // namespace thrystr::gui
