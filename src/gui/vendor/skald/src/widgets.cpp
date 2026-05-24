#include <skald/widgets.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include <skald/icons.gen.h>
#include <skald/tokens.h>

namespace skald {

namespace t   = tokens;
namespace s_  = tokens::surface;
namespace b_  = tokens::border;
namespace i_  = tokens::ink;
namespace st_ = tokens::status;
namespace r_  = tokens::radii;
namespace p_  = tokens::pad;

// ── Layout helpers ──────────────────────────────────────────────

void SectionHeader(std::string_view label) {
    auto* dl = ImGui::GetWindowDrawList();
    const float avail_w  = ImGui::GetContentRegionAvail().x;
    const float h        = t::sizes::section_h;
    const ImVec2 cursor  = ImGui::GetCursorScreenPos();

    // Title.
    ImGui::PushStyleColor(ImGuiCol_Text, t::to_vec4(i_::muted));
    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + 6.0f));
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopStyleColor();

    // Hairline under-rule.
    const ImVec2 a(cursor.x,           cursor.y + h - 1.0f);
    const ImVec2 b(cursor.x + avail_w, cursor.y + h - 1.0f);
    dl->AddLine(a, b, b_::separator, 1.0f);

    // Reserve full strip height + a small gap below.
    ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + h + 6.0f));
}

void MutedSeparator(float top_pad, float bot_pad) {
    auto* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    ImVec2 c = ImGui::GetCursorScreenPos();
    c.y += top_pad;
    dl->AddLine(c, ImVec2(c.x + w, c.y), b_::separator, 1.0f);
    ImGui::Dummy(ImVec2(w, top_pad + bot_pad + 1.0f));
}

// ── Buttons ─────────────────────────────────────────────────────

bool AccentButton(std::string_view label, ImVec2 size) {
    auto& s = ImGui::GetStyle();
    const ImU32 accent = ImGui::GetColorU32(s.Colors[ImGuiCol_CheckMark]);

    // v0.1: subdued at rest, accent emerges on hover/press. The button
    // reads as "the primary action" via its accent text colour at rest;
    // the hue only saturates when the user reaches for it. Adobe-aligned
    // restraint vs. the v0 always-saturated fill.
    const ImVec4 base = t::to_vec4(s_::control);                       // dark grey at rest
    const ImVec4 hov  = t::to_vec4(t::with_alpha(accent, 0.85f));      // accent fills on hover
    const ImVec4 act  = t::to_vec4(accent);                             // saturated when pressed

    ImGui::PushStyleColor(ImGuiCol_Button,        base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);
    // Accent ink at rest (the cue), deep ink only when the fill is hot —
    // ImGui can't push two-state text colours, so default to accent and
    // let the hover state carry visual weight via fill+text contrast.
    ImGui::PushStyleColor(ImGuiCol_Text,          t::to_vec4(accent));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r_::ctrl);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  p_::loose);

    char id[256];
    std::snprintf(id, sizeof id, "%.*s", static_cast<int>(label.size()), label.data());
    const bool clicked = ImGui::Button(id, size);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

bool GhostButton(std::string_view label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, t::to_vec4(s_::control));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  t::to_vec4(s_::control_hi));
    ImGui::PushStyleColor(ImGuiCol_Border,        t::to_vec4(b_::default_));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r_::ctrl);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, p_::loose);

    char id[256];
    std::snprintf(id, sizeof id, "%.*s", static_cast<int>(label.size()), label.data());
    const bool clicked = ImGui::Button(id, size);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);
    return clicked;
}

void Tooltip(std::string_view text) {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, t::to_vec4(s_::panel_alt));
    ImGui::PushStyleColor(ImGuiCol_Border,  t::to_vec4(b_::strong));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
    if (ImGui::BeginTooltip()) {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

bool IconButton(std::string_view glyph_or_label,
                std::string_view tooltip,
                float            size_px) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, t::to_vec4(s_::control));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  t::to_vec4(s_::control_hi));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, r_::ctrl);

    char id[256];
    std::snprintf(id, sizeof id, "%.*s",
                  static_cast<int>(glyph_or_label.size()),
                  glyph_or_label.data());
    const bool clicked = ImGui::Button(id, ImVec2(size_px, size_px));

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);

    if (!tooltip.empty() && ImGui::IsItemHovered()) {
        Tooltip(tooltip);
    }
    return clicked;
}

