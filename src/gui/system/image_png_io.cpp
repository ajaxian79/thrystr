// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "image_png_io.hpp"

namespace thrystr::gui {
namespace {

PngFile open_png(const std::filesystem::path& path, const char* mode) {
    return PngFile(std::fopen(path.string().c_str(), mode), std::fclose);
}

bool png_has_alpha(png_structp png, png_infop info, png_byte color) {
    return color == PNG_COLOR_TYPE_RGBA || color == PNG_COLOR_TYPE_GRAY_ALPHA ||
           png_get_valid(png, info, PNG_INFO_tRNS);
}

} // namespace

PngFile open_png_read(const std::filesystem::path& path) { return open_png(path, "rb"); }

PngFile open_png_write(const std::filesystem::path& path) { return open_png(path, "wb"); }

void normalize_png_input(png_structp png, png_infop info) {
    const png_byte color = png_get_color_type(png, info);
    const png_byte depth = png_get_bit_depth(png, info);
    if (depth == 16) {
        png_set_strip_16(png);
    }
    if (color == PNG_COLOR_TYPE_GRAY && depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    if (color == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (!png_has_alpha(png, info, color)) {
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    }
}

} // namespace thrystr::gui
