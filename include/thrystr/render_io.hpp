// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <filesystem>

namespace thrystr {

struct Texture2D {
    /// OpenGL texture object id; zero means no texture.
    unsigned int id = 0;
    /// Texture width in pixels.
    int width = 0;
    /// Texture height in pixels.
    int height = 0;
};

/// Load an image file into an RGBA OpenGL texture.
Texture2D load_rgba_texture(const std::filesystem::path& path);

/// Delete an OpenGL texture id and reset it to zero.
void destroy_texture(unsigned int& texture_id);

/// Capture the current framebuffer to a PNG.
bool save_screenshot_png(const std::filesystem::path& path, int width, int height);

} // namespace thrystr
