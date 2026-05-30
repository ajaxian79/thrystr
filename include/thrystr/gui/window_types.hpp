// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <string_view>

namespace thrystr::gui {

struct Point {
    int x = 0;
    int y = 0;
};

struct Size {
    int width = 0;
    int height = 0;
};

struct CursorPoint {
    double x = 0.0;
    double y = 0.0;
};

struct WindowBounds {
    Point position;
    Size size;
};

struct WindowRequest {
    std::string_view title;
    Size size;
    Size minimum_size;
    bool resizable = true;
    bool visible = false;
};

} // namespace thrystr::gui