// ── Toggles / state ─────────────────────────────────────────────

bool PillToggle(std::string_view label, bool* state) {
    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (!win || win->SkipItems) return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float h     = 22.0f;
    const float pill_w= 38.0f;
    const float gap   = 8.0f;

    char buf[256];
    std::snprintf(buf, sizeof buf, "%.*s",
                  static_cast<int>(label.size()), label.data());
    // Honour ImGui's "##" / "###" hide-from-display convention so callers
    // can pass invisible IDs (e.g. "##hold") inside tables.
    const char* visible = buf;
    const char* visible_end = buf + std::strlen(buf);
    if (const char* hash = std::strstr(buf, "##")) visible_end = hash;
    const bool show_label = (visible_end - visible) > 0;
    const ImVec2 text_sz = show_label
        ? ImGui::CalcTextSize(visible, visible_end)
        : ImVec2(0.0f, 0.0f);
    const float total_w  = show_label
        ? (pill_w + gap + text_sz.x)
        : pill_w;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImRect bb(pos, ImVec2(pos.x + total_w, pos.y + h));

    ImGui::ItemSize(bb, style.FramePadding.y);
    const ImGuiID id = win->GetID(buf);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed && state) *state = !*state;

    auto* dl = win->DrawList;
    const ImU32 accent = ImGui::GetColorU32(style.Colors[ImGuiCol_CheckMark]);
    const ImU32 track  = (state && *state)
                       ? t::with_alpha(accent, 0.40f)
                       : s_::control;
    const ImU32 dot    = (state && *state) ? accent : i_::muted;

    const ImVec2 pill_a(pos.x,          pos.y);
    const ImVec2 pill_b(pos.x + pill_w, pos.y + h);
    dl->AddRectFilled(pill_a, pill_b, track, h * 0.5f);

    const float dot_r = (h - 6.0f) * 0.5f;
    const float dot_x = (state && *state)
                      ? pill_b.x - dot_r - 3.0f
                      : pill_a.x + dot_r + 3.0f;
    dl->AddCircleFilled(ImVec2(dot_x, pos.y + h * 0.5f), dot_r, dot);

    if (show_label) {
        dl->AddText(ImVec2(pill_b.x + gap,
                           pos.y + (h - text_sz.y) * 0.5f),
                    i_::primary, visible, visible_end);
    }
    return pressed;
}

void BadgeChip(std::string_view text, BadgeTone tone) {
    ImU32 fill, ink_;
    switch (tone) {
        case BadgeTone::Accent: {
            const ImU32 accent = ImGui::GetColorU32(
                ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
            fill = t::with_alpha(accent, 0.18f);
            ink_ = accent;
            break;
        }
        case BadgeTone::Success:     fill = t::with_alpha(st_::success,     0.20f); ink_ = st_::success;     break;
        case BadgeTone::Warning:     fill = t::with_alpha(st_::warning,     0.22f); ink_ = st_::warning;     break;
        case BadgeTone::Destructive: fill = t::with_alpha(st_::destructive, 0.22f); ink_ = st_::destructive; break;
        case BadgeTone::Info:        fill = t::with_alpha(st_::info,        0.20f); ink_ = st_::info;        break;
        case BadgeTone::Muted:
        default:                     fill = s_::control; ink_ = i_::muted; break;
    }

    char buf[256];
    std::snprintf(buf, sizeof buf, "%.*s",
                  static_cast<int>(text.size()), text.data());
    const ImVec2 ts = ImGui::CalcTextSize(buf);
    // Pad chip slightly taller than the cap-height so the text lifts
    // off the baseline cleanly. ImGui's CalcTextSize returns the
    // line-height; the visual baseline sits ~75% down. Centre the
    // line-height inside the chip but offset down 1 px so the
    // optical centre lands on the chip's centre.
    const ImVec2 padding(8.0f, 0.0f);
    const float  chip_h = ts.y + 6.0f;          // 2 px above + 4 px below
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(ts.x + padding.x * 2, chip_h);

    auto* dl = ImGui::GetWindowDrawList();
    // Radius respects skald's 2-5 px ceiling (was chip_h*0.5 ≈ 10 px).
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      fill, t::radii::ctrl);
    // Optical-centre the text inside the chip.
    dl->AddText(ImVec2(pos.x + padding.x,
                       pos.y + (chip_h - ts.y) * 0.5f),
                ink_, buf);
    ImGui::Dummy(size);
}

