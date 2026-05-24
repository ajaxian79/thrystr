#include <thrystr/scalar_analysis.hpp>
#include <thrystr/render_io.hpp>

#include <GLFW/glfw3.h>
#if defined(THRYSTR_HAS_X11)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>
#undef None
#undef Success
#endif
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <skald/skald.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr float kToolboxWidth = 380.0f;
constexpr float kTopChromeHeight = 44.0f;
constexpr double kChromeResizeMargin = 8.0;
constexpr int kMinWindowWidth = 760;
constexpr int kMinWindowHeight = 460;
constexpr int kSplashWindowWidth = 960;
constexpr int kSplashWindowHeight = 560;
constexpr int kSplashMinWindowWidth = 720;
constexpr int kSplashMinWindowHeight = 460;
constexpr float kWindowControlButtonSize = 28.0f;
constexpr float kWindowControlRightMargin = 12.0f;
constexpr ImVec2 kSettingsDialogSize{420.0f, 320.0f};
constexpr std::size_t kLazyBlockMinBytes = 1u * 1024u * 1024u;
constexpr std::size_t kLazyBlockMaxBytes = 10u * 1024u * 1024u;
constexpr std::size_t kLazyCacheMaxBlocks = 6u;
constexpr int kDefaultLazyBlockMiB = 4;
constexpr double kDefaultPlaybackPointsPerSecond = 60.0;
constexpr std::array<double, 4> kPlaybackSpeedPresets = {12.0, 24.0, 30.0, 60.0};
constexpr float kPlayheadScrubHitPixels = 18.0f;
constexpr float kTickerCurrentScale = 1.5f;
constexpr float kPlaybackReadoutWidth = 320.0f;
constexpr float kPlaybackReadoutHeight = 36.0f;
constexpr std::size_t kTickerTargetLabelPixels = 58u;
constexpr std::size_t kWaveFitMaxSamples = 4096;
constexpr int kWaveFitWavelengthSteps = 144;
constexpr int kWaveFitPhaseSteps = 128;
constexpr std::size_t kMaxWaveWavelengthModifiers = 16;
constexpr int kWaveModifierWavelengthSteps = 41;
constexpr std::uintmax_t kLargeSourcePhaseFitDeferBytes = 128ull * 1024ull * 1024ull;
constexpr const char* kSplashHeroPath = THRYSTR_ASSET_DIR "/splash_hero.png";
constexpr const char* kFileDialogStableId = "###thrystr_file_dialog";
constexpr const char* kWaveSettingsExtension = ".thryw";
constexpr std::array<char, 8> kWaveSettingsMagic = {'T', 'H', 'R', 'Y', 'W', 'A', 'V', 'E'};
constexpr std::uint32_t kWaveSettingsVersion = 2;
constexpr std::uint32_t kMaxWaveSettingsStringBytes = 1u * 1024u * 1024u;
constexpr std::uint32_t kMaxWaveSettingsItems = 10000u;
constexpr ImU32 kTransparent = IM_COL32(0, 0, 0, 0);

const ImU32 kDataPointHi = skald::tokens::with_alpha(skald::tokens::ink::primary, 0.92f);
const ImU32 kDataPointMed = skald::tokens::with_alpha(skald::tokens::ink::primary, 0.86f);
const ImU32 kDataPointLo = skald::tokens::with_alpha(skald::tokens::ink::primary, 0.73f);
const ImU32 kDataLineColor = skald::tokens::with_alpha(skald::tokens::status::info, 0.92f);
const ImU32 kMaxDeltaFill =
    skald::tokens::with_alpha(skald::tokens::status::destructive, 40.0f / 255.0f);
const ImU32 kSelectionFill =
    skald::tokens::with_alpha(skald::tokens::status::info, 32.0f / 255.0f);
const ImU32 kSelectionEdge =
    skald::tokens::with_alpha(skald::tokens::status::info, 210.0f / 255.0f);
const ImU32 kSelectionHandle =
    skald::tokens::with_alpha(skald::tokens::status::info, 0.92f);
const std::array<ImU32, 6> kWaveColors = {
    skald::tokens::with_alpha(skald::tokens::accents::gold, 0.86f),
    skald::tokens::with_alpha(skald::tokens::accents::mint, 0.86f),
    skald::tokens::with_alpha(skald::tokens::status::destructive, 0.86f),
    skald::tokens::with_alpha(skald::tokens::accents::violet, 0.86f),
    skald::tokens::with_alpha(skald::tokens::accents::ember, 0.86f),
    skald::tokens::with_alpha(skald::tokens::accents::cyan, 0.86f),
};

enum class DialogPurpose {
    None,
    OpenSource,
    LoadWave,
    SaveWave,
};

enum class StartupAction {
    None,
    NewWorkspace,
    OpenWorkspace,
    LoadSource,
};

enum class EntityType {
    Data,
    Wave,
};

struct DataEntity {
    double spatial_period_nm = 1.0;
};

struct WaveEntity {
    double wavelength_nm = 64.0;
    double amplitude = 2.0;
    double amplitude_offset = -1.0;
    double phase_nm = 0.0;
    std::vector<std::pair<double, double>> wavelength_modifiers;
};

struct Entity {
    int id = 0;
    EntityType type = EntityType::Wave;
    std::string name;
    bool visible = true;
    DataEntity data;
    WaveEntity wave;
};

struct Segment {
    bool active = false;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
};

struct WaveFitResult {
    double wavelength_nm = 64.0;
    double phase_nm = 0.0;
    std::size_t hits = 0;
    std::size_t tested = 0;
    double mean_error = 0.0;
};

struct LazyBlock {
    std::size_t index = 0;
    std::size_t offset = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mapped_bytes;
    std::uint64_t last_used_frame = 0;
};

struct Args {
    std::string file;
    std::string screenshot;
    int width = 1600;
    int height = 900;
    int frames = 0;
    bool size_overridden = false;
};

struct WindowGeometry {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct FileBrowserEntry {
    std::string name;
    std::string size;
    std::string modified;
    bool is_folder = false;
};

enum class ChromeAction {
    None,
    Move,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
};

enum class WindowControl {
    Minimize,
    Maximize,
    Close,
};

struct ChromeCursors {
    GLFWcursor* hresize = nullptr;
    GLFWcursor* vresize = nullptr;
    GLFWcursor* nwse = nullptr;
    GLFWcursor* nesw = nullptr;
};

struct AppState {
    char path[4096] = {};
    char wave_path[4096] = {};
    char entity_name[128] = {};
    std::optional<thrystr::Analysis> analysis;
    std::string status = "Empty workspace";
    std::vector<thrystr::ValueMapper> mappers;
    std::vector<Entity> entities;
    Segment segment;
    int next_entity_id = 1;
    int selected_entity_id = 0;
    int selection_drag_handle = 0;
    int entity_name_id = 0;
    int wave_serial = 1;
    bool workspace_open = false;
    bool splash_open = true;
    bool show_settings = false;
    bool show_inspector_overlay = false;
    bool request_close = false;
    ChromeAction chrome_action = ChromeAction::None;
    double chrome_start_global_x = 0.0;
    double chrome_start_global_y = 0.0;
    int chrome_start_window_x = 0;
    int chrome_start_window_y = 0;
    int chrome_start_window_w = 0;
    int chrome_start_window_h = 0;
    bool show_points = true;
    bool show_lines = true;
    bool wheel_scroll_mode = true;
    bool segment_selection_mode = false;
    float zoom_x = 1.0f;
    float zoom_y = 1.0f;
    float timeline_scroll_x = 0.0f;
    float max_slope = thrystr::kDefaultMaxSlope;
    float wave_scale = static_cast<float>(thrystr::kDefaultWaveScale);
    float wave_tolerance = static_cast<float>(thrystr::kDefaultWaveTolerance);
    int phase_steps = 720;
    int phase_test_points = 65536;
    int lazy_block_mib = kDefaultLazyBlockMiB;
    std::vector<LazyBlock> lazy_blocks;
    std::uint64_t lazy_frame = 0;
    std::size_t playhead_index = 0;
    double playhead_fraction = 0.0;
    double playback_points_per_second = kDefaultPlaybackPointsPerSecond;
    bool custom_playback_speed = false;
    char custom_playback_speed_text[32] = "60";
    bool xline_playing = false;
    bool playhead_dragging = false;
    std::optional<std::size_t> pending_scroll_index;
    skald::FileDialogState file_dialog{};
    std::vector<FileBrowserEntry> file_dialog_rows;
    std::vector<skald::FileDialogEntry> file_dialog_entries;
    int file_dialog_last_row = -1;
    bool file_dialog_dirty = true;
    DialogPurpose pending_dialog = DialogPurpose::None;
    DialogPurpose active_dialog = DialogPurpose::None;
    skald::Fonts fonts{};
    GLuint splash_hero_texture = 0;
    ImVec2 splash_hero_size{};
};

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto next_string = [&](std::string& dst, const char* flag) {
            if (arg == flag && i + 1 < argc) {
                dst = argv[++i];
            }
        };
        const auto next_int = [&](int& dst, const char* flag) {
            if (arg == flag && i + 1 < argc) {
                dst = std::max(0, std::atoi(argv[++i]));
                return true;
            }
            return false;
        };

        next_string(args.file, "--file");
        next_string(args.screenshot, "--screenshot");
        args.size_overridden |= next_int(args.width, "--width");
        args.size_overridden |= next_int(args.height, "--height");
        next_int(args.frames, "--frames");
    }
    if (!args.screenshot.empty() && args.frames == 0) {
        args.frames = 90;
    }
    return args;
}

void glfw_error(int code, const char* message) {
    std::fprintf(stderr, "glfw error %d: %s\n", code, message);
}

void prefer_x11_when_available() {
#if defined(__linux__) && defined(GLFW_PLATFORM_X11)
    if (std::getenv("DISPLAY") != nullptr) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
#endif
}

void force_undecorated_window(GLFWwindow* window) {
    if (!window) {
        return;
    }
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
#if defined(THRYSTR_HAS_X11)
    if (glfwGetPlatform() != GLFW_PLATFORM_X11) {
        return;
    }

    Display* display = glfwGetX11Display();
    const Window xwindow = glfwGetX11Window(window);
    if (!display || xwindow == 0) {
        return;
    }

    struct MotifWmHints {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long input_mode;
        unsigned long status;
    };

    constexpr unsigned long kDecorationsFlag = 1UL << 1;
    MotifWmHints hints{};
    hints.flags = kDecorationsFlag;
    hints.decorations = 0;
    const Atom property = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    XChangeProperty(display,
                    xwindow,
                    property,
                    property,
                    32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&hints),
                    5);
    XFlush(display);
#endif
}

void center_window_on_primary_monitor(GLFWwindow* window) {
    if (!window) {
        return;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        return;
    }

    int work_x = 0;
    int work_y = 0;
    int work_w = 0;
    int work_h = 0;
    glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_w, &work_h);

    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwSetWindowPos(window,
                     work_x + std::max(0, (work_w - window_w) / 2),
                     work_y + std::max(0, (work_h - window_h) / 2));
}

std::optional<WindowGeometry> default_workspace_geometry() {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) {
        return std::nullopt;
    }

    int work_x = 0;
    int work_y = 0;
    int work_w = 0;
    int work_h = 0;
    glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_w, &work_h);
    if (work_w <= 0 || work_h <= 0) {
        return std::nullopt;
    }

    const int left_margin = std::min(50, static_cast<int>(std::floor(work_w * 0.05)));
    const int right_margin = std::min(50, static_cast<int>(std::floor(work_w * 0.05)));
    const int top_margin = std::min(50, static_cast<int>(std::floor(work_h * 0.05)));
    const int bottom_margin = std::min(150, static_cast<int>(std::floor(work_h * 0.15)));

    WindowGeometry geometry;
    geometry.x = work_x + left_margin;
    geometry.y = work_y + top_margin;
    geometry.width = std::max(kMinWindowWidth, work_w - left_margin - right_margin);
    geometry.height = std::max(kMinWindowHeight, work_h - top_margin - bottom_margin);
    return geometry;
}

void apply_window_geometry(GLFWwindow* window, const WindowGeometry& geometry) {
    if (!window) {
        return;
    }
    glfwSetWindowPos(window, geometry.x, geometry.y);
    glfwSetWindowSize(window, geometry.width, geometry.height);
}

GLFWwindow* create_undecorated_window(int width,
                                      int height,
                                      const char* title,
                                      int min_width,
                                      int min_height,
                                      bool resizable,
                                      bool visible) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        return nullptr;
    }

    force_undecorated_window(window);
    glfwSetWindowSizeLimits(window,
                            resizable ? min_width : width,
                            resizable ? min_height : height,
                            resizable ? GLFW_DONT_CARE : width,
                            resizable ? GLFW_DONT_CARE : height);
    center_window_on_primary_monitor(window);
    return window;
}

ChromeCursors create_chrome_cursors() {
    ChromeCursors cursors;
    cursors.hresize = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    cursors.vresize = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    cursors.nwse = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    cursors.nesw = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    return cursors;
}

void destroy_chrome_cursors(ChromeCursors& cursors) {
    if (cursors.hresize) {
        glfwDestroyCursor(cursors.hresize);
        cursors.hresize = nullptr;
    }
    if (cursors.vresize) {
        glfwDestroyCursor(cursors.vresize);
        cursors.vresize = nullptr;
    }
    if (cursors.nwse) {
        glfwDestroyCursor(cursors.nwse);
        cursors.nwse = nullptr;
    }
    if (cursors.nesw) {
        glfwDestroyCursor(cursors.nesw);
        cursors.nesw = nullptr;
    }
}

bool chrome_action_is_resize(ChromeAction action) {
    return action != ChromeAction::None && action != ChromeAction::Move;
}

bool chrome_action_has_left(ChromeAction action) {
    return action == ChromeAction::ResizeLeft ||
           action == ChromeAction::ResizeTopLeft ||
           action == ChromeAction::ResizeBottomLeft;
}

bool chrome_action_has_right(ChromeAction action) {
    return action == ChromeAction::ResizeRight ||
           action == ChromeAction::ResizeTopRight ||
           action == ChromeAction::ResizeBottomRight;
}

bool chrome_action_has_top(ChromeAction action) {
    return action == ChromeAction::ResizeTop ||
           action == ChromeAction::ResizeTopLeft ||
           action == ChromeAction::ResizeTopRight;
}

bool chrome_action_has_bottom(ChromeAction action) {
    return action == ChromeAction::ResizeBottom ||
           action == ChromeAction::ResizeBottomLeft ||
           action == ChromeAction::ResizeBottomRight;
}

ChromeAction resize_action_at(double cursor_x,
                              double cursor_y,
                              int window_w,
                              int window_h) {
    const bool left = cursor_x >= 0.0 && cursor_x <= kChromeResizeMargin;
    const bool right = cursor_x <= static_cast<double>(window_w) &&
                       cursor_x >= static_cast<double>(window_w) - kChromeResizeMargin;
    const bool top = cursor_y >= 0.0 && cursor_y <= kChromeResizeMargin;
    const bool bottom = cursor_y <= static_cast<double>(window_h) &&
                        cursor_y >= static_cast<double>(window_h) - kChromeResizeMargin;

    if (top && left) {
        return ChromeAction::ResizeTopLeft;
    }
    if (top && right) {
        return ChromeAction::ResizeTopRight;
    }
    if (bottom && left) {
        return ChromeAction::ResizeBottomLeft;
    }
    if (bottom && right) {
        return ChromeAction::ResizeBottomRight;
    }
    if (top) {
        return ChromeAction::ResizeTop;
    }
    if (bottom) {
        return ChromeAction::ResizeBottom;
    }
    if (left) {
        return ChromeAction::ResizeLeft;
    }
    if (right) {
        return ChromeAction::ResizeRight;
    }
    return ChromeAction::None;
}

