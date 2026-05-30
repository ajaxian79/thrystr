// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/native_application.hpp>

#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

namespace thrystr::gui {
namespace {

void report_error(int code, const char* message) {
    std::fprintf(stderr, "glfw error %d: %s\n", code, message);
}

void prefer_x11_when_available() {
#if defined(__linux__) && defined(GLFW_PLATFORM_X11)
    if (std::getenv("DISPLAY") != nullptr) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
#endif
}

} // namespace

NativeApplication::NativeApplication() {
    glfwSetErrorCallback(report_error);
    prefer_x11_when_available();
    ready_ = glfwInit() == GLFW_TRUE;
}

NativeApplication::~NativeApplication() {
    if (ready_) {
        glfwTerminate();
    }
}

bool NativeApplication::ready() const noexcept { return ready_; }

bool NativeApplication::initialized() const noexcept { return ready(); }

NativeApplication::operator bool() const noexcept { return ready(); }

} // namespace thrystr::gui
