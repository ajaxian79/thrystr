#include <skald/theme.h>

#include <skald/tokens.h>

namespace skald {

using tokens::to_vec4;
using tokens::with_alpha;
namespace s_  = tokens::surface;
namespace b_  = tokens::border;
namespace i_  = tokens::ink;
namespace st_ = tokens::status;
namespace r_  = tokens::radii;
namespace p_  = tokens::pad;
namespace sz_ = tokens::sizes;

void ApplyDarkTheme(ImGuiStyle& s) {
    // Style vars — geometry first so later colour pushes line up.
    s.WindowPadding         = p_::window;
    s.FramePadding          = p_::standard;
    s.CellPadding           = p_::cell;
    s.ItemSpacing           = p_::item_sp;
    s.ItemInnerSpacing      = p_::inner_sp;
    s.IndentSpacing         = 16.0f;

    s.WindowRounding        = r_::none;
    s.ChildRounding         = 3.0f;
    s.FrameRounding         = r_::ctrl;
    s.PopupRounding         = r_::modal;
    s.ScrollbarRounding     = 3.0f;
    s.GrabRounding          = 3.0f;
    s.TabRounding           = 3.0f;

    s.WindowBorderSize      = 0.0f;
    s.ChildBorderSize       = 1.0f;
    s.PopupBorderSize       = 1.0f;
    s.FrameBorderSize       = 0.0f;
    s.TabBorderSize         = 0.0f;

    s.ScrollbarSize         = sz_::scrollbar;
    s.GrabMinSize           = sz_::grab_min;

    s.WindowTitleAlign      = ImVec2(0.0f, 0.5f);
    s.ButtonTextAlign       = ImVec2(0.5f, 0.5f);
    s.SelectableTextAlign   = ImVec2(0.0f, 0.5f);

    s.DisabledAlpha         = 0.45f;

    // Colours.
    auto& c = s.Colors;
    const ImVec4 win        = to_vec4(s_::window);
    const ImVec4 deep       = to_vec4(s_::deep);
    const ImVec4 panel      = to_vec4(s_::panel);
    const ImVec4 panel_alt  = to_vec4(s_::panel_alt);
    const ImVec4 control    = to_vec4(s_::control);
    const ImVec4 ctrl_hi    = to_vec4(s_::control_hi);
    const ImVec4 ctrl_act   = to_vec4(s_::control_act);
    const ImVec4 input      = to_vec4(s_::input);
    const ImVec4 modal      = to_vec4(s_::modal);
    const ImVec4 row_alt    = to_vec4(s_::row_alt);

    const ImVec4 hairline   = to_vec4(b_::hairline);
    const ImVec4 border     = to_vec4(b_::default_);
    const ImVec4 sep        = to_vec4(b_::separator);

    const ImVec4 text       = to_vec4(i_::primary);
    const ImVec4 text_muted = to_vec4(i_::muted);
    const ImVec4 text_dim   = to_vec4(i_::dim);

    // Backgrounds.
    c[ImGuiCol_WindowBg]            = win;
    c[ImGuiCol_ChildBg]             = win;
    c[ImGuiCol_PopupBg]             = deep;
    c[ImGuiCol_MenuBarBg]           = panel;
    c[ImGuiCol_ModalWindowDimBg]    = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
    c[ImGuiCol_TitleBg]             = panel;
    c[ImGuiCol_TitleBgActive]       = panel;
    c[ImGuiCol_TitleBgCollapsed]    = panel;

    // Borders & separators.
    c[ImGuiCol_Border]              = border;
    c[ImGuiCol_BorderShadow]        = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Separator]           = sep;
    c[ImGuiCol_SeparatorHovered]    = hairline;
    c[ImGuiCol_SeparatorActive]     = hairline;

    // Text.
    c[ImGuiCol_Text]                = text;
    c[ImGuiCol_TextDisabled]        = text_dim;
    c[ImGuiCol_TextSelectedBg]      = ImVec4(text.x, text.y, text.z, 0.20f);

    // Frames (inputs, sliders, combos).
    c[ImGuiCol_FrameBg]             = input;
    c[ImGuiCol_FrameBgHovered]      = ctrl_hi;
    c[ImGuiCol_FrameBgActive]       = ctrl_act;