GLFWcursor* cursor_for_action(const ChromeCursors& cursors, ChromeAction action) {
    switch (action) {
    case ChromeAction::ResizeLeft:
    case ChromeAction::ResizeRight:
        return cursors.hresize;
    case ChromeAction::ResizeTop:
    case ChromeAction::ResizeBottom:
        return cursors.vresize;
    case ChromeAction::ResizeTopLeft:
    case ChromeAction::ResizeBottomRight:
        return cursors.nwse;
    case ChromeAction::ResizeTopRight:
    case ChromeAction::ResizeBottomLeft:
        return cursors.nesw;
    case ChromeAction::Move:
    case ChromeAction::None:
        return nullptr;
    }
    return nullptr;
}

std::pair<double, double> global_cursor_position(GLFWwindow* window,
                                                 double cursor_x,
                                                 double cursor_y) {
#if defined(THRYSTR_HAS_X11)
    if (window && glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display* display = glfwGetX11Display();
        const Window xwindow = glfwGetX11Window(window);
        if (display && xwindow != 0) {
            Window root = 0;
            Window child = 0;
            int root_x = 0;
            int root_y = 0;
            int window_x = 0;
            int window_y = 0;
            unsigned int mask = 0;
            if (XQueryPointer(display,
                              xwindow,
                              &root,
                              &child,
                              &root_x,
                              &root_y,
                              &window_x,
                              &window_y,
                              &mask)) {
                return {static_cast<double>(root_x), static_cast<double>(root_y)};
            }
        }
    }
#endif
    int window_x = 0;
    int window_y = 0;
    glfwGetWindowPos(window, &window_x, &window_y);
    return {
        static_cast<double>(window_x) + cursor_x,
        static_cast<double>(window_y) + cursor_y,
    };
}

void begin_chrome_action(AppState& state,
                         GLFWwindow* window,
                         ChromeAction action,
                         double cursor_x,
                         double cursor_y) {
    state.chrome_action = action;
    const auto [global_x, global_y] = global_cursor_position(window, cursor_x, cursor_y);
    state.chrome_start_global_x = global_x;
    state.chrome_start_global_y = global_y;
    glfwGetWindowPos(window, &state.chrome_start_window_x, &state.chrome_start_window_y);
    glfwGetWindowSize(window, &state.chrome_start_window_w, &state.chrome_start_window_h);
}

void update_chrome_action(AppState& state,
                          GLFWwindow* window,
                          double cursor_x,
                          double cursor_y) {
    const auto [global_x, global_y] = global_cursor_position(window, cursor_x, cursor_y);
    const int dx = static_cast<int>(std::lround(global_x - state.chrome_start_global_x));
    const int dy = static_cast<int>(std::lround(global_y - state.chrome_start_global_y));

    if (state.chrome_action == ChromeAction::Move) {
#if defined(THRYSTR_HAS_X11)
        if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
            Display* display = glfwGetX11Display();
            const Window xwindow = glfwGetX11Window(window);
            if (display && xwindow != 0) {
                XMoveWindow(display,
                            xwindow,
                            state.chrome_start_window_x + dx,
                            state.chrome_start_window_y + dy);
                XFlush(display);
                return;
            }
        }
#endif
        glfwSetWindowPos(window,
                         state.chrome_start_window_x + dx,
                         state.chrome_start_window_y + dy);
        return;
    }

    int x = state.chrome_start_window_x;
    int y = state.chrome_start_window_y;
    int w = state.chrome_start_window_w;
    int h = state.chrome_start_window_h;

    if (chrome_action_has_left(state.chrome_action)) {
        w = state.chrome_start_window_w - dx;
        if (w < kMinWindowWidth) {
            w = kMinWindowWidth;
        }
    } else if (chrome_action_has_right(state.chrome_action)) {
        w = std::max(kMinWindowWidth, state.chrome_start_window_w + dx);
    }

    if (chrome_action_has_top(state.chrome_action)) {
        h = state.chrome_start_window_h - dy;
        if (h < kMinWindowHeight) {
            h = kMinWindowHeight;
        }
    } else if (chrome_action_has_bottom(state.chrome_action)) {
        h = std::max(kMinWindowHeight, state.chrome_start_window_h + dy);
    }

#if defined(THRYSTR_HAS_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        Display* display = glfwGetX11Display();
        const Window xwindow = glfwGetX11Window(window);
        if (display && xwindow != 0) {
            XResizeWindow(display,
                          xwindow,
                          static_cast<unsigned int>(w),
                          static_cast<unsigned int>(h));
            XFlush(display);
            return;
        }
    }
#endif

    glfwSetWindowSize(window, w, h);
    (void)x;
    (void)y;
}

void handle_custom_chrome(AppState& state,
                          GLFWwindow* window,
                          const ChromeCursors& cursors,
                          bool allow_resize = true) {
    if (!window || glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
        return;
    }

    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window, &cursor_x, &cursor_y);

    int window_w = 0;
    int window_h = 0;
    glfwGetWindowSize(window, &window_w, &window_h);

    if (state.chrome_action != ChromeAction::None) {
        glfwSetCursor(window, cursor_for_action(cursors, state.chrome_action));
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            update_chrome_action(state, window, cursor_x, cursor_y);
        } else {
            state.chrome_action = ChromeAction::None;
            glfwSetCursor(window, nullptr);
        }
        return;
    }

    if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
        glfwSetCursor(window, nullptr);
        return;
    }

    const ChromeAction resize_action = allow_resize
        ? resize_action_at(cursor_x, cursor_y, window_w, window_h)
        : ChromeAction::None;
    const bool resize_hovered = chrome_action_is_resize(resize_action);
    const bool titlebar_hovered =
        cursor_y >= 0.0 && cursor_y <= static_cast<double>(kTopChromeHeight);
    const bool item_hovered = ImGui::IsAnyItemHovered();
    const ChromeAction hover_action =
        resize_hovered ? resize_action
                       : (titlebar_hovered && !item_hovered ? ChromeAction::Move
                                                            : ChromeAction::None);

    glfwSetCursor(window, cursor_for_action(cursors, hover_action));

    if (hover_action != ChromeAction::None &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        begin_chrome_action(state, window, hover_action, cursor_x, cursor_y);
    }
}

std::string format_bytes(std::uintmax_t bytes) {
    static constexpr std::array<const char*, 5> units = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }

    char buffer[64] = {};
    if (unit == 0) {
        std::snprintf(buffer, sizeof(buffer), "%ju %s",
                      static_cast<std::uintmax_t>(bytes), units[unit]);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unit]);
    }
    return buffer;
}

std::string format_count(std::size_t count) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%zu", count);
    return buffer;
}

template <std::size_t N>
void copy_to_buffer(char (&buffer)[N], const std::string& value) {
    std::snprintf(buffer, N, "%s", value.c_str());
}

std::filesystem::path home_or_current_path() {
    if (const char* home = std::getenv("HOME")) {
        return home;
    }
    std::error_code error;
    const std::filesystem::path cwd = std::filesystem::current_path(error);
    return error ? std::filesystem::path("/") : cwd;
}

void initialize_file_dialog(AppState& state) {
    copy_to_buffer(state.file_dialog.cwd, home_or_current_path().string());
    state.file_dialog.filename[0] = '\0';
    state.file_dialog.filter[0] = '\0';
    state.file_dialog.row_sel = -1;
}

std::string entry_size_text(const std::filesystem::directory_entry& entry) {
    std::error_code error;
    if (entry.is_directory(error)) {
        return {};
    }
    const auto size = entry.file_size(error);
    return error ? std::string{} : format_bytes(size);
}

bool entry_matches_filter(const std::string& name, const char* filter) {
    if (filter == nullptr || filter[0] == '\0') {
        return true;
    }
    return name.find(filter) != std::string::npos;
}

void refresh_file_dialog_entries(AppState& state) {
    state.file_dialog_rows.clear();
    state.file_dialog_entries.clear();

    std::filesystem::path cwd(state.file_dialog.cwd);
    std::error_code error;
    if (!std::filesystem::exists(cwd, error) || !std::filesystem::is_directory(cwd, error)) {
        cwd = home_or_current_path();
        copy_to_buffer(state.file_dialog.cwd, cwd.string());
    }

    if (cwd.has_parent_path()) {
        state.file_dialog_rows.push_back(FileBrowserEntry{"..", "", "", true});
    }

    std::vector<FileBrowserEntry> rows;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(cwd, error)) {
        if (error) {
            break;
        }

        const std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.') {
            continue;
        }
        if (!entry_matches_filter(name, state.file_dialog.filter)) {
            continue;
        }

        std::error_code type_error;
        const bool is_folder = entry.is_directory(type_error);
        rows.push_back(FileBrowserEntry{
            name,
            is_folder ? std::string{} : entry_size_text(entry),
            "",
            is_folder,
        });
    }

    std::sort(rows.begin(), rows.end(), [](const FileBrowserEntry& left,
                                           const FileBrowserEntry& right) {
        if (left.is_folder != right.is_folder) {
            return left.is_folder;
        }
        return left.name < right.name;
    });

    state.file_dialog_rows.insert(state.file_dialog_rows.end(),
                                  rows.begin(), rows.end());
    state.file_dialog_entries.reserve(state.file_dialog_rows.size());
    for (const FileBrowserEntry& row : state.file_dialog_rows) {
        state.file_dialog_entries.push_back(skald::FileDialogEntry{
            row.name,
            row.size,
            row.modified,
            row.is_folder,
        });
    }
}

std::size_t lazy_block_bytes(const AppState& state) {
    const int mib = std::clamp(state.lazy_block_mib, 1, 10);
    return std::clamp(static_cast<std::size_t>(mib) * 1024u * 1024u,
                      kLazyBlockMinBytes,
                      kLazyBlockMaxBytes);
}

std::size_t source_sample_count(const AppState& state) {
    if (!state.analysis) {
        return 0u;
    }
    if (state.analysis->source_size > 0) {
        return static_cast<std::size_t>(std::min<std::uintmax_t>(
            state.analysis->source_size,
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())));
    }
    return state.analysis->scalars.size();
}

void clear_lazy_cache(AppState& state) {
    state.lazy_blocks.clear();
    state.lazy_frame = 0;
}

std::vector<std::uint8_t> read_file_slice(const std::filesystem::path& path,
                                          std::size_t offset,
                                          std::size_t length) {
    std::vector<std::uint8_t> bytes(length);
    if (length == 0u) {
        return bytes;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input) {
        throw std::runtime_error("could not seek file: " + path.string());
    }
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const std::streamsize read_count = input.gcount();
    if (read_count < 0) {
        throw std::runtime_error("could not read file: " + path.string());
    }
    bytes.resize(static_cast<std::size_t>(read_count));
    return bytes;
}

LazyBlock* find_lazy_block(AppState& state, std::size_t block_index) {
    for (LazyBlock& block : state.lazy_blocks) {
        if (block.index == block_index) {
            block.last_used_frame = state.lazy_frame;
            return &block;
        }
    }
    return nullptr;
}

const LazyBlock* find_lazy_block(const AppState& state, std::size_t block_index) {
    for (const LazyBlock& block : state.lazy_blocks) {
        if (block.index == block_index) {
            return &block;
        }
    }
    return nullptr;
}

void trim_lazy_cache(AppState& state) {
    while (state.lazy_blocks.size() > kLazyCacheMaxBlocks) {
        const auto oldest = std::min_element(
            state.lazy_blocks.begin(),
            state.lazy_blocks.end(),
            [](const LazyBlock& left, const LazyBlock& right) {
                return left.last_used_frame < right.last_used_frame;
            });
        if (oldest == state.lazy_blocks.end()) {
            return;
        }
        state.lazy_blocks.erase(oldest);
    }
}

void load_lazy_block(AppState& state, std::size_t block_index) {
    if (!state.analysis || find_lazy_block(state, block_index)) {
        return;
    }

    const std::size_t count = source_sample_count(state);
    const std::size_t block_size = lazy_block_bytes(state);
    const std::size_t offset = block_index * block_size;
    if (offset >= count) {
        return;
    }
    const std::size_t length = std::min(block_size, count - offset);

    LazyBlock block;
    block.index = block_index;
    block.offset = offset;
    block.bytes = read_file_slice(state.analysis->source_path, offset, length);
    block.mapped_bytes = thrystr::map_bytes_to_wrapped(block.bytes, state.mappers);
    block.last_used_frame = state.lazy_frame;
    state.lazy_blocks.push_back(std::move(block));
    trim_lazy_cache(state);
}

void seed_lazy_cache_from_analysis(AppState& state) {
    clear_lazy_cache(state);
    if (!state.analysis || state.analysis->bytes.empty()) {
        return;
    }

    const std::size_t block_size = lazy_block_bytes(state);
    LazyBlock block;
    block.index = state.analysis->window.offset / block_size;
    block.offset = state.analysis->window.offset;
    block.bytes = state.analysis->bytes;
    block.mapped_bytes = state.analysis->mapped_bytes;
    block.last_used_frame = ++state.lazy_frame;
    state.lazy_blocks.push_back(std::move(block));
}

void ensure_lazy_blocks(AppState& state, std::size_t first, std::size_t last) {
    if (!state.analysis) {
        return;
    }
    const std::size_t count = source_sample_count(state);
    if (count == 0u) {
        return;
    }

    first = std::min(first, count - 1u);
    last = std::min(last, count - 1u);
    if (last < first) {
        std::swap(first, last);
    }

    ++state.lazy_frame;
    const std::size_t block_size = lazy_block_bytes(state);
    const std::size_t first_block = first / block_size;
    const std::size_t last_block = last / block_size;
    const std::size_t prefetch_first = first_block == 0u ? 0u : first_block - 1u;
    const std::size_t prefetch_last =
        std::min((count - 1u) / block_size, last_block + 1u);
    for (std::size_t block = prefetch_first; block <= prefetch_last; ++block) {
        load_lazy_block(state, block);
    }
}

std::optional<std::uint8_t> raw_byte_at(const AppState& state, std::size_t index) {
    if (!state.analysis || index >= source_sample_count(state)) {
        return std::nullopt;
    }

    const std::size_t block_size = lazy_block_bytes(state);
    const std::size_t block_index = index / block_size;
    if (const LazyBlock* block = find_lazy_block(state, block_index)) {
        if (index >= block->offset) {
            const std::size_t local = index - block->offset;
            if (local < block->bytes.size()) {
                return block->bytes[local];
            }
        }
    }

    if (index >= state.analysis->window.offset) {
        const std::size_t local = index - state.analysis->window.offset;
        if (local < state.analysis->bytes.size()) {
            return state.analysis->bytes[local];
        }
    }
    return std::nullopt;
}

std::optional<float> scalar_at(const AppState& state, std::size_t index) {
    if (!state.analysis || index >= source_sample_count(state)) {
        return std::nullopt;
    }

    const std::size_t block_size = lazy_block_bytes(state);
    const std::size_t block_index = index / block_size;
    if (const LazyBlock* block = find_lazy_block(state, block_index)) {
        if (index >= block->offset) {
            const std::size_t local = index - block->offset;
            if (local < block->mapped_bytes.size()) {
                return thrystr::byte_to_scalar(block->mapped_bytes[local]);
            }
        }
    }

    if (index >= state.analysis->window.offset) {
        const std::size_t local = index - state.analysis->window.offset;
        if (local < state.analysis->scalars.size()) {
            return state.analysis->scalars[local];
        }
    }
    return std::nullopt;
}

std::uint8_t byte_from_scalar(float scalar) {
    const double scaled = (static_cast<double>(scalar) + 1.0) * 128.0;
    return static_cast<std::uint8_t>(
        std::clamp(std::llround(scaled), 0ll, 255ll));
}

