// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/image.hpp>

#include "image_pipeline.hpp"
#include "image_png_io.hpp"

#include <png.h>

namespace thrystr::gui {

bool write_png(const std::filesystem::path& path, int width, int height,
               const std::vector<unsigned char>& pixels) {
    const auto file = open_png_write(path);
    if (!file || pixels.size() < rgba_byte_count(width, height)) {
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return false;
    }
    png_init_io(png, file.get());
    png_set_IHDR(png, info, static_cast<png_uint_32>(width), static_cast<png_uint_32>(height), 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    std::vector<png_bytep> rows = const_rows(pixels, width, height);
    png_set_rows(png, info, rows.data());
    png_write_png(png, info, PNG_TRANSFORM_IDENTITY, nullptr);
    png_destroy_write_struct(&png, &info);
    return true;
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