// ── Tables / rows ───────────────────────────────────────────────

// 4px right-edge padding so values don't kiss the panel border.
constexpr float kKvRightPad = 4.0f;

void KvRow(std::string_view label, const char* value_fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, value_fmt);
    std::vsnprintf(buf, sizeof buf, value_fmt, ap);
    va_end(ap);

    const float row_h = 24.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float full_w = ImGui::GetContentRegionAvail().x;

    auto* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(pos.x, pos.y + 4.0f),
                i_::primary,
                label.data(), label.data() + label.size());

    const ImVec2 vs = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(pos.x + full_w - vs.x - kKvRightPad, pos.y + 4.0f),
                i_::muted, buf);
    ImGui::Dummy(ImVec2(full_w, row_h));
}

void KvRowStatus(std::string_view label, std::string_view value,
                 BadgeTone tone) {
    ImU32 ink_;
    switch (tone) {
        case BadgeTone::Success:     ink_ = st_::success;     break;
        case BadgeTone::Warning:     ink_ = st_::warning;     break;
        case BadgeTone::Destructive: ink_ = st_::destructive; break;
        case BadgeTone::Info:        ink_ = st_::info;        break;
        case BadgeTone::Accent:      ink_ = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_CheckMark]); break;
        default:                     ink_ = i_::muted;        break;
    }

    const float row_h = 24.0f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float full_w = ImGui::GetContentRegionAvail().x;

    auto* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(pos.x, pos.y + 4.0f),
                i_::primary,
                label.data(), label.data() + label.size());
    char buf[256];
    std::snprintf(buf, sizeof buf, "%.*s",
                  static_cast<int>(value.size()), value.data());
    const ImVec2 vs = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(pos.x + full_w - vs.x - kKvRightPad, pos.y + 4.0f),
                ink_, buf);
    ImGui::Dummy(ImVec2(full_w, row_h));
}

// ── Containers ──────────────────────────────────────────────────

bool BeginPanel(std::string_view id, ImVec2 size, ImU32 tint) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "##skald_panel_%.*s",
                  static_cast<int>(id.size()), id.data());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, t::to_vec4(tint));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, r_::ctrl);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, p_::window);
    return ImGui::BeginChild(buf, size, ImGuiChildFlags_None,
                             ImGuiWindowFlags_NoScrollbar);
}

void EndPanel() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ── Application shell ───────────────────────────────────────────

bool WorkspaceTabs(std::span<const WorkspaceTab> tabs, int* selected) {
    auto& io = ImGui::GetIO();
    auto* dl = ImGui::GetWindowDrawList();
    const float h = t::sizes::wsbar_h;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float full_w = ImGui::GetContentRegionAvail().x;
    const ImU32 accent = ImGui::GetColorU32(
        ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);

    // Strip background — slightly raised above the window.
    dl->AddRectFilled(origin,
                      ImVec2(origin.x + full_w, origin.y + h),
                      s_::panel);
    // Hairline under the strip.
    dl->AddLine(ImVec2(origin.x,          origin.y + h - 0.5f),
                ImVec2(origin.x + full_w, origin.y + h - 0.5f),
                b_::separator, 1.0f);

    bool changed = false;
    float x = origin.x + 12.0f;

    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
        const auto& tab = tabs[i];
        const bool sel  = (selected && *selected == i);

        char label_buf[128];
        const int n = std::snprintf(
            label_buf, sizeof label_buf, "%.*s",
            static_cast<int>(tab.label.size()), tab.label.data());
        const ImVec2 ts = ImGui::CalcTextSize(label_buf, label_buf + n);

        // 14 px horizontal padding.
        const float tab_w = ts.x + 28.0f
                          + (tab.icon.empty() ? 0.0f : 18.0f);
        const ImVec2 a(x,         origin.y);
        const ImVec2 b(x + tab_w, origin.y + h);

        char id_buf[128];
        std::snprintf(id_buf, sizeof id_buf, "##wstab_%d", i);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id_buf, ImVec2(tab_w, h));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        if (clicked && selected) { *selected = i; changed = true; }

        // Selected: vertical accent bar on the LEFT edge of the tab
        // (so it doesn't share an axis with section-header underlines
        // that sit just below the strip). Plus primary ink.
        // Hover: row tint, muted ink.
        // Rest: muted ink, no fill.
        if (sel) {
            dl->AddRectFilled(ImVec2(a.x,         a.y + 6.0f),
                              ImVec2(a.x + 2.0f,  b.y - 6.0f),
                              accent);
        } else if (hovered) {
            dl->AddRectFilled(a, b, b_::hairline);
        }

        // Optional icon glyph (consumer supplies via icon font).
        float text_x = a.x + 14.0f;
        if (!tab.icon.empty()) {
            dl->AddText(ImVec2(text_x, a.y + (h - 16.0f) * 0.5f),
                        sel ? i_::primary : i_::muted,
                        tab.icon.data(),
                        tab.icon.data() + tab.icon.size());
            text_x += 20.0f;
        }
        dl->AddText(ImVec2(text_x, a.y + (h - ts.y) * 0.5f),
                    sel ? i_::primary : i_::muted,
                    label_buf, label_buf + n);

        x += tab_w + 4.0f;
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + h));
    (void)io;
    return changed;
}

