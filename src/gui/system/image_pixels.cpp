// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "image_pipeline.hpp"

#include <GLFW/glfw3.h>
#include <cstring>

namespace thrystr::gui {

std::size_t rgba_stride(int width) { return static_cast<std::size_t>(width) * kRgbaChannels; }

std::size_t rgba_byte_count(int width, int height) {
    return rgba_stride(width) * static_cast<std::size_t>(height);
}

std::vector<unsigned char> read_framebuffer_rgba(int width, int height) {
    std::vector<unsigned char> pixels(rgba_byte_count(width, height));
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

std::vector<unsigned char> flip_rows(const std::vector<unsigned char>& pixels, int width,
                                     int height) {
    std::vector<unsigned char> flipped(pixels.size());
    const std::size_t stride = rgba_stride(width);
    for (int y = 0; y < height; ++y) {
        const auto source = static_cast<std::size_t>(y) * stride;
        const auto target = static_cast<std::size_t>(height - 1 - y) * stride;
        std::memcpy(&flipped[target], &pixels[source], stride);
    }
    return flipped;
}

} // namespace thrystr::gui