std::string hex_byte_text(std::uint8_t byte) {
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02X", static_cast<unsigned>(byte));
    return buffer;
}

template <typename T>
void write_binary(std::ostream& output, const T& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!output) {
        throw std::runtime_error("could not write wave settings");
    }
}

template <typename T>
T read_binary(std::istream& input) {
    T value{};
    input.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!input) {
        throw std::runtime_error("could not read wave settings");
    }
    return value;
}

void write_string(std::ostream& output, const std::string& value) {
    const auto length = static_cast<std::uint32_t>(value.size());
    write_binary(output, length);
    output.write(value.data(), static_cast<std::streamsize>(length));
    if (!output) {
        throw std::runtime_error("could not write wave settings string");
    }
}

std::string read_string(std::istream& input) {
    const std::uint32_t length = read_binary<std::uint32_t>(input);
    if (length > kMaxWaveSettingsStringBytes) {
        throw std::runtime_error("wave settings string too large");
    }
    std::string value(length, '\0');
    if (length > 0) {
        input.read(value.data(), static_cast<std::streamsize>(length));
    }
    if (!input) {
        throw std::runtime_error("could not read wave settings string");
    }
    return value;
}

std::filesystem::path with_wave_settings_extension(std::filesystem::path path) {
    if (path.extension().empty()) {
        path += kWaveSettingsExtension;
    }
    return path;
}

std::size_t nice_tick(double raw) {
    if (raw <= 1.0) {
        return 1;
    }
    const double exponent = std::pow(10.0, std::floor(std::log10(raw)));
    const double scaled = raw / exponent;
    double nice = 10.0;
    if (scaled <= 1.0) {
        nice = 1.0;
    } else if (scaled <= 2.0) {
        nice = 2.0;
    } else if (scaled <= 5.0) {
        nice = 5.0;
    }
    return static_cast<std::size_t>(nice * exponent);
}

const char* entity_type_name(EntityType type) {
    return type == EntityType::Data ? "data" : "wave";
}

Entity* find_entity(AppState& state, int id) {
    for (Entity& entity : state.entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

Entity* data_entity(AppState& state) {
    for (Entity& entity : state.entities) {
        if (entity.type == EntityType::Data) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity* data_entity(const AppState& state) {
    for (const Entity& entity : state.entities) {
        if (entity.type == EntityType::Data) {
            return &entity;
        }
    }
    return nullptr;
}

double data_spatial_period_nm(const AppState& state) {
    const Entity* data = data_entity(state);
    return data ? std::max(0.000001, data->data.spatial_period_nm) : 1.0;
}

std::size_t scalar_count(const AppState& state) {
    return source_sample_count(state);
}

std::size_t clamp_index(double value, std::size_t count) {
    if (count == 0u) {
        return 0u;
    }
    const double clamped = std::clamp(value, 0.0, static_cast<double>(count - 1u));
    return static_cast<std::size_t>(std::llround(clamped));
}

void clamp_segment(AppState& state) {
    const std::size_t count = scalar_count(state);
    if (count == 0u) {
        state.segment.selection_start = 0u;
        state.segment.selection_end = 0u;
        state.segment.active = false;
        state.selection_drag_handle = 0;
        return;
    }
    state.segment.selection_start =
        std::min(state.segment.selection_start, count - 1u);
    state.segment.selection_end =
        std::min(state.segment.selection_end, count - 1u);
}

std::pair<std::size_t, std::size_t> normalized_selection(const AppState& state) {
    return {
        std::min(state.segment.selection_start, state.segment.selection_end),
        std::max(state.segment.selection_start, state.segment.selection_end),
    };
}

std::pair<std::size_t, std::size_t> analysis_selection_bounds(const AppState& state) {
    const std::size_t count = source_sample_count(state);
    if (!state.analysis || count == 0u) {
        return {0u, 0u};
    }
    if (!state.segment.active) {
        return {0u, count - 1u};
    }
    const auto [first, last] = normalized_selection(state);
    return {
        std::min(first, count - 1u),
        std::min(last, count - 1u),
    };
}

std::size_t fit_sample_stride(std::size_t first, std::size_t last) {
    const std::size_t count = last >= first ? last - first + 1u : 0u;
    return std::max<std::size_t>(1u, count / kWaveFitMaxSamples);
}

void sync_entity_name(AppState& state) {
    if (state.entity_name_id == state.selected_entity_id) {
        return;
    }
    state.entity_name_id = state.selected_entity_id;
    const Entity* selected = find_entity(state, state.selected_entity_id);
    copy_to_buffer(state.entity_name, selected ? selected->name : std::string{});
}

void select_entity(AppState& state, int id) {
    state.selected_entity_id = id;
    state.entity_name_id = 0;
    sync_entity_name(state);
}

void open_empty_workspace(AppState& state) {
    state.workspace_open = true;
    state.splash_open = false;
    state.analysis.reset();
    state.entities.clear();
    state.segment = {};
    state.path[0] = '\0';
    state.selected_entity_id = 0;
    state.entity_name_id = 0;
    state.wheel_scroll_mode = true;
    state.segment_selection_mode = false;
    state.timeline_scroll_x = 0.0f;
    state.xline_playing = false;
    state.playhead_dragging = false;
    state.playhead_index = 0;
    state.playhead_fraction = 0.0;
    state.pending_scroll_index.reset();
    state.status = "Empty workspace";
}

void ensure_workspace(AppState& state) {
    if (!state.workspace_open) {
        state.workspace_open = true;
        state.splash_open = false;
    }
}

Entity& ensure_data_entity(AppState& state) {
    ensure_workspace(state);
    if (Entity* existing = data_entity(state)) {
        return *existing;
    }

    Entity entity;
    entity.id = state.next_entity_id++;
    entity.type = EntityType::Data;
    entity.name = state.path[0] == '\0'
        ? std::string("data")
        : std::filesystem::path(state.path).filename().string();
    entity.visible = true;
    state.entities.insert(state.entities.begin(), entity);
    return state.entities.front();
}

Entity& create_wave_entity(AppState& state, std::string name = {}) {
    ensure_workspace(state);
    Entity entity;
    entity.id = state.next_entity_id++;
    entity.type = EntityType::Wave;
    entity.name = name.empty() ? "wave " + std::to_string(state.wave_serial++) : std::move(name);
    entity.visible = true;
    entity.wave = {};
    state.entities.push_back(entity);
    Entity& created = state.entities.back();
    select_entity(state, created.id);
    state.status = "Created " + created.name;
    return created;
}

void create_section(AppState& state) {
    const std::size_t count = source_sample_count(state);
    if (!state.analysis || count == 0u) {
        state.status = "Load source data before creating a section";
        return;
    }
    state.segment.active = true;
    state.segment.selection_start = 0;
    state.segment.selection_end = count - 1u;
    state.selection_drag_handle = 0;
    state.status = "Created x-line section";
}

double wave_wavelength_at_nm(const WaveEntity& wave, double x_nm) {
    double wavelength = std::max(0.000001, wave.wavelength_nm);
    for (const auto& modifier : wave.wavelength_modifiers) {
        if (x_nm >= modifier.first) {
            wavelength = std::max(0.000001, wavelength + modifier.second);
        }
    }
    return wavelength;
}

double wave_value_at_nm(const WaveEntity& wave, double x_nm) {
    const double wavelength = wave_wavelength_at_nm(wave, x_nm);
    const double theta = 2.0 * std::numbers::pi * (x_nm - wave.phase_nm) / wavelength;
    return wave.amplitude_offset + wave.amplitude * ((std::sin(theta) + 1.0) * 0.5);
}

WaveFitResult score_wave_on_range(const AppState& state,
                                  const WaveEntity& wave,
                                  std::size_t first,
                                  std::size_t last) {
    WaveFitResult score;
    score.wavelength_nm = wave.wavelength_nm;
    score.phase_nm = wave.phase_nm;
    const std::size_t count = source_sample_count(state);
    if (!state.analysis || count == 0u || last < first) {
        return score;
    }

    const double period = data_spatial_period_nm(state);
    const double tolerance = std::max(0.000001, static_cast<double>(state.wave_tolerance));
    const std::size_t stride = fit_sample_stride(first, last);
    double error = 0.0;
    // Intersections only count at discrete data samples; drawn data lines are visual guides.
    for (std::size_t index = first; index <= last && index < count; index += stride) {
        const std::optional<float> scalar = scalar_at(state, index);
        if (!scalar) {
            continue;
        }
        const double x_nm = static_cast<double>(index) * period;
        const double sample_error =
            std::abs(wave_value_at_nm(wave, x_nm) - static_cast<double>(*scalar));
        if (sample_error <= tolerance) {
            ++score.hits;
        }
        error += sample_error;
        ++score.tested;
    }
    score.mean_error = score.tested == 0 ? 0.0 : error / static_cast<double>(score.tested);
    return score;
}

bool wave_fit_is_better(const WaveFitResult& candidate, const WaveFitResult& best) {
    if (candidate.hits != best.hits) {
        return candidate.hits > best.hits;
    }
    return candidate.mean_error < best.mean_error;
}

double log_lerp(double low, double high, double t) {
    low = std::max(0.000001, low);
    high = std::max(low, high);
    return std::exp(std::log(low) + (std::log(high) - std::log(low)) * t);
}

WaveFitResult fit_wave_to_selection(const AppState& state, const WaveEntity& base_wave) {
    if (!state.analysis || source_sample_count(state) < 2) {
        return {64.0, 0.0};
    }

    const auto [first, last] = analysis_selection_bounds(state);
    const double period = data_spatial_period_nm(state);
    const double span_nm = std::max(period, static_cast<double>(last - first + 1u) * period);
    const double base_wavelength = std::max(0.000001, base_wave.wavelength_nm);
    const double min_wavelength =
        std::max(0.000001, std::min({period, base_wavelength, span_nm}) / 64.0);
    const double max_wavelength =
        std::max({min_wavelength * 2.0, period * 256.0, base_wavelength * 64.0, span_nm * 2.0});

    WaveFitResult best;
    best.mean_error = DBL_MAX;

    for (int wi = 0; wi < kWaveFitWavelengthSteps; ++wi) {
        const double t = kWaveFitWavelengthSteps > 1
            ? static_cast<double>(wi) / static_cast<double>(kWaveFitWavelengthSteps - 1)
            : 0.0;
        const double wavelength = log_lerp(min_wavelength, max_wavelength, t);
        for (int pi = 0; pi < kWaveFitPhaseSteps; ++pi) {
            const double phase_nm = wavelength * static_cast<double>(pi) /
                                    static_cast<double>(kWaveFitPhaseSteps);
            WaveEntity candidate = base_wave;
            candidate.wavelength_nm = wavelength;
            candidate.phase_nm = phase_nm;
            candidate.wavelength_modifiers.clear();
            WaveFitResult score = score_wave_on_range(state, candidate, first, last);
            score.wavelength_nm = wavelength;
            score.phase_nm = phase_nm;
            if (wave_fit_is_better(score, best)) {
                best = score;
            }
        }
    }

    return best;
}

void rebuild_wavelength_modifiers(const AppState& state, WaveEntity& wave) {
    wave.wavelength_modifiers.clear();
    if (!state.analysis || source_sample_count(state) < 2) {
        return;
    }

    const auto [first, last] = analysis_selection_bounds(state);
    const double period = data_spatial_period_nm(state);
    const std::size_t count = last - first + 1u;
    const std::size_t segments =
        std::min<std::size_t>(kMaxWaveWavelengthModifiers, std::max<std::size_t>(1u, count));

    for (std::size_t segment = 0; segment < segments; ++segment) {
        const std::size_t segment_first = first + (count * segment) / segments;
        const std::size_t segment_last =
            first + (count * (segment + 1u)) / segments - 1u;
        if (segment_last < segment_first) {
            continue;
        }

        const double key_nm = static_cast<double>(segment_first) * period;
        const double current_wavelength = wave_wavelength_at_nm(wave, key_nm);
        WaveFitResult best = score_wave_on_range(state, wave, segment_first, segment_last);
        double best_target = current_wavelength;
        const double min_target = std::max(0.000001, current_wavelength * 0.5);
        const double max_target = std::max(min_target * 1.01, current_wavelength * 1.5);

        for (int step = 0; step < kWaveModifierWavelengthSteps; ++step) {
            const double t = kWaveModifierWavelengthSteps > 1
                ? static_cast<double>(step) / static_cast<double>(kWaveModifierWavelengthSteps - 1)
                : 0.0;
            const double target = log_lerp(min_target, max_target, t);
            WaveEntity candidate = wave;
            candidate.wavelength_modifiers.push_back({key_nm, target - current_wavelength});
            WaveFitResult score =
                score_wave_on_range(state, candidate, segment_first, segment_last);
            if (wave_fit_is_better(score, best)) {
                best = score;
                best_target = target;
            }
        }

        const double delta_nm = best_target - current_wavelength;
        if (std::abs(delta_nm) > std::max(0.000001, current_wavelength * 0.001)) {
            wave.wavelength_modifiers.push_back({key_nm, delta_nm});
        }
    }
}

void create_interpolated_wave(AppState& state) {
    Entity& wave = create_wave_entity(state, "interpolated " + std::to_string(state.wave_serial++));
    const WaveFitResult fit = fit_wave_to_selection(state, wave.wave);
    wave.wave.wavelength_nm = fit.wavelength_nm;
    wave.wave.phase_nm = fit.phase_nm;
    rebuild_wavelength_modifiers(state, wave.wave);
    const WaveFitResult final_fit = score_wave_on_range(
        state, wave.wave, analysis_selection_bounds(state).first,
        analysis_selection_bounds(state).second);
    state.status = "Created interpolated wave: " +
                   format_count(final_fit.hits) + "/" +
                   format_count(final_fit.tested) + " point hits, " +
                   format_count(wave.wave.wavelength_modifiers.size()) + " wavelength keys";
}

void load_path(AppState& state);

bool should_defer_phase_fit_on_load(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    return !error && file_size >= kLargeSourcePhaseFitDeferBytes;
}

thrystr::Analysis load_lazy_analysis_window(const std::filesystem::path& path,
                                            std::size_t block_bytes,
                                            float max_slope,
                                            double wave_scale,
                                            double wave_tolerance,
                                            int phase_steps,
                                            std::size_t max_phase_test_points,
                                            std::span<const thrystr::ValueMapper> mappers) {
    std::error_code error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("could not determine file size: " + path.string());
    }
    if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("file too large for this build: " + path.string());
    }

    thrystr::Analysis analysis;
    analysis.source_path = path;
    analysis.source_size = file_size;
    analysis.mappers.assign(mappers.begin(), mappers.end());
    if (file_size == 0u) {
        return analysis;
    }

    const std::size_t source_size = static_cast<std::size_t>(file_size);
    const std::size_t length = std::min(block_bytes, source_size);
    analysis.bytes = read_file_slice(path, 0u, length);
    analysis.mapped_bytes = thrystr::map_bytes_to_wrapped(analysis.bytes, mappers);
    analysis.scalars = thrystr::map_bytes_to_scalars(analysis.mapped_bytes);
    analysis.window = thrystr::find_highest_entropy_window(
        analysis.mapped_bytes,
        analysis.mapped_bytes.size());
    analysis.window.offset = 0u;
    analysis.window.length = analysis.bytes.size();
    analysis.window_count = source_size >= analysis.window.length
        ? source_size - analysis.window.length + 1u
        : 0u;
    analysis.max_delta_sample_index = analysis.window.delta_index;
    analysis.max_abs_scalar_delta = thrystr::max_abs_scalar_delta(analysis.scalars);
    analysis.x_scale = thrystr::compute_x_scale(analysis.scalars, max_slope);
    analysis.wave_scale = wave_scale;
    analysis.sine = thrystr::fit_wave_phase(analysis.scalars,
                                            false,
                                            wave_scale,
                                            wave_tolerance,
                                            phase_steps,
                                            max_phase_test_points);
    analysis.cosine = thrystr::fit_wave_phase(analysis.scalars,
                                              true,
                                              wave_scale,
                                              wave_tolerance,
                                              phase_steps,
                                              max_phase_test_points);
    return analysis;
}

void fit_wave_phases(AppState& state) {
    if (!state.analysis) {
        return;
    }

    try {
        thrystr::Analysis& analysis = *state.analysis;
        analysis.wave_scale = static_cast<double>(state.wave_scale);
        analysis.sine = thrystr::fit_wave_phase(
            analysis.scalars,
            false,
            analysis.wave_scale,
            static_cast<double>(state.wave_tolerance),
            state.phase_steps,
            static_cast<std::size_t>(state.phase_test_points));
        analysis.cosine = thrystr::fit_wave_phase(
            analysis.scalars,
            true,
            analysis.wave_scale,
            static_cast<double>(state.wave_tolerance),
            state.phase_steps,
            static_cast<std::size_t>(state.phase_test_points));
        state.status = "Fitted wave phases";
    } catch (const std::exception& error) {
        state.status = error.what();
    }
}

void save_wave_settings(AppState& state, const std::filesystem::path& path) {
    const std::filesystem::path output_path = with_wave_settings_extension(path);
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open wave settings for write: " +
                                 output_path.string());
    }

    output.write(kWaveSettingsMagic.data(),
                 static_cast<std::streamsize>(kWaveSettingsMagic.size()));
    write_binary(output, kWaveSettingsVersion);
    write_binary(output, state.max_slope);
    write_binary(output, state.wave_scale);
    write_binary(output, state.wave_tolerance);
    write_binary(output, state.phase_steps);
    write_binary(output, state.phase_test_points);
    write_binary(output, state.zoom_x);
    write_binary(output, state.zoom_y);

    const std::uint8_t show_points = state.show_points ? 1 : 0;
    const std::uint8_t show_lines = state.show_lines ? 1 : 0;
    constexpr std::uint8_t reserved_byte = 1;
    write_binary(output, show_points);
    write_binary(output, show_lines);
    write_binary(output, reserved_byte);
    write_binary(output, reserved_byte);

    const double sine_phase = state.analysis ? state.analysis->sine.phase_radians : 0.0;
    const double cosine_phase = state.analysis ? state.analysis->cosine.phase_radians : 0.0;
    write_binary(output, sine_phase);
    write_binary(output, cosine_phase);

    const auto mapper_count = static_cast<std::uint32_t>(state.mappers.size());
    write_binary(output, mapper_count);
    for (const thrystr::ValueMapper& mapper : state.mappers) {
        const auto kind = static_cast<std::uint32_t>(mapper.kind);
        const std::uint8_t enabled = mapper.enabled ? 1 : 0;
        write_binary(output, kind);
        write_binary(output, mapper.operand);
        write_binary(output, enabled);
    }

    const auto entity_count = static_cast<std::uint32_t>(state.entities.size());
    write_binary(output, entity_count);
    for (const Entity& entity : state.entities) {
        write_binary(output, static_cast<std::int32_t>(entity.id));
        write_binary(output, static_cast<std::uint32_t>(entity.type));
        write_binary(output, static_cast<std::uint8_t>(entity.visible ? 1 : 0));
        write_string(output, entity.name);
        write_binary(output, entity.data.spatial_period_nm);
        write_binary(output, entity.wave.wavelength_nm);
        write_binary(output, entity.wave.amplitude);
        write_binary(output, entity.wave.amplitude_offset);
        write_binary(output, entity.wave.phase_nm);
        const auto modifier_count =
            static_cast<std::uint32_t>(entity.wave.wavelength_modifiers.size());
        write_binary(output, modifier_count);
        for (const auto& modifier : entity.wave.wavelength_modifiers) {
            write_binary(output, modifier.first);
            write_binary(output, modifier.second);
        }
    }
    write_binary(output, static_cast<std::int32_t>(state.selected_entity_id));
    write_binary(output, static_cast<std::uint8_t>(state.segment.active ? 1 : 0));
    write_binary(output, static_cast<std::uint64_t>(state.segment.selection_start));
    write_binary(output, static_cast<std::uint64_t>(state.segment.selection_end));
    write_string(output, state.path);

    copy_to_buffer(state.wave_path, output_path.lexically_normal().string());
    state.status = "Saved wave settings: " + output_path.filename().string();
}

