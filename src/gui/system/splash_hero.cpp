// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "splash_parts.hpp"

#include <algorithm>

namespace thrystr::gui {

void draw_hero_image(ImDrawList* draw, ImTextureID texture, ImVec2 image, ImVec2 size) {
    if (!texture || image.x <= 0.0f || image.y <= 0.0f) {
        return;
    }
    const float scale = std::max(size.x / image.x, (size.y * 0.62f) / image.y);
    const ImVec2 drawn(image.x * scale, image.y * scale);
    const float x = (size.x - drawn.x) * 0.5f;
    draw->AddImage(texture, ImVec2(x, 0), ImVec2(x + drawn.x, drawn.y));
}

} // namespace thrystr::gui