bool DocumentTabs(std::span<const DocumentTab> tabs,
                  int*  selected,
                  int*  closed_out) {
    auto* dl = ImGui::GetWindowDrawList();
    const float h = t::sizes::doctab_h;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float full_w = ImGui::GetContentRegionAvail().x;
    const ImU32 accent = ImGui::GetColorU32(
        ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);

    dl->AddRectFilled(origin,
                      ImVec2(origin.x + full_w, origin.y + h),
                      s_::window);
    dl->AddLine(ImVec2(origin.x,          origin.y + h - 0.5f),
                ImVec2(origin.x + full_w, origin.y + h - 0.5f),
                b_::separator, 1.0f);

    bool changed = false;
    if (closed_out) *closed_out = -1;
    float x = origin.x + 8.0f;

    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
        const auto& tab = tabs[i];
        const bool sel  = (selected && *selected == i);

        char label_buf[128];
        const int n = std::snprintf(
            label_buf, sizeof label_buf, "%.*s",
            static_cast<int>(tab.title.size()), tab.title.data());
        const ImVec2 ts = ImGui::CalcTextSize(label_buf, label_buf + n);
        const float tab_w = ts.x + 36.0f + (tab.dirty ? 10.0f : 0.0f);

        const ImVec2 a(x,         origin.y + 4.0f);
        const ImVec2 b(x + tab_w, origin.y + h);

        char id_buf[128];
        std::snprintf(id_buf, sizeof id_buf, "##doctab_%d", i);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id_buf, ImVec2(tab_w, h - 4.0f));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        if (clicked && selected) { *selected = i; changed = true; }

        // Selected tab: panel surface + vertical accent bar on the
        // LEFT edge (matches the workspace strip; no horizontal
        // marker that would clash with section-header rules).
        // Other tabs: window surface, muted ink.
        if (sel) {
            dl->AddRectFilled(a, ImVec2(b.x, b.y), s_::panel,
                              t::radii::ctrl,
                              ImDrawFlags_RoundCornersTop);
            dl->AddRectFilled(ImVec2(a.x,         a.y + 4.0f),
                              ImVec2(a.x + 2.0f,  b.y - 4.0f),
                              accent);
        } else if (hovered) {
            dl->AddRectFilled(a, ImVec2(b.x, b.y), b_::hairline,
                              t::radii::ctrl,
                              ImDrawFlags_RoundCornersTop);
        }

        // Dirty dot.
        float text_x = a.x + 12.0f;
        if (tab.dirty) {
            dl->AddCircleFilled(
                ImVec2(text_x, a.y + (h - 4.0f) * 0.5f),
                3.0f, accent);
            text_x += 10.0f;
        }
        dl->AddText(ImVec2(text_x, a.y + (h - 4.0f - ts.y) * 0.5f),
                    sel ? i_::primary : i_::muted,
                    label_buf, label_buf + n);

        // Close affordance — small × on hover, always shown if 2+ tabs.
        if (tabs.size() > 1 && (sel || hovered)) {
            const ImVec2 cx_pos(b.x - 16.0f, a.y + (h - 4.0f) * 0.5f);
            char close_id[64];
            std::snprintf(close_id, sizeof close_id, "##close_%d", i);
            ImGui::SetCursorScreenPos(ImVec2(cx_pos.x - 8.0f,
                                             cx_pos.y - 8.0f));
            ImGui::InvisibleButton(close_id, ImVec2(16.0f, 16.0f));
            if (ImGui::IsItemHovered()) {
                dl->AddCircleFilled(cx_pos, 7.0f, b_::default_);
            }
            // Hand-drawn ×.
            dl->AddLine(ImVec2(cx_pos.x - 3, cx_pos.y - 3),
                        ImVec2(cx_pos.x + 3, cx_pos.y + 3),
                        ImGui::IsItemHovered() ? i_::primary : i_::muted,
                        1.0f);
            dl->AddLine(ImVec2(cx_pos.x + 3, cx_pos.y - 3),
                        ImVec2(cx_pos.x - 3, cx_pos.y + 3),
                        ImGui::IsItemHovered() ? i_::primary : i_::muted,
                        1.0f);
            if (ImGui::IsItemClicked() && closed_out) {
                *closed_out = i;
                changed = true;
            }
        }

        x += tab_w + 1.0f;
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + h));
    return changed;
}

