// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/image.hpp>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace thrystr::gui {

// Private image pipeline helpers keep stb and OpenGL details out of app code.
// Each step is deliberately narrow so screenshot failures are easy to isolate.
constexpr int kRgbaChannels = 4;

std::size_t rgba_stride(int width);
std::size_t rgba_byte_count(int width, int height);
Texture upload_texture(int width, int height, const unsigned char* pixels);
std::vector<unsigned char> read_framebuffer_rgba(int width, int height);
std::vector<unsigned char> flip_rows(const std::vector<unsigned char>& pixels, int width,
                                     int height);
bool write_png(const std::filesystem::path& path, int width, int height,
               const std::vector<unsigned char>& pixels);

} // namespace thrystr::gui
