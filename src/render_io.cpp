// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/render_io.hpp>

#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <cstring>
#include <vector>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace thrystr {

Texture2D load_rgba_texture(const std::filesystem::path& path) {
    Texture2D texture;
    int channels = 0;
    unsigned char* pixels =
        stbi_load(path.string().c_str(), &texture.width, &texture.height, &channels, 4);
    if (!pixels) {
        texture.width = 0;
        texture.height = 0;
        return texture;
    }

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 texture.width,
                 texture.height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    return texture;
}

void destroy_texture(unsigned int& texture_id) {
    if (texture_id != 0) {
        glDeleteTextures(1, &texture_id);
        texture_id = 0;
    }
}

bool save_screenshot_png(const std::filesystem::path& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 4u);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[static_cast<std::size_t>(height - 1 - y) *
                             static_cast<std::size_t>(width) * 4u],
                    &pixels[static_cast<std::size_t>(y) *
                            static_cast<std::size_t>(width) * 4u],
                    static_cast<std::size_t>(width) * 4u);
    }
    return stbi_write_png(path.string().c_str(),
                          width,
                          height,
                          4,
                          flipped.data(),
                          width * 4) != 0;
}

}  // namespace thrystr