void load_wave_settings(AppState& state, const std::filesystem::path& path) {
    ensure_workspace(state);
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open wave settings: " + path.string());
    }

    std::array<char, 8> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!input || magic != kWaveSettingsMagic) {
        throw std::runtime_error("not a thrystr wave settings file");
    }

    const std::uint32_t version = read_binary<std::uint32_t>(input);
    if (version != 1 && version != kWaveSettingsVersion) {
        throw std::runtime_error("unsupported wave settings version");
    }

    state.max_slope = read_binary<float>(input);
    state.wave_scale = read_binary<float>(input);
    state.wave_tolerance = read_binary<float>(input);
    state.phase_steps = read_binary<int>(input);
    state.phase_test_points = read_binary<int>(input);
    state.zoom_x = read_binary<float>(input);
    state.zoom_y = read_binary<float>(input);
    state.show_points = read_binary<std::uint8_t>(input) != 0;
    state.show_lines = read_binary<std::uint8_t>(input) != 0;
    (void)read_binary<std::uint8_t>(input);
    (void)read_binary<std::uint8_t>(input);
    const double sine_phase = read_binary<double>(input);
    const double cosine_phase = read_binary<double>(input);

    const std::uint32_t mapper_count = read_binary<std::uint32_t>(input);
    if (mapper_count > kMaxWaveSettingsItems) {
        throw std::runtime_error("wave settings mapper count too large");
    }
    state.mappers.clear();
    state.mappers.reserve(mapper_count);
    for (std::uint32_t i = 0; i < mapper_count; ++i) {
        const auto kind = static_cast<thrystr::ValueMapperKind>(
            read_binary<std::uint32_t>(input));
        const double operand = read_binary<double>(input);
        const bool enabled = read_binary<std::uint8_t>(input) != 0;
        state.mappers.push_back({kind, operand, enabled});
    }

    if (version >= 2) {
        const std::uint32_t entity_count = read_binary<std::uint32_t>(input);
        if (entity_count > kMaxWaveSettingsItems) {
            throw std::runtime_error("wave settings entity count too large");
        }
        state.entities.clear();
        state.entities.reserve(entity_count);
        int max_entity_id = 0;
        int wave_count = 0;
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            Entity entity;
            entity.id = read_binary<std::int32_t>(input);
            entity.type = static_cast<EntityType>(read_binary<std::uint32_t>(input));
            entity.visible = read_binary<std::uint8_t>(input) != 0;
            entity.name = read_string(input);
            entity.data.spatial_period_nm = read_binary<double>(input);
            entity.wave.wavelength_nm = read_binary<double>(input);
            entity.wave.amplitude = read_binary<double>(input);
            entity.wave.amplitude_offset = read_binary<double>(input);
            entity.wave.phase_nm = read_binary<double>(input);
            const std::uint32_t modifier_count = read_binary<std::uint32_t>(input);
            if (modifier_count > kMaxWaveSettingsItems) {
                throw std::runtime_error("wave settings modifier count too large");
            }
            entity.wave.wavelength_modifiers.reserve(modifier_count);
            for (std::uint32_t modifier_index = 0; modifier_index < modifier_count;
                 ++modifier_index) {
                const double x_nm = read_binary<double>(input);
                const double delta_nm = read_binary<double>(input);
                entity.wave.wavelength_modifiers.push_back({x_nm, delta_nm});
            }
            max_entity_id = std::max(max_entity_id, entity.id);
            if (entity.type == EntityType::Wave) {
                ++wave_count;
            }
            state.entities.push_back(std::move(entity));
        }
        state.next_entity_id = std::max(state.next_entity_id, max_entity_id + 1);
        state.wave_serial = std::max(state.wave_serial, wave_count + 1);
        state.selected_entity_id = read_binary<std::int32_t>(input);
        state.segment.active = read_binary<std::uint8_t>(input) != 0;
        state.segment.selection_start =
            static_cast<std::size_t>(read_binary<std::uint64_t>(input));
        state.segment.selection_end =
            static_cast<std::size_t>(read_binary<std::uint64_t>(input));
        const std::string source_path = read_string(input);
        if (!source_path.empty()) {
            copy_to_buffer(state.path, source_path);
        }
        if (!find_entity(state, state.selected_entity_id) && !state.entities.empty()) {
            state.selected_entity_id = state.entities.front().id;
        }
        state.entity_name_id = 0;
        sync_entity_name(state);
    }

    ensure_workspace(state);
    copy_to_buffer(state.wave_path, path.lexically_normal().string());
    if (state.path[0] != '\0') {
        load_path(state);
        if (state.analysis) {
            state.analysis->sine.phase_radians = sine_phase;
            state.analysis->cosine.phase_radians = cosine_phase;
            state.analysis->wave_scale = static_cast<double>(state.wave_scale);
        }
    }
    state.status = "Loaded wave settings: " + path.filename().string();
}

void load_path(AppState& state) {
    try {
        ensure_workspace(state);
        state.status = "Loading";
        const std::filesystem::path source_path(state.path);
        const bool phase_fit_deferred = should_defer_phase_fit_on_load(source_path);
        state.lazy_block_mib = std::clamp(state.lazy_block_mib, 1, 10);
        const std::size_t block_bytes = lazy_block_bytes(state);
        state.analysis = load_lazy_analysis_window(
            source_path,
            block_bytes,
            state.max_slope,
            static_cast<double>(state.wave_scale),
            static_cast<double>(state.wave_tolerance),
            state.phase_steps,
            phase_fit_deferred ? 0u : static_cast<std::size_t>(state.phase_test_points),
            state.mappers);
        seed_lazy_cache_from_analysis(state);

        const auto& analysis = *state.analysis;
        Entity& data = ensure_data_entity(state);
        data.name = analysis.source_path.filename().string();
        if (state.selected_entity_id == 0 || !find_entity(state, state.selected_entity_id)) {
            select_entity(state, data.id);
        }
        const std::size_t count = source_sample_count(state);
        state.playhead_index = analysis.window.offset;
        state.playhead_fraction = 0.0;
        state.timeline_scroll_x = 0.0f;
        state.xline_playing = false;
        state.playhead_dragging = false;
        state.pending_scroll_index = state.playhead_index;
        if (!state.segment.active && count > 0u) {
            state.segment.selection_start = analysis.window.offset;
            state.segment.selection_end =
                std::min(count - 1u, analysis.window.offset + analysis.window.length - 1u);
        } else {
            clamp_segment(state);
        }
        state.status = "Loaded " + analysis.source_path.filename().string() +
                       " / lazy " + format_bytes(block_bytes) +
                       " / source " + format_bytes(analysis.source_size);
        if (phase_fit_deferred) {
            state.status += " / phase fit deferred";
        }
    } catch (const std::exception& error) {
        state.analysis.reset();
        state.status = error.what();
    }
}

void refresh_analysis_params(AppState& state) {
    if (!state.analysis) {
        return;
    }

    try {
        thrystr::Analysis& analysis = *state.analysis;
        analysis.max_abs_scalar_delta = thrystr::max_abs_scalar_delta(analysis.scalars);
        analysis.x_scale = thrystr::compute_x_scale(analysis.scalars, state.max_slope);
        analysis.wave_scale = static_cast<double>(state.wave_scale);
        fit_wave_phases(state);
        state.status = "Updated analysis params";
    } catch (const std::exception& error) {
        state.status = error.what();
    }
}

void reload_if_file_selected(AppState& state) {
    if (state.path[0] != '\0') {
        load_path(state);
    }
}

void configure_file_dialog(AppState& state, DialogPurpose purpose) {
    std::filesystem::path selected;
    if (purpose == DialogPurpose::OpenSource) {
        selected = state.path;
    } else {
        selected = state.wave_path;
    }

    std::error_code error;
    if (!selected.empty() && std::filesystem::is_regular_file(selected, error)) {
        std::filesystem::path parent = selected.parent_path();
        if (parent.empty()) {
            parent = std::filesystem::current_path(error);
            if (error) {
                parent = home_or_current_path();
            }
        }
        const std::string parent_text = parent.string();
        if (parent_text.size() >= sizeof(state.file_dialog.cwd)) {
            copy_to_buffer(state.file_dialog.cwd, home_or_current_path().string());
        } else {
            copy_to_buffer(state.file_dialog.cwd, parent_text);
        }
        copy_to_buffer(state.file_dialog.filename, selected.filename().string());
    } else {
        copy_to_buffer(state.file_dialog.cwd, home_or_current_path().string());
        if (purpose == DialogPurpose::SaveWave) {
            copy_to_buffer(state.file_dialog.filename,
                           std::string("wave") + kWaveSettingsExtension);
        } else {
            state.file_dialog.filename[0] = '\0';
        }
    }

    if (purpose == DialogPurpose::LoadWave || purpose == DialogPurpose::SaveWave) {
        copy_to_buffer(state.file_dialog.filter, "thryw");
    } else {
        state.file_dialog.filter[0] = '\0';
    }

    state.file_dialog.row_sel = -1;
    state.file_dialog_last_row = -1;
    state.file_dialog_dirty = true;
}

void request_file_dialog(AppState& state, DialogPurpose purpose) {
    configure_file_dialog(state, purpose);
    state.pending_dialog = purpose;
}

bool toolbar_icon_button(const char* glyph, const char* tooltip, bool accent = false) {
    const ImU32 text_color = accent
        ? ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_CheckMark])
        : skald::tokens::ink::primary;
    ImGui::PushStyleColor(ImGuiCol_Text,
                          skald::tokens::to_vec4(text_color));
    const bool clicked = skald::IconButton(glyph, tooltip);
    ImGui::PopStyleColor();
    return clicked;
}

bool window_control_button(WindowControl control, const char* tooltip, GLFWwindow* window) {
    const float size = kWindowControlButtonSize;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const char* id = control == WindowControl::Minimize
        ? "##window_minimize"
        : control == WindowControl::Maximize ? "##window_maximize" : "##window_close";
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();
    auto* draw = ImGui::GetWindowDrawList();

    const ImU32 fill = control == WindowControl::Close && hovered
        ? skald::tokens::with_alpha(skald::tokens::status::destructive, active ? 0.70f : 0.52f)
        : hovered ? skald::tokens::surface::control
                  : kTransparent;
    const ImU32 ink = hovered ? skald::tokens::ink::primary
                              : skald::tokens::ink::muted;
    draw->AddRectFilled(pos,
                        ImVec2(pos.x + size, pos.y + size),
                        fill,
                        skald::tokens::radii::ctrl);

    const ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
    if (control == WindowControl::Minimize) {
        draw->AddLine(ImVec2(center.x - 5.0f, center.y + 4.0f),
                      ImVec2(center.x + 5.0f, center.y + 4.0f),
                      ink,
                      1.6f);
    } else if (control == WindowControl::Maximize) {
        const bool maximized = window && glfwGetWindowAttrib(window, GLFW_MAXIMIZED);
        if (maximized) {
            draw->AddRect(ImVec2(center.x - 3.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 3.0f),
                          ink,
                          0.0f,
                          0,
                          1.4f);
            draw->AddRect(ImVec2(center.x - 6.0f, center.y - 2.0f),
                          ImVec2(center.x + 2.0f, center.y + 6.0f),
                          ink,
                          0.0f,
                          0,
                          1.4f);
        } else {
            draw->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 5.0f),
                          ink,
                          0.0f,
                          0,
                          1.5f);
        }
    } else {
        draw->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f),
                      ImVec2(center.x + 5.0f, center.y + 5.0f),
                      ink,
                      1.6f);
        draw->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f),
                      ImVec2(center.x - 5.0f, center.y + 5.0f),
                      ink,
                      1.6f);
    }

    if (hovered && tooltip && tooltip[0] != '\0') {
        skald::Tooltip(tooltip);
    }
    return clicked;
}