void BeginToolbar(float height) {
    auto* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float full_w = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(origin,
                      ImVec2(origin.x + full_w, origin.y + height),
                      s_::panel_alt);
    dl->AddLine(ImVec2(origin.x,          origin.y + height - 0.5f),
                ImVec2(origin.x + full_w, origin.y + height - 0.5f),
                b_::separator, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##skald_toolbar", ImVec2(0, height),
                      ImGuiChildFlags(0));
}

void EndToolbar() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Helper: draw stylized SKALD wordmark — uppercase block letters with
// horizontal cut-through "vents" (Aliens-poster idiom) plus glowing
// accent dots between the letters. The cuts are drawn as thin
// background-coloured rects on top of the rendered text so the letter
// glyphs read as segmented blocks.
static void draw_wordmark(ImDrawList* dl,
                          ImVec2       centre,
                          ImFont*      font,
                          float        font_px,
                          std::string_view text,
                          ImU32        accent,
                          ImU32        bg_for_cuts) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*s",
                  static_cast<int>(text.size()), text.data());
    // Force uppercase.
    for (char* p = buf; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 32);
    }

    // Letter-spacing: ImGui doesn't support tracking, so render each
    // letter individually with a fixed advance gap.
    const float tracking = font_px * 0.55f;     // gap between letters
    float total_w = 0.0f;
    for (char* p = buf; *p; ++p) {
        char one[2] = {*p, 0};
        const ImVec2 sz = font->CalcTextSizeA(font_px, FLT_MAX, 0.0f, one);
        total_w += sz.x + tracking;
    }
    total_w -= tracking;
    const float text_h = font->CalcTextSizeA(font_px, FLT_MAX, 0.0f, "M").y;

    // Soft accent halo behind the wordmark.
    for (int r = 0; r < 24; ++r) {
        const float rad = font_px * (1.6f + static_cast<float>(r) * 0.18f);
        const float a   = 0.020f * (1.0f - static_cast<float>(r) / 24.0f);
        dl->AddCircleFilled(centre, rad, t::with_alpha(accent, a), 48);
    }

    // Render each letter (clean — no cuts) then a glowing accent dot
    // between adjacent letters.
    float x = centre.x - total_w * 0.5f;
    const float y = centre.y - text_h * 0.5f;
    for (char* p = buf; *p; ++p) {
        char one[2] = {*p, 0};
        const ImVec2 sz = font->CalcTextSizeA(font_px, FLT_MAX, 0.0f, one);
        // Faint accent rim — soft 1 px halo under the glyph.
        dl->AddText(font, font_px, ImVec2(x + 1, y + 1),
                    t::with_alpha(accent, 0.45f), one);
        // Main glyph in primary ink.
        dl->AddText(font, font_px, ImVec2(x, y), i_::primary, one);

        x += sz.x;
        // Glowing accent dot between letters (skip after last).
        if (*(p + 1) != '\0') {
            const float dot_x = x + tracking * 0.5f;
            const float dot_y = centre.y;
            for (int gr = 0; gr < 4; ++gr) {
                const float rad = 6.0f - static_cast<float>(gr) * 1.0f;
                const float aa  = 0.12f + static_cast<float>(gr) * 0.06f;
                if (rad <= 0) break;
                dl->AddCircleFilled(ImVec2(dot_x, dot_y), rad,
                                    t::with_alpha(accent, aa), 16);
            }
            dl->AddCircleFilled(ImVec2(dot_x, dot_y), 1.8f, accent, 16);
        }
        x += tracking;
    }
    (void)bg_for_cuts;
}

