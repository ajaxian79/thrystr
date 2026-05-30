// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include "splash_parts.hpp"

#include <thrystr/gui/palette.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace thrystr::gui {
namespace {

std::string uppercase(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

} // namespace

void draw_wordmark(ImDrawList* draw, std::string_view wordmark, std::string_view tagline,
                   ImFont* font, ImVec2 size, ImU32 accent) {
    const std::string title = uppercase(wordmark);
    const std::string tag = uppercase(tagline);
    const float font_size = std::max(48.0f, std::min(72.0f, size.y * 0.11f));
    const ImVec2 center(size.x * 0.5f, size.y * 0.42f);
    const ImVec2 title_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, title.c_str());
    draw->AddText(font, font_size, ImVec2(center.x - title_size.x * 0.5f, center.y), accent,
                  title.c_str());
    const ImVec2 tag_size = ImGui::CalcTextSize(tag.c_str());
    draw->AddText(ImVec2(center.x - tag_size.x * 0.5f, center.y + font_size + 12.0f),
                  palette::ink::muted, tag.c_str());
}

} // namespace thrystr::gui