    // Buttons.
    c[ImGuiCol_Button]              = control;
    c[ImGuiCol_ButtonHovered]       = ctrl_hi;
    c[ImGuiCol_ButtonActive]        = ctrl_act;

    // Headers (CollapsingHeader, Selectable, TreeNode).
    c[ImGuiCol_Header]              = panel_alt;
    c[ImGuiCol_HeaderHovered]       = ctrl_hi;
    c[ImGuiCol_HeaderActive]        = ctrl_act;

    // Tabs.
    c[ImGuiCol_Tab]                 = panel;
    c[ImGuiCol_TabHovered]          = ctrl_hi;
#ifdef ImGuiCol_TabSelected
    c[ImGuiCol_TabSelected]         = panel_alt;
    // No accent overline — it lives on the same axis as section
    // header underlines and they fight each other. Selection reads
    // through the panel_alt fill alone (see WorkspaceTabs /
    // DocumentTabs for vertical-left-edge accent on custom tabs).
    c[ImGuiCol_TabSelectedOverline] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TabDimmed]           = panel;
    c[ImGuiCol_TabDimmedSelected]   = panel_alt;
    c[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0, 0, 0, 0);
#else
    // Older ImGui (pre-1.91): TabActive / TabUnfocused.
    c[ImGuiCol_TabActive]           = panel_alt;
    c[ImGuiCol_TabUnfocused]        = panel;
    c[ImGuiCol_TabUnfocusedActive]  = panel_alt;
#endif

    // Scrollbar.
    c[ImGuiCol_ScrollbarBg]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]       = control;
    c[ImGuiCol_ScrollbarGrabHovered]= ctrl_hi;
    c[ImGuiCol_ScrollbarGrabActive] = ctrl_act;

    // Tables.
    c[ImGuiCol_TableHeaderBg]       = panel;
    c[ImGuiCol_TableBorderStrong]   = to_vec4(b_::strong);
    c[ImGuiCol_TableBorderLight]    = to_vec4(b_::default_);
    c[ImGuiCol_TableRowBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]       = row_alt;

    // Resize grip.
    c[ImGuiCol_ResizeGrip]          = to_vec4(b_::hairline);
    c[ImGuiCol_ResizeGripHovered]   = hairline;
    c[ImGuiCol_ResizeGripActive]    = border;

    // Plots.
    c[ImGuiCol_PlotLines]           = text_muted;
    c[ImGuiCol_PlotLinesHovered]    = text;
    c[ImGuiCol_PlotHistogram]       = text_muted;
    c[ImGuiCol_PlotHistogramHovered]= text;

    // Drag & drop.
    c[ImGuiCol_DragDropTarget]      = ImVec4(1, 1, 1, 0.4f);

    // Suppress modal/dim and the rare "BG" we want flat.
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(1, 1, 1, 0.7f);
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.40f);

    // Defer to ApplyAccent for accent-driven slots — set chrome
    // default so a host that forgets to call ApplyAccent still
    // looks coherent.
    ApplyAccent(s, tokens::accents::chrome);

    // Touch unused vars to avoid -Wunused-variable on older ImGui.
    (void)modal;
    (void)deep;
}

void ApplyAccent(ImGuiStyle& s, ImU32 accent) {
    auto& c = s.Colors;
    const ImVec4 a    = tokens::to_vec4(accent);
    const ImVec4 a_dim= ImVec4(a.x, a.y, a.z, 0.35f);

    c[ImGuiCol_CheckMark]           = a;
    c[ImGuiCol_SliderGrab]          = a;
    c[ImGuiCol_SliderGrabActive]    = a;
    c[ImGuiCol_TextSelectedBg]      = a_dim;
#ifdef ImGuiCol_NavCursor
    c[ImGuiCol_NavCursor]           = a;
#else
    c[ImGuiCol_NavHighlight]        = a;     // pre-1.91 keyboard-nav focus ring
#endif
    c[ImGuiCol_NavWindowingHighlight] = ImVec4(a.x, a.y, a.z, 0.75f);
    c[ImGuiCol_DragDropTarget]      = a;
    c[ImGuiCol_PlotLinesHovered]    = a;
    c[ImGuiCol_PlotHistogramHovered]= a;
}

void ApplyDefaults(ImU32 accent) {
    ApplyDarkTheme(ImGui::GetStyle());
    ApplyAccent(ImGui::GetStyle(), accent);
}

}  // namespace skald