SplashChoice Splash(std::string_view              wordmark,
                    std::string_view              tagline,
                    std::span<const SplashRecent> recents,
                    std::span<const SplashAction> actions,
                    ImTextureID                   hero_tex,
                    ImVec2                        hero_size,
                    ImFont*                       hero_font) {
    SplashChoice out;
    auto* dl = ImGui::GetWindowDrawList();
    auto& io = ImGui::GetIO();
    const ImVec2 size = io.DisplaySize;
    const ImU32 accent = ImGui::GetColorU32(
        ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);

    // ── Layout (relative to window size; tuned at 800x600 default) ──
    const float hero_h_frac    = 0.62f;            // top hero band
    const float hero_h         = size.y * hero_h_frac;
    const float black_y        = hero_h;           // start of black band

    // Top hero region — pure deep with a faint vertical gradient,
    // optional Blender-rendered hero image, faint grid, and a strong
    // accent radial behind the wordmark.
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, hero_h),
                      s_::deep);

    // Hero image (Blender render) — fitted to cover the hero band,
    // centred, with the bottom edge fading to deep so the wordmark
    // sits cleanly on top.
    if (hero_tex && hero_size.x > 0 && hero_size.y > 0) {
        const float scale = std::max(size.x / hero_size.x,
                                     hero_h / hero_size.y);
        const float w = hero_size.x * scale;
        const float h = hero_size.y * scale;
        const float x = (size.x - w) * 0.5f;
        const float y = -(h - hero_h) * 0.5f;
        dl->AddImage(hero_tex, ImVec2(x, y),
                     ImVec2(x + w, y + h));
        // Bottom-fade so the image blends to black at the band edge.
        dl->AddRectFilledMultiColor(
            ImVec2(0, hero_h - 60.0f), ImVec2(size.x, hero_h),
            IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
            s_::deep, s_::deep);
    }

    // 32 px grid, very faint, only in upper hero band.
    for (float gx = 0; gx < size.x; gx += 32.0f) {
        dl->AddLine(ImVec2(gx, 0), ImVec2(gx, hero_h),
                    t::with_alpha(IM_COL32(255,255,255,255), 0.025f), 1.0f);
    }
    for (float gy = 0; gy < hero_h; gy += 32.0f) {
        dl->AddLine(ImVec2(0, gy), ImVec2(size.x, gy),
                    t::with_alpha(IM_COL32(255,255,255,255), 0.025f), 1.0f);
    }

    // Spotlight radial behind the wordmark — sits a touch lower in
    // the hero band so the wordmark feels grounded.
    const ImVec2 c(size.x * 0.5f, hero_h * 0.68f);
    const float spot_r = std::min(size.x, hero_h) * 0.45f;
    for (int r = static_cast<int>(spot_r); r > 0; r -= 8) {
        const float a = 0.020f * static_cast<float>(r) / spot_r;
        dl->AddCircleFilled(c, static_cast<float>(r),
                            t::with_alpha(accent, a), 64);
    }

    // ── Wordmark + tagline (in the lower portion of the hero band) ─
    ImFont* font = hero_font ? hero_font : ImGui::GetFont();
    // Cap the wordmark size so it stays balanced in the 800x600
    // default frame (was 0.10*y — pushed too tall on portraits).
    const float fs_word = std::max(48.0f, std::min(72.0f, size.y * 0.11f));
    draw_wordmark(dl, c, font, fs_word, wordmark, accent, s_::deep);

    // Tagline beneath the wordmark.
    char tag[256];
    std::snprintf(tag, sizeof tag, "%.*s",
                  static_cast<int>(tagline.size()), tagline.data());
    for (char* p = tag; *p; ++p) {
        if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 32);
    }
    const ImVec2 tag_sz = ImGui::CalcTextSize(tag);
    const float  tag_y  = c.y + fs_word * 0.7f + 14.0f;
    dl->AddText(ImVec2(c.x - tag_sz.x * 0.5f, tag_y), i_::muted, tag);

    // Hairline accent rules — vertically CENTERED on the tagline
    // text (was previously below it), one on each side.
    const float rule_y = tag_y + tag_sz.y * 0.5f;
    const float rule_w = 64.0f;
    dl->AddLine(ImVec2(c.x - tag_sz.x * 0.5f - rule_w - 10, rule_y),
                ImVec2(c.x - tag_sz.x * 0.5f - 10,          rule_y),
                accent, 1.0f);
    dl->AddLine(ImVec2(c.x + tag_sz.x * 0.5f + 10,          rule_y),
                ImVec2(c.x + tag_sz.x * 0.5f + rule_w + 10, rule_y),
                accent, 1.0f);

    // ── Black bottom band (3 columns; only the right 2 are used) ───
    dl->AddRectFilled(ImVec2(0, black_y),
                      ImVec2(size.x, size.y),
                      IM_COL32(0, 0, 0, 255));

    // Divide the band into 3 equal columns. Leave the LEFT column
    // empty for visual breathing room; populate the middle and right.
    const float pad_x   = 36.0f;
    const float col_gap = 32.0f;
    const float col_w   = (size.x - pad_x * 2 - col_gap * 2) / 3.0f;
    const float left_x  = pad_x + col_w + col_gap;             // middle col x
    const float right_x = left_x + col_w + col_gap;            // right col x
    const float panel_y = black_y + 28.0f;
    const float row_h   = 36.0f;

    auto draw_heading = [&](float x, const char* label) {
        // Small accent rule + uppercase muted heading.
        dl->AddLine(ImVec2(x, panel_y - 4),
                    ImVec2(x + 24.0f, panel_y - 4),
                    accent, 2.0f);
        dl->AddText(ImVec2(x, panel_y),
                    i_::muted, label);
    };
    draw_heading(left_x,  "RECENT");
    draw_heading(right_x, "START");

    auto truncate_to_width = [&](char* s, float max_w) {
        const ImVec2 sz = ImGui::CalcTextSize(s);
        if (sz.x <= max_w) return;
        const std::size_t n = std::strlen(s);
        for (std::size_t k = n; k > 1; --k) {
            s[k - 1] = '\0';
            char trial[512];
            std::snprintf(trial, sizeof trial, "%s...", s);
            if (ImGui::CalcTextSize(trial).x <= max_w) {
                std::strcpy(s, trial);
                return;
            }
        }
    };

    // Recents.
    for (int i = 0; i < static_cast<int>(recents.size()); ++i) {
        const auto& r = recents[i];
        const float y = panel_y + 24.0f + static_cast<float>(i) * row_h;
        const ImVec2 a(left_x - 4,        y - 4);
        const ImVec2 b(left_x + col_w + 4, y + row_h - 8);
        char id[64];
        std::snprintf(id, sizeof id, "##sp_recent_%d", i);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
        const bool hov = ImGui::IsItemHovered();
        if (hov) dl->AddRectFilled(a, b, t::with_alpha(accent, 0.12f),
                                   t::radii::ctrl);
        if (ImGui::IsItemClicked()) {
            out.kind  = SplashChoice::Kind::Recent;
            out.index = i;
        }
        char title_buf[128];
        std::snprintf(title_buf, sizeof title_buf, "%.*s",
                      static_cast<int>(r.title.size()), r.title.data());
        truncate_to_width(title_buf, col_w - 8);
        char path_buf[256];
        std::snprintf(path_buf, sizeof path_buf, "%.*s  -  %.*s",
                      static_cast<int>(r.path.size()),     r.path.data(),
                      static_cast<int>(r.modified.size()), r.modified.data());
        truncate_to_width(path_buf, col_w - 8);
        dl->AddText(ImVec2(left_x, y),
                    hov ? accent : i_::primary, title_buf);
        dl->AddText(ImVec2(left_x, y + 18),
                    i_::dim, path_buf);
    }

    // Actions.
    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
        const auto& act_ = actions[i];
        const float y = panel_y + 24.0f + static_cast<float>(i) * row_h;
        const ImVec2 a(right_x - 4,         y - 4);
        const ImVec2 b(right_x + col_w + 4, y + row_h - 8);
        char id[64];
        std::snprintf(id, sizeof id, "##sp_action_%d", i);
        ImGui::SetCursorScreenPos(a);
        ImGui::InvisibleButton(id, ImVec2(b.x - a.x, b.y - a.y));
        const bool hov = ImGui::IsItemHovered();
        if (hov) dl->AddRectFilled(a, b, t::with_alpha(accent, 0.12f),
                                   t::radii::ctrl);
        if (ImGui::IsItemClicked()) {
            out.kind  = SplashChoice::Kind::Action;
            out.index = i;
        }
        char label_buf[128];
        std::snprintf(label_buf, sizeof label_buf, "%.*s",
                      static_cast<int>(act_.label.size()),
                      act_.label.data());
        truncate_to_width(label_buf, col_w - 80);
        dl->AddText(ImVec2(right_x, y + 6),
                    hov ? accent : i_::primary, label_buf);
        if (!act_.shortcut.empty()) {
            char sc[32];
            std::snprintf(sc, sizeof sc, "%.*s",
                          static_cast<int>(act_.shortcut.size()),
                          act_.shortcut.data());
            const ImVec2 sc_sz = ImGui::CalcTextSize(sc);
            dl->AddText(ImVec2(right_x + col_w - sc_sz.x, y + 6),
                        i_::dim, sc);
        }
    }

    return out;
}

