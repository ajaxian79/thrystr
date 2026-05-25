// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/interface_types.hpp>
#include <thrystr/gui/window_types.hpp>

#include <optional>

namespace thrystr::gui {

WindowHandle create_window(const WindowRequest& request);
void destroy_window(WindowHandle& window) noexcept;
void show_window(WindowHandle window) noexcept;
void poll_events() noexcept;

bool should_close(WindowHandle window) noexcept;
bool is_iconified(WindowHandle window) noexcept;
bool is_maximized(WindowHandle window) noexcept;
void request_close(WindowHandle window) noexcept;

Point window_position(WindowHandle window) noexcept;
Size window_size(WindowHandle window) noexcept;
CursorPoint cursor_position(WindowHandle window) noexcept;
CursorPoint global_cursor_position(WindowHandle window, CursorPoint local) noexcept;

void move_window(WindowHandle window, Point position) noexcept;
void resize_window(WindowHandle window, Size size) noexcept;
void move_resize_window(WindowHandle window, WindowBounds bounds) noexcept;

void minimize_window(WindowHandle window) noexcept;
void toggle_maximized_window(WindowHandle window) noexcept;

void set_cursor(WindowHandle window, void* cursor) noexcept;
std::optional<WindowBounds> preferred_workspace_bounds(Size minimum_size) noexcept;

} // namespace thrystr::gui
