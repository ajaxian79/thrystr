// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

struct ImFont;

namespace thrystr::gui {

struct FontSet {
    ImFont* sans = nullptr;
    ImFont* sans_md = nullptr;
    ImFont* mono = nullptr;
    ImFont* hero = nullptr;
    bool loaded = false;

    void clear() noexcept;
    ImFont* body() const noexcept;
    ImFont* heading() const noexcept;
    ImFont* code() const noexcept;
};

struct FrameSize {
    int width = 0;
    int height = 0;

    bool empty() const noexcept;
    float aspect_ratio() const noexcept;
};

using WindowHandle = void*;

} // namespace thrystr::gui