float window_control_group_width(int button_count) {
    if (button_count <= 0) {
        return kWindowControlRightMargin;
    }
    const float gaps =
        static_cast<float>(button_count - 1) * ImGui::GetStyle().ItemSpacing.x;
    return static_cast<float>(button_count) * kWindowControlButtonSize +
           gaps +
           kWindowControlRightMargin;
}

ImVec2 begin_titlebar_chrome(const char* id) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport.x, kTopChromeHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, skald::tokens::pad::window);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, skald::tokens::radii::none);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(2);
    return viewport;
}

void draw_titlebar_background(const ImVec2& viewport) {
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(0.0f, 0.0f),
                        ImVec2(viewport.x, kTopChromeHeight),
                        skald::tokens::surface::deep);
    draw->AddLine(ImVec2(0.0f, kTopChromeHeight - 1.0f),
                  ImVec2(viewport.x, kTopChromeHeight - 1.0f),
                  skald::tokens::border::separator,
                  1.0f);
}

void draw_titlebar_wordmark(AppState& state, const char* tagline) {
    ImGui::SetCursorPos(ImVec2(16.0f, 10.0f));
    if (state.fonts.sans_md) {
        ImGui::PushFont(state.fonts.sans_md);
    }
    ImGui::TextUnformatted("thrystr");
    if (state.fonts.sans_md) {
        ImGui::PopFont();
    }
    ImGui::SameLine(72.0f);
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted),
                       "%s",
                       tagline);
}

void draw_titlebar(AppState& state, GLFWwindow* window) {
    const ImVec2 viewport = begin_titlebar_chrome("##titlebar");
    draw_titlebar_background(viewport);
    draw_titlebar_wordmark(state, "/ wave workspace");

    ImGui::SameLine(188.0f);
    if (skald::GhostButton("File", ImVec2(52.0f, 0.0f))) {
        ImGui::OpenPopup("##file_menu");
    }
    if (ImGui::BeginPopup("##file_menu")) {
        if (ImGui::MenuItem("New Workspace")) {
            open_empty_workspace(state);
        }
        if (ImGui::MenuItem("Open Workspace...")) {
            request_file_dialog(state, DialogPurpose::LoadWave);
        }
        if (ImGui::MenuItem("Load Source...")) {
            request_file_dialog(state, DialogPurpose::OpenSource);
        }
        if (ImGui::MenuItem("Save Wave Data...")) {
            request_file_dialog(state, DialogPurpose::SaveWave);
        }
        skald::MutedSeparator();
        if (ImGui::MenuItem("Exit")) {
            state.request_close = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (toolbar_icon_button(skald::icons::kFile, "Load source data")) {
        request_file_dialog(state, DialogPurpose::OpenSource);
    }
    ImGui::SameLine();
    if (toolbar_icon_button(skald::icons::kFolder, "Load wave data")) {
        request_file_dialog(state, DialogPurpose::LoadWave);
    }
    ImGui::SameLine();
    if (toolbar_icon_button(skald::icons::kSave, "Save wave data")) {
        request_file_dialog(state, DialogPurpose::SaveWave);
    }

    ImGui::SameLine();
    if (skald::GhostButton("Create", ImVec2(70.0f, 0.0f))) {
        ImGui::OpenPopup("##create_menu");
    }
    if (ImGui::BeginPopup("##create_menu")) {
        if (ImGui::MenuItem("Wave", "Ctrl/Cmd+W")) {
            create_wave_entity(state);
        }
        if (ImGui::MenuItem("Interpolated Wave")) {
            create_interpolated_wave(state);
        }
        if (ImGui::MenuItem("X-Line Section")) {
            create_section(state);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (skald::GhostButton("Settings", ImVec2(82.0f, 0.0f))) {
        ImGui::OpenPopup("##settings_menu");
    }
    if (ImGui::BeginPopup("##settings_menu")) {
        if (ImGui::MenuItem("Preferences...")) {
            state.show_settings = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    const std::string source_name = state.path[0] == '\0'
        ? std::string("source: none")
        : std::string("source: ") + std::filesystem::path(state.path).filename().string();
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted), "%s",
                       source_name.c_str());

    ImGui::SameLine();
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted), "%s",
                       state.status.c_str());

    const float controls_width = window_control_group_width(3);
    ImGui::SetCursorPos(ImVec2(viewport.x - controls_width, 8.0f));
    if (window_control_button(WindowControl::Minimize, "Minimize", window)) {
        glfwIconifyWindow(window);
    }
    ImGui::SameLine();
    if (window_control_button(WindowControl::Maximize, "Maximize", window)) {
        if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
            glfwRestoreWindow(window);
        } else {
            glfwMaximizeWindow(window);
        }
    }
    ImGui::SameLine();
    if (window_control_button(WindowControl::Close, "Close", window)) {
        state.request_close = true;
    }

    ImGui::End();
}

bool value_bar_double(const char* label,
                      double* value,
                      double step_per_pixel,
                      const char* format = "%.4f",
                      ImFont* mono_font = nullptr) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const float width = std::max(80.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, 28.0f);
    ImGui::InvisibleButton("##value_bar", size);
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    bool changed = false;
    if (active) {
        const double delta = static_cast<double>(ImGui::GetIO().MouseDelta.x) * step_per_pixel;
        if (delta != 0.0 && std::isfinite(*value)) {
            *value += delta;
            changed = true;
        }
    }

    auto* draw = ImGui::GetWindowDrawList();
    const ImU32 fill = active
        ? skald::tokens::surface::control_hi
        : hovered ? skald::tokens::surface::control : skald::tokens::surface::panel_alt;
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                        fill, skald::tokens::radii::ctrl);
    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                  skald::tokens::border::default_, skald::tokens::radii::ctrl);
    char text[128] = {};
    std::snprintf(text, sizeof(text), format, *value);
    if (mono_font) {
        ImGui::PushFont(mono_font);
    }
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(pos.x + size.x - text_size.x - 10.0f,
                         pos.y + (size.y - text_size.y) * 0.5f),
                  skald::tokens::ink::primary, text);
    if (mono_font) {
        ImGui::PopFont();
    }
    if (hovered) {
        skald::Tooltip("Drag left or right to adjust");
    }
    ImGui::PopID();
    return changed;
}

bool value_bar_int(const char* label,
                   int* value,
                   double step_per_pixel,
                   ImFont* mono_font = nullptr) {
    double editable = static_cast<double>(*value);
    const bool changed = value_bar_double(label, &editable, step_per_pixel, "%.0f", mono_font);
    if (changed) {
        *value = static_cast<int>(std::llround(editable));
    }
    return changed;
}

