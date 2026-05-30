// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/resize_cursor_set.hpp>

#include <GLFW/glfw3.h>

namespace thrystr::gui {
namespace {

int diagonal_down_cursor_shape() {
#if defined(GLFW_RESIZE_NWSE_CURSOR)
    return GLFW_RESIZE_NWSE_CURSOR;
#else
    return GLFW_ARROW_CURSOR;
#endif
}

int diagonal_up_cursor_shape() {
#if defined(GLFW_RESIZE_NESW_CURSOR)
    return GLFW_RESIZE_NESW_CURSOR;
#else
    return GLFW_ARROW_CURSOR;
#endif
}

} // namespace

ResizeCursorSet::ResizeCursorSet() {
    horizontal_ = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    vertical_ = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    diagonal_down_ = glfwCreateStandardCursor(diagonal_down_cursor_shape());
    diagonal_up_ = glfwCreateStandardCursor(diagonal_up_cursor_shape());
}

ResizeCursorSet::~ResizeCursorSet() {
    glfwDestroyCursor(static_cast<GLFWcursor*>(horizontal_));
    glfwDestroyCursor(static_cast<GLFWcursor*>(vertical_));
    glfwDestroyCursor(static_cast<GLFWcursor*>(diagonal_down_));
    glfwDestroyCursor(static_cast<GLFWcursor*>(diagonal_up_));
}

void* ResizeCursorSet::cursor(ResizeCursor cursor) const noexcept {
    switch (cursor) {
    case ResizeCursor::Horizontal:
        return horizontal_;
    case ResizeCursor::Vertical:
        return vertical_;
    case ResizeCursor::DiagonalDown:
        return diagonal_down_;
    case ResizeCursor::DiagonalUp:
        return diagonal_up_;
    }
    return nullptr;
}

} // namespace thrystr::gui
