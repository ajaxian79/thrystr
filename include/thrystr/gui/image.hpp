// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <filesystem>

namespace thrystr::gui {

// A GPU texture managed by the GUI backend.
// The id is zero when no OpenGL texture is owned.
struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;

    bool valid() const noexcept;
    bool empty() const noexcept;
    int area() const noexcept;
    void clear() noexcept;
};

Texture load_texture(const std::filesystem::path& path);
void destroy_texture(unsigned int& texture_id);
bool save_framebuffer_png(const std::filesystem::path& path, int width, int height);

} // namespace thrystr::gui