void draw_value_mapper_stack(AppState& state) {
    skald::SectionHeader("Value Mappers");

    bool mapper_changed = false;
    int remove_index = -1;
    int move_from = -1;
    int move_to = -1;

    if (ImGui::BeginTable("##mapper_table", 4,
                          ImGuiTableFlags_BordersInnerH |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 46.0f);
        ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableHeadersRow();

        static constexpr const char* kKinds[] = {"add", "sub", "mul", "div"};
        for (std::size_t i = 0; i < state.mappers.size(); ++i) {
            thrystr::ValueMapper& mapper = state.mappers[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            mapper_changed |= ImGui::Checkbox("##enabled", &mapper.enabled);
            ImGui::SameLine();
            ImGui::Text("%zu", i + 1u);

            ImGui::TableSetColumnIndex(1);
            int kind = 0;
            switch (mapper.kind) {
            case thrystr::ValueMapperKind::Add:
                kind = 0;
                break;
            case thrystr::ValueMapperKind::Subtract:
                kind = 1;
                break;
            case thrystr::ValueMapperKind::Multiply:
                kind = 2;
                break;
            case thrystr::ValueMapperKind::Divide:
                kind = 3;
                break;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##kind", &kind, kKinds, IM_ARRAYSIZE(kKinds))) {
                mapper.kind = static_cast<thrystr::ValueMapperKind>(kind);
                mapper_changed = true;
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            mapper_changed |= ImGui::DragScalar("##operand",
                                                ImGuiDataType_Double,
                                                &mapper.operand,
                                                0.1f,
                                                nullptr,
                                                nullptr,
                                                "%.6f");

            ImGui::TableSetColumnIndex(3);
            if (skald::IconButton(skald::icons::kChevronRight, "Move up", 22.0f) && i > 0) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i - 1);
            }
            ImGui::SameLine();
            if (skald::IconButton(skald::icons::kChevronDown, "Move down", 22.0f) &&
                i + 1 < state.mappers.size()) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i + 1);
            }
            ImGui::SameLine();
            if (skald::IconButton(skald::icons::kX, "Remove", 22.0f)) {
                remove_index = static_cast<int>(i);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (remove_index >= 0) {
        state.mappers.erase(state.mappers.begin() + remove_index);
        mapper_changed = true;
    }
    if (move_from >= 0 && move_to >= 0) {
        std::swap(state.mappers[static_cast<std::size_t>(move_from)],
                  state.mappers[static_cast<std::size_t>(move_to)]);
        mapper_changed = true;
    }

    if (skald::GhostButton("+ add")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Add, 1.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (skald::GhostButton("- sub")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Subtract, 1.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (skald::GhostButton("* mul")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Multiply, 2.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (skald::GhostButton("/ div")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Divide, 2.0, true});
        mapper_changed = true;
    }

    skald::KvRow("tail", "uint %% 256 -> [-1, 1)");

    if (mapper_changed) {
        reload_if_file_selected(state);
    }
}

void draw_inspector_overlay(AppState& state) {
    if (!state.show_inspector_overlay) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(18.0f, kTopChromeHeight + 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("##inspector_overlay", &state.show_inspector_overlay,
                      ImGuiWindowFlags_NoTitleBar |
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    skald::SectionHeader("Inspector");
    skald::SectionHeader("Analysis");
    if (state.fonts.mono) {
        ImGui::PushFont(state.fonts.mono);
    }
    if (state.analysis) {
        const thrystr::Analysis& a = *state.analysis;
        skald::KvRow("source", "%s", format_bytes(a.source_size).c_str());
        skald::KvRow("windows", "%s", format_count(a.window_count).c_str());
        skald::KvRow("offset", "%s", format_count(a.window.offset).c_str());
        skald::KvRow("length", "%s", format_bytes(a.window.length).c_str());
        skald::KvRow("max delta", "%u", static_cast<unsigned>(a.window.max_delta));
        skald::KvRow("x scale", "%.3f", static_cast<double>(a.x_scale));
        skald::KvRow("points", "%s", format_count(source_sample_count(state)).c_str());
        skald::KvRow("mappers", "%s", format_count(a.mappers.size()).c_str());
    } else {
        skald::KvRowStatus("state", "empty", skald::BadgeTone::Muted);
    }

    if (state.analysis) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Legacy Fit");
        const thrystr::Analysis& a = *state.analysis;
        skald::KvRow("scale", "%.4f", a.wave_scale);
        skald::KvRow("sine hits", "%s", format_count(a.sine.hits).c_str());
        skald::KvRow("sine phase", "%.4f", a.sine.phase_radians);
        skald::KvRow("cos hits", "%s", format_count(a.cosine.hits).c_str());
        skald::KvRow("cos phase", "%.4f", a.cosine.phase_radians);
        skald::KvRow("tested", "%s", format_count(a.sine.tested_points).c_str());
    }
    if (state.fonts.mono) {
        ImGui::PopFont();
    }

    ImGui::End();
}

void draw_settings_dialog(AppState& state) {
    if (state.show_settings) {
        ImGui::OpenPopup("Settings");
    }

    bool open = state.show_settings;
    ImGui::SetNextWindowSize(kSettingsDialogSize, ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,
                          skald::tokens::to_vec4(skald::tokens::surface::panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, skald::tokens::pad::window);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, skald::tokens::radii::modal);
    if (ImGui::BeginPopupModal("Settings", &open, ImGuiWindowFlags_NoCollapse)) {
        skald::SectionHeader("Overlays");
        skald::PillToggle("Inspector overlay", &state.show_inspector_overlay);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Render");
        skald::PillToggle("Data points", &state.show_points);
        ImGui::SameLine();
        skald::PillToggle("Data lines", &state.show_lines);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        double zoom_x = static_cast<double>(state.zoom_x);
        double zoom_y = static_cast<double>(state.zoom_y);
        if (value_bar_double("x zoom", &zoom_x, 0.01, "%.2f", state.fonts.mono)) {
            state.zoom_x = static_cast<float>(zoom_x);
        }
        if (value_bar_double("y zoom", &zoom_y, 0.01, "%.2f", state.fonts.mono)) {
            state.zoom_y = static_cast<float>(zoom_y);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (skald::AccentButton("Done", ImVec2(96.0f, 0.0f))) {
            state.show_settings = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (!open) {
        state.show_settings = false;
    }
}

StartupAction draw_splash(AppState& state) {
    if (!state.splash_open) {
        return StartupAction::None;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const float body_top = kTopChromeHeight;
    const ImVec2 body_size(viewport.x, std::max(160.0f, viewport.y - body_top));
    ImGui::SetNextWindowPos(ImVec2(0.0f, body_top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(body_size, ImGuiCond_Always);
    ImGui::Begin("##splash_host", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);

    const std::array<skald::SplashAction, 3> actions = {{
        {"New workspace", "Ctrl+N"},
        {"Open workspace", ""},
        {"Load source", ""},
    }};
    const skald::SplashChoice splash_choice = skald::Splash(
        "thrystr",
        "Scalar wave workspace",
        {},
        std::span<const skald::SplashAction>(actions.data(), actions.size()),
        static_cast<ImTextureID>(state.splash_hero_texture),
        state.splash_hero_size,
        state.fonts.hero);

    ImGui::End();

    if (splash_choice.kind == skald::SplashChoice::Kind::Action) {
        switch (splash_choice.index) {
        case 0:
            return StartupAction::NewWorkspace;
        case 1:
            return StartupAction::OpenWorkspace;
        case 2:
            return StartupAction::LoadSource;
        default:
            break;
        }
    }
    return StartupAction::None;
}

void draw_splash_titlebar(AppState& state, GLFWwindow* window) {
    const ImVec2 viewport = begin_titlebar_chrome("##splash_titlebar");
    draw_titlebar_background(viewport);
    draw_titlebar_wordmark(state, "/ start");

    ImGui::SetCursorPos(ImVec2(viewport.x - window_control_group_width(1), 8.0f));
    if (window_control_button(WindowControl::Close, "Close", window)) {
        state.request_close = true;
    }

    ImGui::End();
}

StartupAction draw_splash_window(AppState& state,
                                 GLFWwindow* window,
                                 const ChromeCursors& cursors) {
    StartupAction choice = draw_splash(state);
    draw_splash_titlebar(state, window);
    handle_custom_chrome(state, window, cursors, false);
    return choice;
}

void draw_entity_toolbox(AppState& state) {
    if (!state.workspace_open) {
        return;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(std::max(0.0f, viewport.x - kToolboxWidth),
                                   kTopChromeHeight));
    ImGui::SetNextWindowSize(ImVec2(std::min(kToolboxWidth, viewport.x),
                                    std::max(120.0f, viewport.y - kTopChromeHeight)));
    ImGui::Begin("##toolbox", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

    skald::SectionHeader("Toolbox");
    skald::SectionHeader("Entities");
    if (skald::AccentButton("+ Wave", ImVec2(92.0f, 0.0f))) {
        create_wave_entity(state);
    }
    ImGui::SameLine();
    if (skald::GhostButton("Fit Wave", ImVec2(96.0f, 0.0f))) {
        create_interpolated_wave(state);
    }
    ImGui::SameLine();
    if (skald::GhostButton("Section", ImVec2(92.0f, 0.0f))) {
        create_section(state);
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    for (Entity& entity : state.entities) {
        ImGui::PushID(entity.id);
        ImGui::Checkbox("##visible", &entity.visible);
        ImGui::SameLine();
        const std::string label = entity.name + "  [" + entity_type_name(entity.type) + "]";
        if (ImGui::Selectable(label.c_str(), entity.id == state.selected_entity_id)) {
            select_entity(state, entity.id);
        }
        ImGui::PopID();
    }

    Entity* data = data_entity(state);
    if (data || state.analysis) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Data");
        if (state.analysis) {
            const thrystr::Analysis& a = *state.analysis;
            skald::KvRow("points", "%s", format_count(source_sample_count(state)).c_str());
            skald::KvRow("loaded blocks", "%s",
                         format_count(state.lazy_blocks.size()).c_str());
            skald::KvRow("block size", "%s", format_bytes(lazy_block_bytes(state)).c_str());
            skald::KvRow("source", "%s", format_bytes(a.source_size).c_str());
        } else {
            skald::KvRowStatus("source", "none", skald::BadgeTone::Muted);
        }
        if (data &&
            value_bar_double("point spacing nm", &data->data.spatial_period_nm,
                             0.01, "%.6f", state.fonts.mono)) {
            data->data.spatial_period_nm =
                std::max(0.000001, data->data.spatial_period_nm);
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("X-Line Section");
        if (state.analysis && source_sample_count(state) > 0u) {
            if (!state.segment.active) {
                if (skald::AccentButton("Create Section", ImVec2(132.0f, 0.0f))) {
                    create_section(state);
                }
            } else {
                clamp_segment(state);
                const double period = data_spatial_period_nm(state);
                double start_index = static_cast<double>(state.segment.selection_start);
                double end_index = static_cast<double>(state.segment.selection_end);
                bool changed = false;
                changed |= value_bar_double("start index",
                                            &start_index,
                                            0.25,
                                            "%.0f",
                                            state.fonts.mono);
                changed |= value_bar_double("end index",
                                            &end_index,
                                            0.25,
                                            "%.0f",
                                            state.fonts.mono);
                if (changed) {
                    const std::size_t count = source_sample_count(state);
                    state.segment.selection_start = clamp_index(start_index, count);
                    state.segment.selection_end = clamp_index(end_index, count);
                    clamp_segment(state);
                }
                double start_nm =
                    static_cast<double>(state.segment.selection_start) * period;
                double end_nm =
                    static_cast<double>(state.segment.selection_end) * period;
                bool nm_changed = false;
                nm_changed |= value_bar_double("start nm",
                                               &start_nm,
                                               period * 0.25,
                                               "%.3f",
                                               state.fonts.mono);
                nm_changed |= value_bar_double("end nm",
                                               &end_nm,
                                               period * 0.25,
                                               "%.3f",
                                               state.fonts.mono);
                if (nm_changed) {
                    const std::size_t count = source_sample_count(state);
                    state.segment.selection_start = clamp_index(start_nm / period, count);
                    state.segment.selection_end = clamp_index(end_nm / period, count);
                    clamp_segment(state);
                }
                const auto [first, last] = normalized_selection(state);
                skald::KvRow("span", "%.3f nm",
                             static_cast<double>(last - first) * period);
                if (skald::GhostButton("Select All", ImVec2(104.0f, 0.0f))) {
                    create_section(state);
                }
            }
        } else {
            skald::KvRowStatus("section", "no data", skald::BadgeTone::Muted);
        }
    }

    if (!find_entity(state, state.selected_entity_id) && !state.entities.empty()) {
        select_entity(state, state.entities.front().id);
    }
    Entity* selected = find_entity(state, state.selected_entity_id);
    if (!selected) {
        ImGui::End();
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    skald::SectionHeader("Selection");
    sync_entity_name(state);
    if (ImGui::InputText("Name", state.entity_name, sizeof(state.entity_name))) {
        selected->name = state.entity_name;
    }
    skald::PillToggle("Visible", &selected->visible);

    if (selected->type == EntityType::Data) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Load Params");
        bool params_changed = false;
        bool lazy_block_changed = false;
        double max_slope = static_cast<double>(state.max_slope);
        double wave_tolerance = static_cast<double>(state.wave_tolerance);
        params_changed |= value_bar_double("max slope",
                                           &max_slope,
                                           0.001,
                                           "%.4f",
                                           state.fonts.mono);
        params_changed |= value_bar_double("wave tolerance",
                                           &wave_tolerance,
                                           0.0001,
                                           "%.5f",
                                           state.fonts.mono);
        params_changed |= value_bar_int("phase steps", &state.phase_steps, 4.0, state.fonts.mono);
        params_changed |= value_bar_int("phase samples",
                                        &state.phase_test_points,
                                        512.0,
                                        state.fonts.mono);
        lazy_block_changed |= value_bar_int("lazy block MiB",
                                            &state.lazy_block_mib,
                                            0.05,
                                            state.fonts.mono);
        state.max_slope = static_cast<float>(max_slope);
        state.wave_tolerance = static_cast<float>(wave_tolerance);
        if (lazy_block_changed) {
            state.lazy_block_mib = std::clamp(state.lazy_block_mib, 1, 10);
            reload_if_file_selected(state);
        }
        if (params_changed) {
            refresh_analysis_params(state);
        }
        if (skald::GhostButton("Fit Phases", ImVec2(112.0f, 0.0f))) {
            fit_wave_phases(state);
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        draw_value_mapper_stack(state);
    } else {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Wave");
        value_bar_double("wave spatial distance nm",
                         &selected->wave.wavelength_nm,
                         0.10,
                         "%.4f",
                         state.fonts.mono);
        value_bar_double("amplitude", &selected->wave.amplitude, 0.01, "%.4f", state.fonts.mono);
        value_bar_double("amplitude offset",
                         &selected->wave.amplitude_offset,
                         0.01,
                         "%.4f",
                         state.fonts.mono);
        value_bar_double("phase nm", &selected->wave.phase_nm, 0.10, "%.4f", state.fonts.mono);
        skald::KvRow("modifiers", "%s",
                     format_count(selected->wave.wavelength_modifiers.size()).c_str());
        if (skald::GhostButton("Fit Selection", ImVec2(124.0f, 0.0f))) {
            const WaveFitResult fit = fit_wave_to_selection(state, selected->wave);
            selected->wave.wavelength_nm = fit.wavelength_nm;
            selected->wave.phase_nm = fit.phase_nm;
            rebuild_wavelength_modifiers(state, selected->wave);
            const auto [first, last] = analysis_selection_bounds(state);
            const WaveFitResult final_fit =
                score_wave_on_range(state, selected->wave, first, last);
            state.status = "Fitted " + selected->name + ": " +
                           format_count(final_fit.hits) + "/" +
                           format_count(final_fit.tested) + " point hits, " +
                           format_count(selected->wave.wavelength_modifiers.size()) +
                           " wavelength keys";
        }
    }

    ImGui::End();
}

float y_for_value(float value, float top, float bottom, float zoom_y) {
    const float scaled = value * zoom_y;
    const float normalized = (scaled + 1.0f) * 0.5f;
    return bottom - normalized * (bottom - top);
}

float plot_x_step(const AppState& state) {
    if (!state.analysis) {
        return 1.0f;
    }
    const float slope_scale = std::max(1.0f, state.analysis->x_scale);
    const float point_spacing =
        static_cast<float>(std::clamp(data_spatial_period_nm(state), 0.000001, 1.0e6));
    const float zoom = std::max(0.01f, std::abs(state.zoom_x));
    return std::max(0.05f, point_spacing * slope_scale * zoom);
}

void draw_data_point_markers(ImDrawList* draw,
                             const AppState& state,
                             std::size_t first,
                             std::size_t last,
                             float plot_left,
                             float plot_top,
                             float plot_bottom,
                             float x_step,
                             float y_zoom,
                             const ImVec2& clip_min,
                             const ImVec2& clip_max) {
    const std::size_t count = source_sample_count(state);
    if (!draw || count == 0u || last < first) {
        return;
    }

    if (x_step >= 3.0f) {
        for (std::size_t i = first; i <= last && i < count; ++i) {
            const std::optional<float> scalar = scalar_at(state, i);
            if (!scalar) {
                continue;
            }
            const ImVec2 point(plot_left + static_cast<float>(i) * x_step,
                               y_for_value(*scalar, plot_top, plot_bottom, y_zoom));
            draw->AddCircleFilled(point, 2.2f, kDataPointHi);
        }
        return;
    }

    const int column_count =
        std::max(1, static_cast<int>(std::ceil(std::max(1.0f, clip_max.x - clip_min.x))));
    std::vector<float> min_y(static_cast<std::size_t>(column_count), FLT_MAX);
    std::vector<float> max_y(static_cast<std::size_t>(column_count), -FLT_MAX);

    for (std::size_t i = first; i <= last && i < count; ++i) {
        const std::optional<float> scalar = scalar_at(state, i);
        if (!scalar) {
            continue;
        }
        const float x = plot_left + static_cast<float>(i) * x_step;
        if (x < clip_min.x || x > clip_max.x) {
            continue;
        }
        const int column = std::clamp(static_cast<int>(std::floor(x - clip_min.x)),
                                      0,
                                      column_count - 1);
        const float y = y_for_value(*scalar, plot_top, plot_bottom, y_zoom);
        min_y[static_cast<std::size_t>(column)] =
            std::min(min_y[static_cast<std::size_t>(column)], y);
        max_y[static_cast<std::size_t>(column)] =
            std::max(max_y[static_cast<std::size_t>(column)], y);
    }

    for (int column = 0; column < column_count; ++column) {
        const std::size_t index = static_cast<std::size_t>(column);
        if (min_y[index] == FLT_MAX) {
            continue;
        }
        const float x = clip_min.x + static_cast<float>(column) + 0.5f;
        if (std::abs(max_y[index] - min_y[index]) < 1.0f) {
            draw->AddRectFilled(ImVec2(x - 0.5f, min_y[index] - 0.5f),
                                ImVec2(x + 0.5f, min_y[index] + 0.5f),
                                kDataPointMed);
        } else {
            draw->AddLine(ImVec2(x, min_y[index]),
                          ImVec2(x, max_y[index]),
                          kDataPointLo,
                          1.0f);
        }
    }
}

void set_playhead_index(AppState& state, std::size_t index, bool center_view) {
    const std::size_t count = source_sample_count(state);
    if (count == 0u) {
        state.playhead_index = 0;
        state.playhead_fraction = 0.0;
        return;
    }
    state.playhead_index = std::min(index, count - 1u);
    state.playhead_fraction = 0.0;
    if (center_view) {
        state.pending_scroll_index = state.playhead_index;
    }
}

void move_playhead_by(AppState& state, int delta) {
    const std::size_t count = source_sample_count(state);
    if (count == 0u || delta == 0) {
        return;
    }
    if (delta < 0) {
        const std::size_t amount = static_cast<std::size_t>(-delta);
        set_playhead_index(state,
                           amount > state.playhead_index ? 0u : state.playhead_index - amount,
                           true);
        return;
    }
    const std::size_t amount = static_cast<std::size_t>(delta);
    const std::size_t max_index = count - 1u;
    const std::size_t clamped_playhead = std::min(state.playhead_index, max_index);
    const std::size_t remaining = max_index - clamped_playhead;
    set_playhead_index(state,
                       amount >= remaining ? max_index : clamped_playhead + amount,
                       true);
}

void toggle_xline_playback(AppState& state) {
    if (source_sample_count(state) == 0u) {
        return;
    }
    state.xline_playing = !state.xline_playing;
    state.playhead_dragging = false;
    state.playhead_fraction = 0.0;
}

void apply_custom_playback_speed(AppState& state) {
    char* end = nullptr;
    const double value = std::strtod(state.custom_playback_speed_text, &end);
    if (end != state.custom_playback_speed_text &&
        std::isfinite(value) &&
        value > 0.0) {
        state.playback_points_per_second = value;
    }
}

bool playback_speed_button(const char* label, bool selected, float width = 42.0f) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              skald::tokens::to_vec4(skald::tokens::status::info));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              skald::tokens::to_vec4(skald::tokens::status::info));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              skald::tokens::to_vec4(skald::tokens::surface::window));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              skald::tokens::to_vec4(skald::tokens::surface::control));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              skald::tokens::to_vec4(skald::tokens::surface::control_hi));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              skald::tokens::to_vec4(skald::tokens::ink::primary));
    }
    const bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::PopStyleColor(3);
    return clicked;
}

void draw_playback_speed_controls(AppState& state) {
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted), "speed");
    for (double preset : kPlaybackSpeedPresets) {
        ImGui::SameLine();
        char label[16] = {};
        std::snprintf(label, sizeof(label), "%.0f", preset);
        const bool selected =
            !state.custom_playback_speed &&
            std::abs(state.playback_points_per_second - preset) < 0.001;
        if (playback_speed_button(label, selected)) {
            state.custom_playback_speed = false;
            state.playback_points_per_second = preset;
            state.playhead_fraction = 0.0;
        }
    }

    ImGui::SameLine();
    if (playback_speed_button("custom", state.custom_playback_speed, 74.0f)) {
        if (!state.custom_playback_speed) {
            std::snprintf(state.custom_playback_speed_text,
                          sizeof(state.custom_playback_speed_text),
                          "%.3g",
                          state.playback_points_per_second);
        }
        state.custom_playback_speed = true;
        apply_custom_playback_speed(state);
        state.playhead_fraction = 0.0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Use a typed playback rate");
    }

    if (state.custom_playback_speed) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(78.0f);
        if (ImGui::InputText("##custom_playback_pps",
                             state.custom_playback_speed_text,
                             sizeof(state.custom_playback_speed_text))) {
            apply_custom_playback_speed(state);
            state.playhead_fraction = 0.0;
        }
    }
}

void update_xline_playback(AppState& state) {
    const std::size_t count = source_sample_count(state);
    const double points_per_second = std::max(0.0, state.playback_points_per_second);
    if (!state.xline_playing || count == 0u || points_per_second <= 0.0) {
        return;
    }

    state.playhead_fraction += ImGui::GetIO().DeltaTime * points_per_second;
    const auto step_count = static_cast<std::size_t>(state.playhead_fraction);
    if (step_count == 0u) {
        return;
    }
    state.playhead_fraction -= static_cast<double>(step_count);
    state.playhead_index = std::min(count - 1u, state.playhead_index + step_count);
    if (state.playhead_index + 1u >= count) {
        state.xline_playing = false;
        state.playhead_fraction = 0.0;
    }
}

std::optional<std::uint8_t> ticker_byte_at(const AppState& state, std::size_t index) {
    if (const std::optional<std::uint8_t> byte = raw_byte_at(state, index)) {
        return byte;
    }
    if (const std::optional<float> scalar = scalar_at(state, index)) {
        return byte_from_scalar(*scalar);
    }
    return std::nullopt;
}

void draw_data_ticker(const AppState& state,
                      ImDrawList* draw,
                      std::size_t first,
                      std::size_t last,
                      float plot_left,
                      float plot_bottom,
                      float x_step,
                      const ImVec2& clip_min,
                      const ImVec2& clip_max) {
    if (!draw || source_sample_count(state) == 0u || last < first) {
        return;
    }

    const float ticker_top = plot_bottom + 22.0f;
    const float ticker_bottom = clip_max.y - 8.0f;
    if (ticker_bottom <= ticker_top) {
        return;
    }

    draw->AddRectFilled(ImVec2(clip_min.x, ticker_top - 4.0f),
                        ImVec2(clip_max.x, ticker_bottom),
                        skald::tokens::surface::panel_alt);
    draw->AddLine(ImVec2(clip_min.x, ticker_top - 4.0f),
                  ImVec2(clip_max.x, ticker_top - 4.0f),
                  skald::tokens::border::separator,
                  1.0f);

    const std::size_t stride = std::max<std::size_t>(
        1u,
        static_cast<std::size_t>(std::ceil(
            static_cast<double>(kTickerTargetLabelPixels) /
            std::max(1.0f, x_step))));
    const std::size_t start = first - (first % stride);
    for (std::size_t i = start; i <= last && i < source_sample_count(state); i += stride) {
        if (i == state.playhead_index) {
            continue;
        }
        const std::optional<std::uint8_t> byte = ticker_byte_at(state, i);
        if (!byte) {
            continue;
        }
        const std::string label = hex_byte_text(*byte);
        const float x = plot_left + static_cast<float>(i) * x_step;
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(x - text_size.x * 0.5f, ticker_top),
                      skald::tokens::ink::muted,
                      label.c_str());
    }

    if (state.playhead_index < first || state.playhead_index > last) {
        return;
    }
    const std::optional<std::uint8_t> current_byte =
        ticker_byte_at(state, state.playhead_index);
    if (!current_byte) {
        return;
    }

    const std::string label = hex_byte_text(*current_byte);
    const float x = plot_left + static_cast<float>(state.playhead_index) * x_step;
    ImFont* current_font = state.fonts.mono ? state.fonts.mono : ImGui::GetFont();
    const float current_font_size = ImGui::GetFontSize() * kTickerCurrentScale;
    const ImVec2 text_size =
        current_font->CalcTextSizeA(current_font_size,
                                    FLT_MAX,
                                    0.0f,
                                    label.c_str());
    const ImVec2 text_pos(x - text_size.x * 0.5f, ticker_top);
    draw->AddRectFilled(ImVec2(text_pos.x - 6.0f, text_pos.y - 3.0f),
                        ImVec2(text_pos.x + text_size.x + 6.0f,
                               text_pos.y + text_size.y + 3.0f),
                        skald::tokens::surface::control_hi,
                        skald::tokens::radii::ctrl);
    draw->AddText(current_font,
                  current_font_size,
                  text_pos,
                  skald::tokens::ink::primary,
                  label.c_str());
}

