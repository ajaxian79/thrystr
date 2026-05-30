// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/timeline_draw.hpp>

#include <thrystr/gui/palette.hpp>

#include <cfloat>

namespace thrystr::gui {

namespace {

ImVec2 label_position(float x, float y, const ImVec2& size) { return {x - size.x * 0.5f, y}; }

void draw_current_label(ImDrawList* draw, const TimelinePlotArea& area,
                        const TimelineTicker& ticker, float top) {
    if (ticker.current_text.empty()) {
        return;
    }
    ImFont* font = ticker.current_font ? ticker.current_font : ImGui::GetFont();
    const float size = ImGui::GetFontSize() * ticker.current_font_scale;
    const float x = area.plot_left + static_cast<float>(ticker.current_index) * area.x_step;
    const ImVec2 text =
        font->CalcTextSizeA(size, FLT_MAX, 0.0f, ticker.current_text.data(),
                            ticker.current_text.data() + ticker.current_text.size());
    const ImVec2 pos = label_position(x, top, text);
    draw->AddRectFilled(ImVec2(pos.x - 6.0f, pos.y - 3.0f),
                        ImVec2(pos.x + text.x + 6.0f, pos.y + text.y + 3.0f),
                        palette::surface::control_hi, palette::radii::ctrl);
    draw->AddText(font, size, pos, palette::ink::primary, ticker.current_text.data(),
                  ticker.current_text.data() + ticker.current_text.size());
}

} // namespace

void draw_timeline_ticker(ImDrawList* draw, const TimelinePlotArea& area,
                          const TimelineTicker& ticker) {
    if (!draw) {
        return;
    }
    const float top = area.plot_bottom + 58.0f;
    const float bottom = area.clip_max.y - 8.0f;
    if (bottom <= top) {
        return;
    }
    draw->AddRectFilled(ImVec2(area.clip_min.x, top - 4.0f), ImVec2(area.clip_max.x, bottom),
                        palette::surface::panel_alt);
    draw->AddLine(ImVec2(area.clip_min.x, top - 4.0f), ImVec2(area.clip_max.x, top - 4.0f),
                  palette::border::separator, 1.0f);
    for (const TimelineTextLabel& label : ticker.labels) {
        if (label.index == ticker.current_index) {
            continue;
        }
        const float x = area.plot_left + static_cast<float>(label.index) * area.x_step;
        const ImVec2 text = ImGui::CalcTextSize(label.text.c_str());
        draw->AddText(label_position(x, top, text), palette::ink::muted, label.text.c_str());
    }
    draw_current_label(draw, area, ticker, top);
}

} // namespace thrystr::gui
