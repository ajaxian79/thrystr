// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "image_png_io.hpp"

namespace thrystr::gui {

std::vector<png_bytep> const_rows(const std::vector<unsigned char>& pixels, int width, int height) {
    std::vector<png_bytep> rows(static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        rows[static_cast<std::size_t>(y)] =
            const_cast<png_bytep>(pixels.data() + rgba_stride(width) * y);
    }
    return rows;
}

std::vector<png_bytep> mutable_rows(std::vector<unsigned char>& pixels, int width, int height) {
    std::vector<png_bytep> rows(static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        rows[static_cast<std::size_t>(y)] = pixels.data() + rgba_stride(width) * y;
    }
    return rows;
}

} // namespace thrystr::gui