void draw_plot(AppState& state) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const float toolbox_width = state.workspace_open ? std::min(kToolboxWidth, viewport.x * 0.45f) : 0.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, kTopChromeHeight));
    ImGui::SetNextWindowSize(ImVec2(std::max(120.0f, viewport.x - toolbox_width),
                                    std::max(120.0f, viewport.y - kTopChromeHeight)));
    ImGui::Begin("Waveform", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!state.analysis || source_sample_count(state) == 0u) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##empty_plot", available);
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        draw->AddRectFilled(min, max, skald::tokens::surface::window);
        draw->AddRect(min, max, skald::tokens::border::separator);
        draw->AddText(ImVec2(min.x + 24.0f, min.y + 24.0f),
                      skald::tokens::ink::muted,
                      state.workspace_open ? "No source data" : "No workspace");
        ImGui::End();
        return;
    }

    update_xline_playback(state);
    const thrystr::Analysis& analysis = *state.analysis;
    const std::size_t count = source_sample_count(state);
    state.segment.selection_start = std::min(state.segment.selection_start, count - 1u);
    state.segment.selection_end = std::min(state.segment.selection_end, count - 1u);
    state.playhead_index = std::min(state.playhead_index, count - 1u);
    const float x_step = plot_x_step(state);
    const float y_zoom = std::max(0.01f, std::abs(state.zoom_y));
    const double period_nm = data_spatial_period_nm(state);
    ensure_lazy_blocks(state, state.playhead_index, state.playhead_index);
    if (skald::IconButton(state.xline_playing ? skald::icons::kPause : skald::icons::kPlay,
                          state.xline_playing ? "Pause x-line" : "Play x-line")) {
        toggle_xline_playback(state);
    }
    ImGui::SameLine();
    const std::optional<std::uint8_t> playhead_byte =
        ticker_byte_at(state, state.playhead_index);
    const std::string playhead_hex = playhead_byte ? hex_byte_text(*playhead_byte) : "--";
    const float readout_x = ImGui::GetCursorPosX();
    ImGui::BeginChild("##playback_readout",
                      ImVec2(kPlaybackReadoutWidth, kPlaybackReadoutHeight),
                      false,
                      ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted),
                       "x %s",
                       format_count(state.playhead_index).c_str());
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted),
                       "hex %s",
                       playhead_hex.c_str());
    ImGui::EndChild();
    ImGui::SameLine(readout_x + kPlaybackReadoutWidth);
    if (playback_speed_button(state.wheel_scroll_mode ? "wheel scroll" : "wheel scale",
                              state.wheel_scroll_mode,
                              102.0f)) {
        state.wheel_scroll_mode = !state.wheel_scroll_mode;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.wheel_scroll_mode
                              ? "Mouse wheel scrolls the x timeline"
                              : "Mouse wheel scales the x timeline");
    }
    ImGui::SameLine();
    if (playback_speed_button("segment",
                              state.segment_selection_mode,
                              78.0f)) {
        state.segment_selection_mode = !state.segment_selection_mode;
        state.selection_drag_handle = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.segment_selection_mode
                              ? "Timeline clicks edit the segment selection"
                              : "Timeline clicks move the playhead");
    }
    if (ImGui::GetContentRegionAvail().x > 330.0f) {
        ImGui::SameLine();
    }
    draw_playback_speed_controls(state);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float plot_height = std::max(360.0f, available.y - 8.0f);
    const float margin_left = 58.0f;
    const float margin_right = 30.0f;
    const float margin_top = 28.0f;
    const float margin_bottom = 84.0f;
    const float logical_width = std::max(available.x,
        margin_left + margin_right + static_cast<float>(count - 1) * x_step);

    ImGui::BeginChild("##plot_scroll", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 child_pos = ImGui::GetWindowPos();
    const ImVec2 child_size = ImGui::GetWindowSize();
    const bool child_mouse_inside =
        io.MousePos.x >= child_pos.x &&
        io.MousePos.x <= child_pos.x + child_size.x &&
        io.MousePos.y >= child_pos.y &&
        io.MousePos.y <= child_pos.y + child_size.y;
    const float child_width = ImGui::GetWindowWidth();
    const float max_scroll_x = std::max(0.0f, logical_width - child_width);
    const float native_scroll_x = ImGui::GetScrollX();
    state.timeline_scroll_x = std::clamp(state.timeline_scroll_x, 0.0f, max_scroll_x);
    if (!state.pending_scroll_index &&
        !state.xline_playing &&
        io.MouseWheel == 0.0f &&
        std::abs(native_scroll_x - state.timeline_scroll_x) > 0.5f) {
        state.timeline_scroll_x = std::clamp(native_scroll_x, 0.0f, max_scroll_x);
    }
    if (state.pending_scroll_index) {
        const float target_scroll =
            static_cast<float>(*state.pending_scroll_index) * x_step +
            margin_left -
            child_width * 0.5f;
        state.timeline_scroll_x = std::clamp(target_scroll, 0.0f, max_scroll_x);
        state.pending_scroll_index.reset();
    }
    if (state.xline_playing) {
        const float playhead_local =
            margin_left + static_cast<float>(state.playhead_index) * x_step;
        const float visible_midpoint = state.timeline_scroll_x + child_width * 0.5f;
        if (playhead_local >= visible_midpoint) {
            state.timeline_scroll_x =
                std::clamp(playhead_local - child_width * 0.5f, 0.0f, max_scroll_x);
        }
    }
    if (child_mouse_inside &&
        state.wheel_scroll_mode &&
        io.MouseWheel != 0.0f &&
        !io.KeyCtrl &&
        !io.KeySuper) {
        const float scroll_step =
            std::clamp(child_width * 0.18f, 64.0f, 420.0f);
        state.timeline_scroll_x =
            std::clamp(state.timeline_scroll_x -
                           io.MouseWheel * scroll_step,
                       0.0f,
                       max_scroll_x);
    }
    ImGui::SetScrollX(state.timeline_scroll_x);
    ImGui::InvisibleButton("##plot_canvas", ImVec2(logical_width, plot_height));
    ImGui::SetScrollX(state.timeline_scroll_x);
    const bool plot_hovered = ImGui::IsItemHovered();

    const ImVec2 item_min = ImGui::GetItemRectMin();
    const ImVec2 item_max = ImGui::GetItemRectMax();
    const ImVec2 clip_min(child_pos.x, child_pos.y);
    const ImVec2 clip_max(child_pos.x + child_size.x, child_pos.y + child_size.y);

    const float plot_left = child_pos.x + margin_left - state.timeline_scroll_x;
    const float plot_right = plot_left + static_cast<float>(count - 1) * x_step;
    const float plot_top = item_min.y + margin_top;
    const float plot_bottom = item_max.y - margin_bottom;
    if (plot_hovered && io.MouseWheel != 0.0f) {
        if (io.KeyCtrl || io.KeySuper) {
            const float factor = std::pow(1.12f, io.MouseWheel);
            state.zoom_y = std::max(0.01f, state.zoom_y * factor);
        } else if (io.KeyShift && !state.wheel_scroll_mode) {
            const float scroll_step =
                std::clamp(ImGui::GetWindowWidth() * 0.18f, 64.0f, 420.0f);
            state.timeline_scroll_x =
                std::clamp(state.timeline_scroll_x - io.MouseWheel * scroll_step,
                           0.0f,
                           max_scroll_x);
            ImGui::SetScrollX(state.timeline_scroll_x);
        } else if (!state.wheel_scroll_mode) {
            const float factor = std::pow(1.12f, io.MouseWheel);
            const float old_step = x_step;
            const float mouse_index = std::clamp(
                (io.MousePos.x - plot_left) / old_step,
                0.0f,
                static_cast<float>(count - 1u));
            state.zoom_x = std::max(0.01f, state.zoom_x * factor);
            const float new_step = plot_x_step(state);
            const float mouse_local_x = io.MousePos.x - child_pos.x;
            const float new_logical_width = std::max(
                available.x,
                margin_left + margin_right + static_cast<float>(count - 1) * new_step);
            const float new_max_scroll_x = std::max(0.0f, new_logical_width - child_width);
            state.timeline_scroll_x =
                std::clamp(mouse_index * new_step + margin_left - mouse_local_x,
                           0.0f,
                           new_max_scroll_x);
            ImGui::SetScrollX(state.timeline_scroll_x);
        }
    }
    const float visible_left_local = state.timeline_scroll_x;
    const float visible_width = child_width;
    const float index_left = std::max(0.0f, (visible_left_local - margin_left) / x_step);
    const float index_right = std::min(static_cast<float>(count - 1),
        (visible_left_local + visible_width - margin_left) / x_step);
    const std::size_t first = std::min(count - 1,
        static_cast<std::size_t>(std::floor(std::max(0.0f, index_left))));
    const std::size_t last = std::min(count - 1,
        static_cast<std::size_t>(std::ceil(std::max(index_left, index_right))) + 2u);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const auto index_from_x = [&](float x) {
        const double index = std::round((x - plot_left) / x_step);
        return static_cast<std::size_t>(
            std::clamp(index, 0.0, static_cast<double>(count - 1u)));
    };
    const auto index_from_mouse = [&]() {
        return index_from_x(mouse.x);
    };
    const float playhead_hit_x =
        plot_left + static_cast<float>(state.playhead_index) * x_step;
    const bool playhead_hit =
        std::abs(mouse.x - playhead_hit_x) <= kPlayheadScrubHitPixels &&
        mouse.y >= plot_top &&
        mouse.y <= clip_max.y;
    bool playhead_scrub_claimed = state.playhead_dragging;
    if (plot_hovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        playhead_hit) {
        state.playhead_dragging = true;
        state.xline_playing = false;
        state.playhead_fraction = 0.0;
        state.selection_drag_handle = 0;
        playhead_scrub_claimed = true;
    }
    if (state.playhead_dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            set_playhead_index(state, index_from_mouse(), false);
            playhead_scrub_claimed = true;
        } else {
            state.playhead_dragging = false;
        }
    }
    ensure_lazy_blocks(state, first, last);
    ensure_lazy_blocks(state, state.playhead_index, state.playhead_index);

    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(clip_min, clip_max, true);
    draw->AddRectFilled(item_min, item_max, skald::tokens::surface::window);

    const float label_x = child_pos.x + 8.0f;
    const float grid_x0 = child_pos.x + margin_left;
    const float grid_x1 = child_pos.x + child_size.x - 18.0f;
    for (float y_value : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        const float y = y_for_value(y_value, plot_top, plot_bottom, y_zoom);
        draw->AddLine(ImVec2(grid_x0, y), ImVec2(grid_x1, y),
                      y_value == 0.0f ? skald::tokens::border::strong
                                      : skald::tokens::border::separator,
                      y_value == 0.0f ? 1.4f : 1.0f);
        char label[24] = {};
        std::snprintf(label, sizeof(label), "%.1f", static_cast<double>(y_value));
        draw->AddText(ImVec2(label_x, y - 7.0f), skald::tokens::ink::muted, label);
    }

    const std::size_t tick = nice_tick(160.0 / std::max(0.1f, x_step));
    const std::size_t tick_start = first - (first % tick);
    for (std::size_t i = tick_start; i <= last && i < count; i += tick) {
        const float x = plot_left + static_cast<float>(i) * x_step;
        draw->AddLine(ImVec2(x, plot_top), ImVec2(x, plot_bottom),
                      skald::tokens::border::separator, 1.0f);
        char label[32] = {};
        std::snprintf(label, sizeof(label), "%.0f nm", static_cast<double>(i) * period_nm);
        draw->AddText(ImVec2(x + 4.0f, plot_bottom + 8.0f),
                      skald::tokens::ink::muted, label);
    }

    const std::size_t delta_index = std::min(analysis.window.delta_index, count - 1);
    if (delta_index >= first && delta_index <= last) {
        const float x = plot_left + static_cast<float>(delta_index) * x_step;
        draw->AddRectFilled(ImVec2(x - 2.0f, plot_top),
                            ImVec2(x + std::max(2.0f, x_step + 2.0f), plot_bottom),
                            kMaxDeltaFill);
        draw->AddText(ImVec2(x + 6.0f, plot_top + 6.0f),
                      skald::tokens::status::destructive, "max delta");
    }

    if (state.segment.active) {
        clamp_segment(state);
        const auto [selection_first, selection_last] = normalized_selection(state);
        const float start_x = plot_left + static_cast<float>(selection_first) * x_step;
        const float end_x = plot_left + static_cast<float>(selection_last) * x_step;
        draw->AddRectFilled(ImVec2(start_x, plot_top), ImVec2(end_x, plot_bottom),
                            kSelectionFill);
        draw->AddLine(ImVec2(start_x, plot_top), ImVec2(start_x, plot_bottom),
                      kSelectionEdge, 2.0f);
        draw->AddLine(ImVec2(end_x, plot_top), ImVec2(end_x, plot_bottom),
                      kSelectionEdge, 2.0f);
        draw->AddCircleFilled(ImVec2(start_x, plot_top + 8.0f), 4.0f,
                              kSelectionHandle);
        draw->AddCircleFilled(ImVec2(end_x, plot_top + 8.0f), 4.0f,
                              kSelectionHandle);
    }

    if (plot_hovered &&
        !playhead_scrub_claimed &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const std::size_t index = index_from_mouse();
        if (!state.segment_selection_mode) {
            set_playhead_index(state, index, false);
        } else if (!state.segment.active) {
            state.segment.active = true;
            state.segment.selection_start = index;
            state.segment.selection_end = index;
            state.selection_drag_handle = 1;
        } else {
            const auto [selection_first, selection_last] = normalized_selection(state);
            const float start_x = plot_left + static_cast<float>(selection_first) * x_step;
            const float end_x = plot_left + static_cast<float>(selection_last) * x_step;
            const float start_distance = std::abs(mouse.x - start_x);
            const float end_distance = std::abs(mouse.x - end_x);
            state.selection_drag_handle = start_distance <= end_distance ? -1 : 1;
            if (std::min(start_distance, end_distance) > 18.0f) {
                state.segment.selection_start = index;
                state.segment.selection_end = index;
                state.selection_drag_handle = 1;
            }
        }
    }
    if (state.segment_selection_mode && state.selection_drag_handle != 0) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const std::size_t index = index_from_mouse();
            if (state.selection_drag_handle < 0) {
                state.segment.selection_start = index;
            } else {
                state.segment.selection_end = index;
            }
            clamp_segment(state);
        } else {
            state.selection_drag_handle = 0;
        }
    } else if (!state.segment_selection_mode) {
        state.selection_drag_handle = 0;
    }

    const Entity* data = data_entity(state);
    const bool draw_data = data == nullptr || data->visible;
    if (draw_data && state.show_lines) {
        for (std::size_t i = first; i < last && i + 1 < count; ++i) {
            const std::optional<float> scalar_a = scalar_at(state, i);
            const std::optional<float> scalar_b = scalar_at(state, i + 1u);
            if (!scalar_a || !scalar_b) {
                continue;
            }
            const ImVec2 a(plot_left + static_cast<float>(i) * x_step,
                           y_for_value(*scalar_a, plot_top, plot_bottom, y_zoom));
            const ImVec2 b(plot_left + static_cast<float>(i + 1) * x_step,
                           y_for_value(*scalar_b, plot_top, plot_bottom, y_zoom));
            draw->AddLine(a, b, kDataLineColor, 1.2f);
        }
    }
    std::size_t wave_index = 0;
    for (const Entity& entity : state.entities) {
        if (entity.type != EntityType::Wave || !entity.visible || last <= first) {
            continue;
        }
        const ImU32 color = kWaveColors[wave_index % kWaveColors.size()];
        ++wave_index;
        const std::size_t samples = std::min<std::size_t>(4096, last - first + 1u);
        ImVec2 previous{};
        bool has_previous = false;
        for (std::size_t sample = 0; sample < samples; ++sample) {
            const double t = samples > 1
                ? static_cast<double>(sample) / static_cast<double>(samples - 1u)
                : 0.0;
            const double index = static_cast<double>(first) +
                                 t * static_cast<double>(last - first);
            const double x_nm = index * period_nm;
            const double value = wave_value_at_nm(entity.wave, x_nm);
            const ImVec2 point(plot_left + static_cast<float>(index) * x_step,
                               y_for_value(static_cast<float>(value), plot_top,
                                           plot_bottom, y_zoom));
            if (has_previous) {
                draw->AddLine(previous, point, color, 1.5f);
            }
            previous = point;
            has_previous = true;
        }
    }

    if (draw_data && state.show_points) {
        draw_data_point_markers(draw,
                                state,
                                first,
                                last,
                                plot_left,
                                plot_top,
                                plot_bottom,
                                x_step,
                                y_zoom,
                                clip_min,
                                clip_max);
    }

    draw->AddRect(ImVec2(plot_left, plot_top), ImVec2(plot_right, plot_bottom),
                  skald::tokens::border::default_, 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(child_pos.x + margin_left, child_pos.y + 8.0f),
                  skald::tokens::ink::primary, "x-line section");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 178.0f, child_pos.y + 8.0f),
                  kDataLineColor, "data");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 108.0f, child_pos.y + 8.0f),
                  kWaveColors[0], "waves");

    if (state.playhead_index >= first && state.playhead_index <= last) {
        const float playhead_x = plot_left + static_cast<float>(state.playhead_index) * x_step;
        draw->AddLine(ImVec2(playhead_x, plot_top),
                      ImVec2(playhead_x, clip_max.y - 8.0f),
                      kSelectionHandle,
                      2.0f);
        if (const std::optional<float> scalar = scalar_at(state, state.playhead_index)) {
            draw->AddCircleFilled(ImVec2(playhead_x,
                                         y_for_value(*scalar, plot_top, plot_bottom, y_zoom)),
                                  4.0f,
                                  kSelectionHandle);
        }
    }
    draw_data_ticker(state,
                     draw,
                     first,
                     last,
                     plot_left,
                     plot_bottom,
                     x_step,
                     clip_min,
                     clip_max);

    draw->PopClipRect();
    ImGui::EndChild();
    ImGui::End();
}

