// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/image.hpp>

#include "image_pipeline.hpp"

#include <GLFW/glfw3.h>
#include <stb_image.h>

namespace thrystr::gui {

bool Texture::valid() const noexcept { return id != 0 && !empty(); }

bool Texture::empty() const noexcept { return width <= 0 || height <= 0; }

int Texture::area() const noexcept { return empty() ? 0 : width * height; }

void Texture::clear() noexcept {
    id = 0;
    width = 0;
    height = 0;
}

Texture load_texture(const std::filesystem::path& path) {
    Texture texture;
    int channels = 0;
    unsigned char* pixels =
        stbi_load(path.string().c_str(), &texture.width, &texture.height, &channels, 4);
    if (!pixels) {
        return {};
    }
    texture = upload_texture(texture.width, texture.height, pixels);
    stbi_image_free(pixels);
    return texture;
}

void destroy_texture(unsigned int& texture_id) {
    if (texture_id == 0) {
        return;
    }
    glDeleteTextures(1, &texture_id);
    texture_id = 0;
}

} // namespace thrystr::gui
