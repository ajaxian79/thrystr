// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/image.hpp>

#include "image_pipeline.hpp"

#include <stb_image_write.h>

namespace thrystr::gui {

// PNG output is isolated here so stb never leaks into the public GUI API.
bool write_png(const std::filesystem::path& path, int width, int height,
               const std::vector<unsigned char>& pixels) {
    return stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}

bool save_framebuffer_png(const std::filesystem::path& path, int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    const std::vector<unsigned char> pixels = read_framebuffer_rgba(width, height);
    const std::vector<unsigned char> flipped = flip_rows(pixels, width, height);
    return write_png(path, width, height, flipped);
}

} // namespace thrystr::gui
