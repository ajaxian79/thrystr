#pragma once

// skald::widgets — small set of opinionated widget helpers that apply
// the palette and spacing rules from tokens.h. Apps that need only
// the global theme can ignore this header; apps that want the
// shared chrome (section headers, accent buttons, pill toggles, kv
// rows, inline badges) include it.
//
// All widgets push their own colours / vars internally and pop on
// exit, so they're safe to interleave with raw ImGui code.

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include <skald/tokens.h>

namespace skald {

// ── Layout helpers ──────────────────────────────────────────────

// A flat, left-aligned section title with a thin under-rule and a
// subtle uppercase letter-spacing-ish look (achieved via padding +
// muted ink — ImGui doesn't track letter-spacing). Use to introduce
// a panel section.
void SectionHeader(std::string_view label);

// Horizontal hairline at the current cursor, full content width,
// using tokens::border::separator. Less heavy than ImGui::Separator.
void MutedSeparator(float top_pad = 4.0f, float bot_pad = 4.0f);

// ── Buttons ─────────────────────────────────────────────────────

// Primary action — accent-tinted background, accent-coloured text on
// hover. Use one per surface (Adobe-style: one primary CTA visible).
bool AccentButton(std::string_view label, ImVec2 size = ImVec2(0, 0));

// Secondary action — transparent background, hairline border, text
// only. Default state for "Cancel", "Reset", and other reversible
// actions.
bool GhostButton(std::string_view label, ImVec2 size = ImVec2(0, 0));

// Square icon-only button (use with a ttf-loaded icon font glyph).
// `tooltip` is shown on hover after default delay using skald::Tooltip.
bool IconButton(std::string_view glyph_or_label,
                std::string_view tooltip = {},
                float            size_px = 28.0f);

// Themed tooltip — distinct panel_alt background + strong border so it
// reads off the parent surface without a colour clash.
void Tooltip(std::string_view text);

// ── Toggles / state ─────────────────────────────────────────────

// A pill-shaped on/off toggle. The "off" state shows a muted dot;
// "on" shows the accent dot at the right side and the pill fills
// with tinted accent. Returns true on toggle.
bool PillToggle(std::string_view label, bool* state);

// A small static badge — used for tags, status chips, keyboard
// shortcuts. `tone` picks the colour; default is muted.
enum class BadgeTone { Muted, Accent, Success, Warning, Destructive, Info };
void BadgeChip(std::string_view text, BadgeTone tone = BadgeTone::Muted);

// ── Tables / rows ───────────────────────────────────────────────

// A "label : value" row with primary ink for the label, monospace +
// muted ink for the value. Used in inspector panels (FPS / GPU mem /
// frame time / asset paths). printf-style formatting on the value.
void KvRow(std::string_view label, const char* value_fmt, ...) IM_FMTARGS(2);

// Same as KvRow but the value is a status colour. Use sparingly —
// for state, not style.
void KvRowStatus(std::string_view label, std::string_view value,
                 BadgeTone tone);

// ── Containers ──────────────────────────────────────────────────

// Begin / End a flat panel (no border, panel surface, standard
// padding). Always pair Begin/End. Returns whether the panel is
// visible (always true on this default impl; future scroll/clip
// support).
bool BeginPanel(std::string_view id,
                ImVec2           size  = ImVec2(0, 0),
                ImU32            tint  = tokens::surface::panel);
void EndPanel();

// ── Application shell ───────────────────────────────────────────

// Blender-style workspace tabs along the top of the app. Each tab is
// a logical layout preset ("Layout", "Modeling", "Sculpting", etc.);
// switching tabs swaps the panel arrangement. Returns true on the
// frame `*selected` changed.
struct WorkspaceTab {
    std::string_view id;     // e.g. "layout"
    std::string_view label;  // e.g. "Layout"
    std::string_view icon;   // optional icon-font glyph (use kIconXxx)
};
bool WorkspaceTabs(std::span<const WorkspaceTab> tabs, int* selected);

// Document tabs (per-workspace open files). `closeable` items show an
// inline close affordance on hover. Returns true on tab change OR
// close click; on close, sets `*closed = index_of_closed`.
struct DocumentTab {
    std::string_view title;
    bool             dirty = false;     // shows a small dot when true
};
bool DocumentTabs(std::span<const DocumentTab> tabs,
                  int*  selected,
                  int*  closed_out = nullptr);

// Toolbar — a thin horizontal strip below the document tabs. Use
// IconButton inside, with `MutedSeparator` for groups.
void BeginToolbar(float height = tokens::sizes::toolbar_h);
void EndToolbar();

// Splash screen — full-window overlay drawn before any other content.
// Renders the procedural hero (gradient + grid), centred wordmark,
// recents list, and primary actions. Stops drawing once the user picks
// a recent / clicks an action; the host clears the splash flag.
struct SplashRecent {
    std::string_view title;
    std::string_view path;        // shown muted
    std::string_view modified;    // "12 minutes ago"
};
struct SplashAction {
    std::string_view label;       // "New project"
    std::string_view shortcut;    // "Ctrl+N"
};
struct SplashChoice {
    enum class Kind { None, Recent, Action };
    Kind kind = Kind::None;
    int  index = -1;
};
SplashChoice Splash(std::string_view              wordmark,
                    std::string_view              tagline,
                    std::span<const SplashRecent> recents,
                    std::span<const SplashAction> actions,
                    ImTextureID                   hero_tex = 0,
                    ImVec2                        hero_size = ImVec2(0, 0),
                    ImFont*                       hero_font = nullptr);

// ── File / folder dialog ────────────────────────────────────────
//
// A modal Open / Save dialog with breadcrumb + filter + sorted listing
// + filename input + action row. The dialog respects the host's modal
// stack — call `OpenFileDialog(id)` first, then `BeginFileDialog(id, ...)`
// in the next frame to drive it.
//
// State is owned by the caller. The `cwd` and `selection` strings are
// filled in / read by the dialog; `result` reports what closed it.

enum class FileDialogMode { Open, Save };

struct FileDialogEntry {
    std::string_view name;
    std::string_view size;       // "12 KB", "4.2 MB", or "" for folders
    std::string_view modified;   // "2 minutes ago"
    bool             is_folder = false;
};

struct FileDialogState {
    char        cwd[512]      = "/home/user";
    char        filter[128]   = "";
    char        filename[256] = "";
    int         row_sel       = -1;
};

enum class FileDialogResult { None, Confirmed, Cancelled };

void OpenFileDialog(const char* popup_id);

// Drive a modal file dialog. Caller supplies the entry list (already
// sorted / filtered however they like). Returns Confirmed when the
// user clicks Open/Save, Cancelled when they cancel.
FileDialogResult BeginFileDialog(const char*                          popup_id,
                                 FileDialogMode                       mode,
                                 FileDialogState&                     state,
                                 std::span<const FileDialogEntry>     entries);

}  // namespace skald