void navigate_file_dialog_selection(AppState& state) {
    const int row = state.file_dialog.row_sel;
    if (row == state.file_dialog_last_row ||
        row < 0 ||
        row >= static_cast<int>(state.file_dialog_rows.size())) {
        return;
    }

    state.file_dialog_last_row = row;
    const FileBrowserEntry& entry = state.file_dialog_rows[static_cast<std::size_t>(row)];
    if (!entry.is_folder) {
        return;
    }

    std::filesystem::path cwd(state.file_dialog.cwd);
    if (entry.name == "..") {
        cwd = cwd.parent_path();
    } else {
        cwd /= entry.name;
    }
    copy_to_buffer(state.file_dialog.cwd, cwd.lexically_normal().string());
    state.file_dialog.filename[0] = '\0';
    state.file_dialog.row_sel = -1;
    state.file_dialog_last_row = -1;
    refresh_file_dialog_entries(state);
    state.file_dialog_dirty = false;
}

const char* file_dialog_popup_label(DialogPurpose purpose) {
    switch (purpose) {
    case DialogPurpose::OpenSource:
        return "Load Source###thrystr_file_dialog";
    case DialogPurpose::LoadWave:
        return "Open Workspace###thrystr_file_dialog";
    case DialogPurpose::SaveWave:
        return "Save Wave Data###thrystr_file_dialog";
    case DialogPurpose::None:
        break;
    }
    return kFileDialogStableId;
}

skald::FileDialogResult begin_chromed_file_dialog(
    DialogPurpose purpose,
    skald::FileDialogMode mode,
    skald::FileDialogState& dialog,
    std::span<const skald::FileDialogEntry> entries) {
    return skald::BeginFileDialog(file_dialog_popup_label(purpose), mode, dialog, entries);
}

void draw_file_dialog(AppState& state) {
    if (state.pending_dialog != DialogPurpose::None) {
        state.active_dialog = state.pending_dialog;
        state.pending_dialog = DialogPurpose::None;
        skald::OpenFileDialog(file_dialog_popup_label(state.active_dialog));
    }

    if (state.active_dialog == DialogPurpose::None) {
        return;
    }

    if (state.file_dialog_dirty) {
        refresh_file_dialog_entries(state);
        state.file_dialog_dirty = false;
    }
    const std::string cwd_before = state.file_dialog.cwd;
    const std::string filter_before = state.file_dialog.filter;
    const skald::FileDialogMode mode = state.active_dialog == DialogPurpose::SaveWave
        ? skald::FileDialogMode::Save
        : skald::FileDialogMode::Open;
    const skald::FileDialogResult result = begin_chromed_file_dialog(
        state.active_dialog,
        mode,
        state.file_dialog,
        std::span<const skald::FileDialogEntry>(state.file_dialog_entries.data(),
                                                state.file_dialog_entries.size()));

    if (cwd_before != state.file_dialog.cwd || filter_before != state.file_dialog.filter) {
        state.file_dialog_dirty = true;
    }
    navigate_file_dialog_selection(state);

    if (result == skald::FileDialogResult::Cancelled) {
        state.active_dialog = DialogPurpose::None;
        return;
    }
    if (result != skald::FileDialogResult::Confirmed) {
        return;
    }

    if (state.file_dialog.filename[0] == '\0') {
        state.status = "Select a file";
        state.active_dialog = DialogPurpose::None;
        return;
    }

    const std::filesystem::path selected =
        std::filesystem::path(state.file_dialog.cwd) / state.file_dialog.filename;
    try {
        switch (state.active_dialog) {
        case DialogPurpose::OpenSource:
            copy_to_buffer(state.path, selected.lexically_normal().string());
            load_path(state);
            break;
        case DialogPurpose::LoadWave:
            load_wave_settings(state, selected.lexically_normal());
            break;
        case DialogPurpose::SaveWave:
            save_wave_settings(state, selected.lexically_normal());
            break;
        case DialogPurpose::None:
            break;
        }
    } catch (const std::exception& error) {
        state.status = error.what();
    }
    state.active_dialog = DialogPurpose::None;
}

void handle_shortcuts(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput ||
        state.active_dialog != DialogPurpose::None ||
        state.pending_dialog != DialogPurpose::None) {
        return;
    }
    if (state.analysis && source_sample_count(state) > 0u) {
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            toggle_xline_playback(state);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
            move_playhead_by(state, -1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
            move_playhead_by(state, 1);
        }
    }
    const bool shortcut = io.KeyCtrl || io.KeySuper;
    if (shortcut && ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        create_wave_entity(state);
    }
    if (shortcut && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        open_empty_workspace(state);
    }
}

void draw_app(AppState& state, GLFWwindow* window, const ChromeCursors& cursors) {
    handle_shortcuts(state);
    draw_titlebar(state, window);
    draw_plot(state);
    draw_entity_toolbox(state);
    draw_inspector_overlay(state);
    draw_settings_dialog(state);
    draw_file_dialog(state);
    handle_custom_chrome(state, window, cursors);
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    glfwSetErrorCallback(glfw_error);
    prefer_x11_when_available();
    if (!glfwInit()) {
        return 1;
    }

    ChromeCursors chrome_cursors = create_chrome_cursors();
    AppState state;
    initialize_file_dialog(state);

    auto begin_imgui_for_window = [&](GLFWwindow* target) {
        glfwMakeContextCurrent(target);
        glfwSwapInterval(1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        skald::ApplyDefaults(skald::tokens::accents::cyan);
        state.fonts = skald::LoadFonts(io, THRYSTR_SKALD_FONT_DIR);
        ImGui_ImplGlfw_InitForOpenGL(target, true);
        ImGui_ImplOpenGL3_Init("#version 130");
    };

    auto shutdown_imgui = [&]() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.fonts = {};
    };

    auto begin_frame = []() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    };

    auto render_frame = [](GLFWwindow* target) {
        ImGui::Render();
        int fb_width = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(target, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        constexpr ImU32 bg = skald::tokens::surface::deep;
        glClearColor(
            static_cast<float>((bg >> IM_COL32_R_SHIFT) & 0xff) / 255.0f,
            static_cast<float>((bg >> IM_COL32_G_SHIFT) & 0xff) / 255.0f,
            static_cast<float>((bg >> IM_COL32_B_SHIFT) & 0xff) / 255.0f,
            1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(target);
        return std::pair<int, int>{fb_width, fb_height};
    };

    StartupAction startup_action = StartupAction::None;
    bool screenshot_saved = false;
    if (args.file.empty()) {
        GLFWwindow* splash_window = create_undecorated_window(kSplashWindowWidth,
                                                              kSplashWindowHeight,
                                                              "thrystr start",
                                                              kSplashMinWindowWidth,
                                                              kSplashMinWindowHeight,
                                                              false,
                                                              false);
        if (!splash_window) {
            destroy_chrome_cursors(chrome_cursors);
            glfwTerminate();
            return 1;
        }

        begin_imgui_for_window(splash_window);
        const thrystr::Texture2D hero_texture = thrystr::load_rgba_texture(kSplashHeroPath);
        state.splash_hero_texture = hero_texture.id;
        state.splash_hero_size =
            ImVec2(static_cast<float>(hero_texture.width), static_cast<float>(hero_texture.height));
        glfwShowWindow(splash_window);

        int rendered_frames = 0;
        while (!glfwWindowShouldClose(splash_window)) {
            glfwPollEvents();
            begin_frame();

            StartupAction action = draw_splash_window(state, splash_window, chrome_cursors);
            ImGuiIO& io = ImGui::GetIO();
            const bool shortcut = io.KeyCtrl || io.KeySuper;
            if (shortcut && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
                action = StartupAction::NewWorkspace;
            }
            if (state.request_close) {
                glfwSetWindowShouldClose(splash_window, GLFW_TRUE);
            }

            const auto [fb_width, fb_height] = render_frame(splash_window);
            ++rendered_frames;
            if (!args.screenshot.empty() && rendered_frames >= args.frames) {
                if (!thrystr::save_screenshot_png(args.screenshot, fb_width, fb_height)) {
                    std::fprintf(stderr,
                                 "could not write screenshot: %s\n",
                                 args.screenshot.c_str());
                }
                screenshot_saved = true;
                break;
            }
            if (action != StartupAction::None) {
                startup_action = action;
                break;
            }
        }

        thrystr::destroy_texture(state.splash_hero_texture);
        state.splash_hero_size = {};
        shutdown_imgui();
        glfwDestroyWindow(splash_window);
        state.request_close = false;
        state.chrome_action = ChromeAction::None;
        state.splash_open = false;

        if (screenshot_saved) {
            destroy_chrome_cursors(chrome_cursors);
            glfwTerminate();
            return 0;
        }
        if (startup_action == StartupAction::None) {
            destroy_chrome_cursors(chrome_cursors);
            glfwTerminate();
            return 0;
        }

        switch (startup_action) {
        case StartupAction::NewWorkspace:
            open_empty_workspace(state);
            break;
        case StartupAction::OpenWorkspace:
            ensure_workspace(state);
            request_file_dialog(state, DialogPurpose::LoadWave);
            break;
        case StartupAction::LoadSource:
            ensure_workspace(state);
            request_file_dialog(state, DialogPurpose::OpenSource);
            break;
        case StartupAction::None:
            break;
        }
    } else {
        state.splash_open = false;
    }

    std::optional<WindowGeometry> workspace_geometry;
    int workspace_width = args.width;
    int workspace_height = args.height;
    if (!args.size_overridden) {
        workspace_geometry = default_workspace_geometry();
        if (workspace_geometry) {
            workspace_width = workspace_geometry->width;
            workspace_height = workspace_geometry->height;
        }
    }

    GLFWwindow* window = create_undecorated_window(workspace_width,
                                                  workspace_height,
                                                  "thrystr",
                                                  kMinWindowWidth,
                                                  kMinWindowHeight,
                                                  true,
                                                  false);
    if (!window) {
        destroy_chrome_cursors(chrome_cursors);
        glfwTerminate();
        return 1;
    }
    if (workspace_geometry) {
        apply_window_geometry(window, *workspace_geometry);
    }

    begin_imgui_for_window(window);
    if (!args.file.empty()) {
        std::snprintf(state.path, sizeof(state.path), "%s", args.file.c_str());
        load_path(state);
    }
    glfwShowWindow(window);

    int rendered_frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        begin_frame();

        draw_app(state, window, chrome_cursors);
        if (state.request_close) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        const auto [fb_width, fb_height] = render_frame(window);

        ++rendered_frames;
        if (!args.screenshot.empty() && rendered_frames >= args.frames) {
            if (!thrystr::save_screenshot_png(args.screenshot, fb_width, fb_height)) {
                std::fprintf(stderr, "could not write screenshot: %s\n", args.screenshot.c_str());
            }
            break;
        }
    }

    shutdown_imgui();
    destroy_chrome_cursors(chrome_cursors);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
