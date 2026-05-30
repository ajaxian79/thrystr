// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "image_pipeline.hpp"
#include "image_png_io.hpp"

#include <png.h>

namespace thrystr::gui {

bool read_png(const std::filesystem::path& path, int& width, int& height,
              std::vector<unsigned char>& pixels) {
    const auto file = open_png_read(path);
    if (!file) {
        return false;
    }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return false;
    }
    png_init_io(png, file.get());
    png_read_info(png, info);
    normalize_png_input(png, info);
    png_read_update_info(png, info);
    width = static_cast<int>(png_get_image_width(png, info));
    height = static_cast<int>(png_get_image_height(png, info));
    pixels.resize(rgba_byte_count(width, height));
    std::vector<png_bytep> rows = mutable_rows(pixels, width, height);
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

} // namespace thrystr::gui
