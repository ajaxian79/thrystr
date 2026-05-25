// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include "image_pipeline.hpp"

#include <png.h>

#include <cstdio>
#include <memory>

namespace thrystr::gui {

using PngFile = std::unique_ptr<FILE, int (*)(FILE*)>;

PngFile open_png_read(const std::filesystem::path& path);
PngFile open_png_write(const std::filesystem::path& path);
void normalize_png_input(png_structp png, png_infop info);
std::vector<png_bytep> const_rows(const std::vector<unsigned char>& pixels, int width, int height);
std::vector<png_bytep> mutable_rows(std::vector<unsigned char>& pixels, int width, int height);

} // namespace thrystr::gui
