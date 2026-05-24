#pragma once

#include <filesystem>

namespace thrystr {

struct Texture2D {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
};

Texture2D load_rgba_texture(const std::filesystem::path& path);
void destroy_texture(unsigned int& texture_id);
bool save_screenshot_png(const std::filesystem::path& path, int width, int height);

}  // namespace thrystr