// ── File / folder dialog ────────────────────────────────────────

void OpenFileDialog(const char* popup_id) {
    ImGui::OpenPopup(popup_id);
}

FileDialogResult BeginFileDialog(const char*                       popup_id,
                                 FileDialogMode                    mode,
                                 FileDialogState&                  state,
                                 std::span<const FileDialogEntry>  entries) {
    FileDialogResult out = FileDialogResult::None;

    ImGui::SetNextWindowSize(ImVec2(680.0f, 480.0f), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, t::to_vec4(s_::panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    if (!ImGui::BeginPopupModal(popup_id, nullptr,
                                ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoResize)) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return out;
    }

    SectionHeader(mode == FileDialogMode::Open ? "Open file" : "Save file");

    // Breadcrumb row: cwd input + filter input.
    const float btn_w = 28.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg, t::to_vec4(s_::input));
    ImGui::SetNextItemWidth(-(180.0f + btn_w + 12.0f));
    ImGui::InputTextWithHint("##cwd", "path", state.cwd, sizeof state.cwd);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputTextWithHint("##filter", "filter", state.filter, sizeof state.filter);
    ImGui::SameLine();
    IconButton(skald::icons::kCog, "Folder options", btn_w);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    // Listing.
    const float listing_h = 270.0f;
    if (ImGui::BeginTable("##fdlist", 3,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerH |
                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, listing_h))) {
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size",     ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            const auto& e = entries[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool sel = (i == state.row_sel);
            // Icon glyph + name.
            char row_label[320];
            std::snprintf(row_label, sizeof row_label, "%s  %.*s",
                          e.is_folder ? skald::icons::kFolder : skald::icons::kFile,
                          static_cast<int>(e.name.size()), e.name.data());
            if (ImGui::Selectable(row_label, sel,
                                  ImGuiSelectableFlags_SpanAllColumns,
                                  ImVec2(0, 24.0f))) {
                state.row_sel = i;
                if (!e.is_folder) {
                    std::snprintf(state.filename, sizeof state.filename, "%.*s",
                                  static_cast<int>(e.name.size()), e.name.data());
                }
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.size.data(), e.size.data() + e.size.size());
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, t::to_vec4(i_::muted));
            ImGui::TextUnformatted(e.modified.data(), e.modified.data() + e.modified.size());
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, 12));

    // Filename input row.
    ImGui::TextUnformatted("Filename");
    ImGui::SameLine(120.0f);
    const float action_w = 240.0f;
    ImGui::SetNextItemWidth(-(action_w + 12.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, t::to_vec4(s_::input));
    ImGui::InputText("##fname", state.filename, sizeof state.filename);
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Actions.
    if (GhostButton("Cancel")) {
        out = FileDialogResult::Cancelled;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (AccentButton(mode == FileDialogMode::Open ? "Open" : "Save")) {
        out = FileDialogResult::Confirmed;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    (void)popup_id;
    return out;
}

}  // namespace skald
