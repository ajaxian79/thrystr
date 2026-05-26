// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/convergent_fit.hpp>
#include <thrystr/app/fit_validation.hpp>
#include <thrystr/app/multi_track_fit.hpp>
#include <thrystr/gui/image.hpp>
#include <thrystr/gui/interface_session.hpp>
#include <thrystr/gui/native_application.hpp>
#include <thrystr/gui/native_window.hpp>
#include <thrystr/gui/resize_cursor_set.hpp>
#include <thrystr/gui/timeline_draw.hpp>
#include <thrystr/scalar_analysis.hpp>

#include <imgui.h>
#include <thrystr/gui/controls.hpp>
#include <thrystr/gui/file_dialog.hpp>
#include <thrystr/gui/icons.hpp>
#include <thrystr/gui/palette.hpp>
#include <thrystr/gui/splash.hpp>
#if defined(THRYSTR_HAS_DOCS)
#include "docs_resources.hpp"
#endif
#if defined(THRYSTR_HAS_FLOAT128)
#include <quadmath.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace palette = thrystr::gui::palette;

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
constexpr std::size_t kTickerTargetLabelPixels = 58u;
constexpr std::uint64_t kManualScrollAutoscrollInhibitFrames = 90u;
constexpr std::size_t kWaveFitMaxSamples = 4096;
constexpr int kWaveFitWavelengthSteps = 144;
constexpr int kWaveFitPhaseSteps = 128;
constexpr std::size_t kFunctionFitMaxPoints = 512;
constexpr int kFunctionFitSpatialPeriodSteps = 5;
constexpr int kFunctionFitWavelengthSteps = 10;
constexpr int kFunctionFitPhaseSteps = 12;
constexpr int kFunctionFitRotationSteps = 9;
constexpr int kFunctionFitTopSingleCount = 5;
constexpr double kFunctionValueClamp = 8.0;
constexpr int kRandomSampleDefaultCount = 128;
constexpr std::size_t kMaxWaveWavelengthModifiers = 16;
constexpr int kWaveModifierWavelengthSteps = 41;
constexpr std::uintmax_t kLargeSourcePhaseFitDeferBytes = 128ull * 1024ull * 1024ull;
constexpr const char* kSplashHeroPath = THRYSTR_ASSET_DIR "/splash_hero.png";
constexpr const char* kFileDialogStableId = "###thrystr_file_dialog";
constexpr const char* kWaveSettingsExtension = ".thryw";
constexpr std::array<char, 8> kWaveSettingsMagic = {'T', 'H', 'R', 'Y', 'W', 'A', 'V', 'E'};
constexpr std::uint32_t kWaveSettingsVersion = 7;
constexpr std::uint32_t kWaveSettingsEndianStamp = 0x01020304u;
constexpr std::uint32_t kMaxWaveSettingsStringBytes = 1u * 1024u * 1024u;
constexpr std::uint32_t kMaxWaveSettingsItems = 10000u;
constexpr std::uint32_t kMaxWaveSettingsMaskBytes = 512u * 1024u * 1024u;
constexpr std::size_t kAutoFitMaxSamples = 65536u;
constexpr std::size_t kAutoFitActivityMaxItems = 240u;
constexpr std::size_t kMaxFitValidationIssues = 512u;
constexpr std::uint8_t kTrackAutoFitMinDataTracks = 2u;
constexpr std::uint8_t kTrackAutoFitMaxDataTracks =
    static_cast<std::uint8_t>(thrystr::app::kMaxDataTracks);
constexpr std::size_t kEncodedWorkspaceHeaderBytes = 16u;
constexpr std::size_t kEncodedTrackHeaderBytes = 8u;
constexpr std::size_t kEncodedSectionBytes = sizeof(std::uint32_t) * 2u + sizeof(double) * 8u;
constexpr ImU32 kTransparent = IM_COL32(0, 0, 0, 0);

const ImU32 kDataPointHi = palette::with_alpha(palette::ink::primary, 0.92f);
const ImU32 kDataPointMed = palette::with_alpha(palette::ink::primary, 0.86f);
const ImU32 kDataPointLo = palette::with_alpha(palette::ink::primary, 0.73f);
const ImU32 kDataLineColor = palette::with_alpha(palette::status::info, 0.92f);
const ImU32 kMaxDeltaFill = palette::with_alpha(palette::status::destructive, 40.0f / 255.0f);
const ImU32 kSelectionFill = palette::with_alpha(palette::status::info, 32.0f / 255.0f);
const ImU32 kSelectionEdge = palette::with_alpha(palette::status::info, 210.0f / 255.0f);
const ImU32 kSelectionHandle = palette::with_alpha(palette::status::info, 0.92f);
const std::array<ImU32, 6> kWaveColors = {
    palette::with_alpha(palette::accents::gold, 0.86f),
    palette::with_alpha(palette::accents::mint, 0.86f),
    palette::with_alpha(palette::status::destructive, 0.86f),
    palette::with_alpha(palette::accents::violet, 0.86f),
    palette::with_alpha(palette::accents::ember, 0.86f),
    palette::with_alpha(palette::accents::cyan, 0.86f),
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

enum class WaveFunctionKind : std::uint32_t {
    Sine,
    Cosine,
    Tangent,
    Cosecant,
    Secant,
    Cotangent,
    ArcSine,
    ArcCosine,
    ArcTangent,
    ArcCosecant,
    ArcSecant,
    ArcCotangent,
    HyperbolicSine,
    HyperbolicCosine,
    HyperbolicTangent,
    HyperbolicCosecant,
    HyperbolicSecant,
    HyperbolicCotangent,
    InverseHyperbolicSine,
    InverseHyperbolicCosine,
    InverseHyperbolicTangent,
    InverseHyperbolicCosecant,
    InverseHyperbolicSecant,
    InverseHyperbolicCotangent,
    NaturalExponential,
    GeneralExponential,
    NaturalLog,
    CommonLog,
    BinaryLog,
    Square,
    Cube,
    SquareRoot,
    CubeRoot,
    Reciprocal,
    AbsoluteValue,
    Sign,
    Floor,
    Ceiling,
    Round,
    FractionalPart,
    Heaviside,
    Gamma,
    ErrorFunction,
    RiemannZeta,
    BesselJ0,
    Count,
};

enum class PropertyTab {
    Entities,
    Data,
    Wave,
    Fit,
    Points,
    Mappers,
    View,
};

struct DataEntity {
    double spatial_period_nm = 1.0;
    std::uint8_t sample_bits = 8;
};

struct WaveEntity {
    WaveFunctionKind function = WaveFunctionKind::Sine;
    bool use_secondary_function = false;
    WaveFunctionKind secondary_function = WaveFunctionKind::Cosine;
    double wavelength_nm = 64.0;
    double amplitude = 2.0;
    double secondary_amplitude = 0.0;
    double amplitude_offset = -1.0;
    double phase_nm = 0.0;
    double rotation_degrees = 0.0;
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

struct SelectedPointSample {
    std::size_t index = 0;
    double value = 0.0;
};

struct FunctionFitResult {
    bool valid = false;
    WaveFunctionKind function = WaveFunctionKind::Sine;
    bool use_secondary_function = false;
    WaveFunctionKind secondary_function = WaveFunctionKind::Cosine;
    double data_spatial_period_nm = 1.0;
    double wavelength_nm = 64.0;
    double phase_nm = 0.0;
    double rotation_degrees = 0.0;
    double amplitude = 2.0;
    double secondary_amplitude = 0.0;
    double amplitude_offset = -1.0;
    double mean_error = DBL_MAX;
    double max_error = DBL_MAX;
    std::size_t hits = 0;
    std::size_t tested = 0;
};

struct LazyBlock {
    std::size_t index = 0;
    std::size_t offset = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mapped_bytes;
    std::vector<thrystr::app::Scalar> scalars;
    std::uint64_t last_used_frame = 0;
};

struct SegmentFitPatch {
    std::size_t start_offset = 0;
    std::size_t sample_count = 0;
    std::size_t segment_index = 0;
    thrystr::app::WorkspaceModel workspace;
    thrystr::app::ValidationReport validation;
    bool used_parity = false;
    bool cancelled = false;
};

struct EncodingEstimate {
    std::size_t source_bytes = 0;
    std::size_t covered_samples = 0;
    std::size_t encoded_bytes = 0;
    std::size_t curve_points = 0;
    std::size_t data_tracks = 0;
    std::size_t data_sections = 0;
    std::size_t parity_sections = 0;
    double source_ratio = 0.0;
    double covered_ratio = 0.0;
};

struct AutoFitJob {
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> done{false};
    std::atomic<bool> cancel_requested{false};
    std::mutex result_mutex;
    std::vector<thrystr::app::Section> sections;
    thrystr::app::WorkspaceModel workspace;
    thrystr::app::ValidationReport validation;
    std::vector<SegmentFitPatch> pending_patches;
    bool cancelled = false;
    bool multi_track = false;
    bool used_parity = false;
    bool partial_dirty = false;
    std::size_t start_offset = 0;
    std::size_t requested_last = 0;
    std::size_t capped_last = 0;
    std::size_t processed_samples = 0;
    std::size_t total_samples = 0;
    std::size_t completed_segments = 0;
    std::size_t total_segments = 0;
    std::size_t active_segment_index = 0;
    std::size_t active_segment_start = 0;
    std::size_t active_segment_length = 0;
    std::string progress_label;
    std::string active_operation;
    std::vector<std::string> activity_log;
    std::string error;
};

struct AutoFitProgressSnapshot {
    bool running = false;
    bool multi_track = false;
    bool cancelled = false;
    std::size_t processed_samples = 0;
    std::size_t total_samples = 0;
    std::size_t completed_segments = 0;
    std::size_t total_segments = 0;
    std::size_t active_segment_index = 0;
    std::size_t active_segment_start = 0;
    std::size_t active_segment_length = 0;
    std::string progress_label;
    std::string active_operation;
    std::string error;
    std::vector<std::string> activity_log;
};

struct Args {
    std::string file;
    std::string screenshot;
    int width = 1600;
    int height = 900;
    int frames = 0;
    bool size_overridden = false;
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

bool parse_non_negative_int(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    int parsed = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc() || result.ptr != last || parsed < 0) {
        return false;
    }
    value = parsed;
    return true;
}

struct AppState {
    char path[4096] = {};
    char wave_path[4096] = {};
    char entity_name[128] = {};
    std::optional<thrystr::Analysis> analysis;
    std::string status = "Empty workspace";
    std::vector<thrystr::ValueMapper> mappers;
    std::vector<Entity> entities;
    std::vector<thrystr::app::Section> fitted_sections;
    thrystr::app::WorkspaceModel fitted_workspace;
    bool fitted_workspace_valid = false;
    bool fitted_workspace_used_parity = false;
    thrystr::app::ValidationReport fit_validation;
    AutoFitJob auto_fit_job;
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
    bool show_reconstruction_only = false;
    bool show_fit_validation = false;
    bool show_docs = false;
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
    bool point_selection_mode = false;
    PropertyTab property_tab = PropertyTab::Entities;
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
    std::ifstream lazy_source_stream;
    std::filesystem::path lazy_source_stream_path;
    std::uint64_t plot_frame = 0;
    std::uint64_t last_manual_scroll_frame = 0;
    std::size_t playhead_index = 0;
    double playhead_fraction = 0.0;
    double playback_points_per_second = kDefaultPlaybackPointsPerSecond;
    bool custom_playback_speed = false;
    char custom_playback_speed_text[32] = "60";
    bool xline_playing = false;
    bool playhead_dragging = false;
    std::vector<std::size_t> selected_data_points;
    int random_sample_count = kRandomSampleDefaultCount;
    std::uint64_t random_sample_seed = 0x544852595354ull;
    bool fit_function_combinations = true;
    int docs_page = 0;
    char docs_search[128] = {};
    std::optional<std::size_t> pending_scroll_index;
    thrystr::gui::FileDialogState file_dialog{};
    std::vector<FileBrowserEntry> file_dialog_rows;
    std::vector<thrystr::gui::FileDialogEntry> file_dialog_entries;
    std::vector<float> plot_min_y;
    std::vector<float> plot_max_y;
    std::vector<ImVec2> plot_points;
    int file_dialog_last_row = -1;
    bool file_dialog_dirty = true;
    DialogPurpose pending_dialog = DialogPurpose::None;
    DialogPurpose active_dialog = DialogPurpose::None;
    thrystr::gui::FontSet fonts{};
    unsigned int splash_hero_texture = 0;
    ImVec2 splash_hero_size{};
};

const Entity* data_entity(const AppState& state);
Entity* data_entity(AppState& state);
bool playback_speed_button(const char* label, bool selected, float width = 42.0f);

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
                const std::string value = argv[++i];
                if (parse_non_negative_int(value, dst)) {
                    return true;
                }
                std::fprintf(stderr, "warning: ignoring invalid value for %s: %s\n", flag,
                             value.c_str());
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

bool chrome_action_is_resize(ChromeAction action) {
    return action != ChromeAction::None && action != ChromeAction::Move;
}

bool chrome_action_has_left(ChromeAction action) {
    return action == ChromeAction::ResizeLeft || action == ChromeAction::ResizeTopLeft ||
           action == ChromeAction::ResizeBottomLeft;
}

bool chrome_action_has_right(ChromeAction action) {
    return action == ChromeAction::ResizeRight || action == ChromeAction::ResizeTopRight ||
           action == ChromeAction::ResizeBottomRight;
}

bool chrome_action_has_top(ChromeAction action) {
    return action == ChromeAction::ResizeTop || action == ChromeAction::ResizeTopLeft ||
           action == ChromeAction::ResizeTopRight;
}

bool chrome_action_has_bottom(ChromeAction action) {
    return action == ChromeAction::ResizeBottom || action == ChromeAction::ResizeBottomLeft ||
           action == ChromeAction::ResizeBottomRight;
}

ChromeAction resize_action_at(double cursor_x, double cursor_y, int window_w, int window_h) {
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

void* cursor_for_action(const thrystr::gui::ResizeCursorSet& cursors, ChromeAction action) {
    switch (action) {
    case ChromeAction::ResizeLeft:
    case ChromeAction::ResizeRight:
        return cursors.cursor(thrystr::gui::ResizeCursor::Horizontal);
    case ChromeAction::ResizeTop:
    case ChromeAction::ResizeBottom:
        return cursors.cursor(thrystr::gui::ResizeCursor::Vertical);
    case ChromeAction::ResizeTopLeft:
    case ChromeAction::ResizeBottomRight:
        return cursors.cursor(thrystr::gui::ResizeCursor::DiagonalDown);
    case ChromeAction::ResizeTopRight:
    case ChromeAction::ResizeBottomLeft:
        return cursors.cursor(thrystr::gui::ResizeCursor::DiagonalUp);
    case ChromeAction::Move:
    case ChromeAction::None:
        return nullptr;
    }
    return nullptr;
}

std::pair<double, double> global_cursor_position(thrystr::gui::WindowHandle window, double cursor_x,
                                                 double cursor_y) {
    const thrystr::gui::CursorPoint point =
        thrystr::gui::global_cursor_position(window, {cursor_x, cursor_y});
    return {point.x, point.y};
}

void begin_chrome_action(AppState& state, thrystr::gui::WindowHandle window, ChromeAction action,
                         double cursor_x, double cursor_y) {
    state.chrome_action = action;
    const auto [global_x, global_y] = global_cursor_position(window, cursor_x, cursor_y);
    state.chrome_start_global_x = global_x;
    state.chrome_start_global_y = global_y;
    const thrystr::gui::Point position = thrystr::gui::window_position(window);
    const thrystr::gui::Size size = thrystr::gui::window_size(window);
    state.chrome_start_window_x = position.x;
    state.chrome_start_window_y = position.y;
    state.chrome_start_window_w = size.width;
    state.chrome_start_window_h = size.height;
}

void update_chrome_action(AppState& state, thrystr::gui::WindowHandle window, double cursor_x,
                          double cursor_y) {
    const auto [global_x, global_y] = global_cursor_position(window, cursor_x, cursor_y);
    const int dx = static_cast<int>(std::lround(global_x - state.chrome_start_global_x));
    const int dy = static_cast<int>(std::lround(global_y - state.chrome_start_global_y));

    if (state.chrome_action == ChromeAction::Move) {
        thrystr::gui::move_window(
            window, {state.chrome_start_window_x + dx, state.chrome_start_window_y + dy});
        return;
    }

    int x = state.chrome_start_window_x;
    int y = state.chrome_start_window_y;
    int w = state.chrome_start_window_w;
    int h = state.chrome_start_window_h;

    if (chrome_action_has_left(state.chrome_action)) {
        const int right = state.chrome_start_window_x + state.chrome_start_window_w;
        w = state.chrome_start_window_w - dx;
        if (w < kMinWindowWidth) {
            w = kMinWindowWidth;
        }
        x = right - w;
    } else if (chrome_action_has_right(state.chrome_action)) {
        w = std::max(kMinWindowWidth, state.chrome_start_window_w + dx);
    }

    if (chrome_action_has_top(state.chrome_action)) {
        const int bottom = state.chrome_start_window_y + state.chrome_start_window_h;
        h = state.chrome_start_window_h - dy;
        if (h < kMinWindowHeight) {
            h = kMinWindowHeight;
        }
        y = bottom - h;
    } else if (chrome_action_has_bottom(state.chrome_action)) {
        h = std::max(kMinWindowHeight, state.chrome_start_window_h + dy);
    }

    thrystr::gui::move_resize_window(window, {{x, y}, {w, h}});
}

void handle_custom_chrome(AppState& state, thrystr::gui::WindowHandle window,
                          const thrystr::gui::ResizeCursorSet& cursors, bool allow_resize = true) {
    if (!window || thrystr::gui::is_iconified(window)) {
        return;
    }

    const thrystr::gui::CursorPoint cursor = thrystr::gui::cursor_position(window);
    const thrystr::gui::Size size = thrystr::gui::window_size(window);

    if (state.chrome_action != ChromeAction::None) {
        thrystr::gui::set_cursor(window, cursor_for_action(cursors, state.chrome_action));
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            update_chrome_action(state, window, cursor.x, cursor.y);
        } else {
            state.chrome_action = ChromeAction::None;
            thrystr::gui::set_cursor(window, nullptr);
        }
        return;
    }

    if (thrystr::gui::is_maximized(window)) {
        thrystr::gui::set_cursor(window, nullptr);
        return;
    }

    const ChromeAction resize_action =
        allow_resize ? resize_action_at(cursor.x, cursor.y, size.width, size.height)
                     : ChromeAction::None;
    const bool resize_hovered = chrome_action_is_resize(resize_action);
    const bool titlebar_hovered =
        cursor.y >= 0.0 && cursor.y <= static_cast<double>(kTopChromeHeight);
    const bool item_hovered = ImGui::IsAnyItemHovered();
    const ChromeAction hover_action =
        resize_hovered
            ? resize_action
            : (titlebar_hovered && !item_hovered ? ChromeAction::Move : ChromeAction::None);

    thrystr::gui::set_cursor(window, cursor_for_action(cursors, hover_action));

    if (hover_action != ChromeAction::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        begin_chrome_action(state, window, hover_action, cursor.x, cursor.y);
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
        std::snprintf(buffer, sizeof(buffer), "%ju %s", static_cast<std::uintmax_t>(bytes),
                      units[unit]);
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

std::string format_scalar(double value) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.6f", value);
    return buffer;
}

std::string format_percent(double ratio) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2f%%", ratio * 100.0);
    return buffer;
}

template <std::size_t N> void copy_to_buffer(char (&buffer)[N], const std::string& value) {
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

    std::sort(rows.begin(), rows.end(),
              [](const FileBrowserEntry& left, const FileBrowserEntry& right) {
                  if (left.is_folder != right.is_folder) {
                      return left.is_folder;
                  }
                  return left.name < right.name;
              });

    state.file_dialog_rows.insert(state.file_dialog_rows.end(), rows.begin(), rows.end());
    state.file_dialog_entries.reserve(state.file_dialog_rows.size());
    for (const FileBrowserEntry& row : state.file_dialog_rows) {
        state.file_dialog_entries.push_back(thrystr::gui::FileDialogEntry{
            row.name,
            row.size,
            row.modified,
            row.is_folder,
        });
    }
}

std::size_t lazy_block_bytes(const AppState& state) {
    const int mib = std::clamp(state.lazy_block_mib, 1, 10);
    return std::clamp(static_cast<std::size_t>(mib) * 1024u * 1024u, kLazyBlockMinBytes,
                      kLazyBlockMaxBytes);
}

std::uint8_t normalize_sample_bits(std::uint8_t bits) {
    switch (bits) {
    case 8:
    case 16:
    case 32:
    case 64:
        return bits;
    default:
        return 8;
    }
}

std::uint8_t data_sample_bits(const AppState& state) {
    const Entity* data = data_entity(state);
    return data ? normalize_sample_bits(data->data.sample_bits) : 8u;
}

std::size_t data_sample_bytes(const AppState& state) {
    return static_cast<std::size_t>(data_sample_bits(state) / 8u);
}

std::size_t lazy_block_samples(const AppState& state) {
    return std::max<std::size_t>(1u, lazy_block_bytes(state) / data_sample_bytes(state));
}

std::size_t source_sample_count(const AppState& state) {
    if (!state.analysis) {
        return 0u;
    }
    const std::size_t sample_bytes = data_sample_bytes(state);
    if (state.analysis->source_size > 0) {
        const std::size_t byte_count = static_cast<std::size_t>(std::min<std::uintmax_t>(
            state.analysis->source_size,
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())));
        return byte_count / sample_bytes;
    }
    return state.analysis->scalars.size();
}

thrystr::app::Scalar scalar_ldexp(thrystr::app::Scalar value, int exponent) {
#if defined(THRYSTR_HAS_FLOAT128)
    return ldexpq(value, exponent);
#else
    return std::ldexp(value, exponent);
#endif
}

bool scalar_isfinite(thrystr::app::Scalar value) {
#if defined(THRYSTR_HAS_FLOAT128)
    return finiteq(value) != 0;
#else
    return std::isfinite(value);
#endif
}

thrystr::app::Scalar scalar_trunc(thrystr::app::Scalar value) {
#if defined(THRYSTR_HAS_FLOAT128)
    return truncq(value);
#else
    return std::trunc(value);
#endif
}

thrystr::app::Scalar scalar_fmod(thrystr::app::Scalar value, thrystr::app::Scalar divisor) {
#if defined(THRYSTR_HAS_FLOAT128)
    return fmodq(value, divisor);
#else
    return std::fmod(value, divisor);
#endif
}

std::optional<std::uint64_t> sample_from_bytes(std::span<const std::uint8_t> bytes,
                                               std::size_t sample_index, std::size_t sample_bytes) {
    const std::size_t byte_offset = sample_index * sample_bytes;
    if (sample_bytes == 0u || byte_offset + sample_bytes > bytes.size() || sample_bytes > 8u) {
        return std::nullopt;
    }

    std::uint64_t value = 0u;
    for (std::size_t i = 0; i < sample_bytes; ++i) {
        value |= static_cast<std::uint64_t>(bytes[byte_offset + i]) << (i * 8u);
    }
    return value;
}

thrystr::app::Scalar apply_value_mappers_scalar(thrystr::app::Scalar value,
                                                std::span<const thrystr::ValueMapper> mappers) {
    for (std::size_t index = 0; index < mappers.size(); ++index) {
        const thrystr::ValueMapper& mapper = mappers[index];
        if (!mapper.enabled) {
            continue;
        }

        const thrystr::app::Scalar operand = static_cast<thrystr::app::Scalar>(mapper.operand);
        switch (mapper.kind) {
        case thrystr::ValueMapperKind::Add:
            value += operand;
            break;
        case thrystr::ValueMapperKind::Subtract:
            value -= operand;
            break;
        case thrystr::ValueMapperKind::Multiply:
            value *= operand;
            break;
        case thrystr::ValueMapperKind::Divide:
            if (std::abs(mapper.operand) <= 1.0e-12) {
                throw std::invalid_argument("divide mapper operand must not be zero");
            }
            value /= operand;
            break;
        }

        if (!scalar_isfinite(value)) {
            throw std::invalid_argument("mapper stack produced a non-finite value at stage " +
                                        std::to_string(index));
        }
    }
    return value;
}

std::uint64_t wrap_sample_value(thrystr::app::Scalar value, std::uint8_t sample_bits) {
    if (!scalar_isfinite(value)) {
        throw std::invalid_argument("cannot wrap non-finite sample value");
    }
    const thrystr::app::Scalar modulus =
        scalar_ldexp(static_cast<thrystr::app::Scalar>(1.0), sample_bits);
    thrystr::app::Scalar wrapped = scalar_fmod(scalar_trunc(value), modulus);
    if (wrapped < static_cast<thrystr::app::Scalar>(0.0)) {
        wrapped += modulus;
    }
    if (wrapped <= static_cast<thrystr::app::Scalar>(0.0)) {
        return 0u;
    }
    const std::uint64_t max_value = sample_bits == 64u ? std::numeric_limits<std::uint64_t>::max()
                                                       : ((1ull << sample_bits) - 1ull);
    const thrystr::app::Scalar max_scalar = static_cast<thrystr::app::Scalar>(max_value);
    if (wrapped >= max_scalar) {
        return max_value;
    }
    return static_cast<std::uint64_t>(wrapped);
}

std::uint64_t map_sample_to_wrapped(std::uint64_t sample, std::uint8_t sample_bits,
                                    std::span<const thrystr::ValueMapper> mappers) {
    const thrystr::app::Scalar mapped =
        apply_value_mappers_scalar(static_cast<thrystr::app::Scalar>(sample), mappers);
    return wrap_sample_value(mapped, sample_bits);
}

thrystr::app::Scalar sample_to_scalar(std::uint64_t sample, std::uint8_t sample_bits) {
    const thrystr::app::Scalar denominator =
        scalar_ldexp(static_cast<thrystr::app::Scalar>(1.0), sample_bits - 1);
    return static_cast<thrystr::app::Scalar>(sample) / denominator -
           static_cast<thrystr::app::Scalar>(1.0);
}

std::vector<thrystr::app::Scalar>
map_source_bytes_to_scalars(std::span<const std::uint8_t> bytes, std::uint8_t sample_bits,
                            std::span<const thrystr::ValueMapper> mappers) {
    sample_bits = normalize_sample_bits(sample_bits);
    const std::size_t sample_bytes = static_cast<std::size_t>(sample_bits / 8u);
    const std::size_t sample_count = bytes.size() / sample_bytes;
    std::vector<thrystr::app::Scalar> scalars;
    scalars.reserve(sample_count);
    for (std::size_t i = 0; i < sample_count; ++i) {
        const std::optional<std::uint64_t> raw = sample_from_bytes(bytes, i, sample_bytes);
        if (!raw) {
            break;
        }
        const std::uint64_t mapped = map_sample_to_wrapped(*raw, sample_bits, mappers);
        scalars.push_back(sample_to_scalar(mapped, sample_bits));
    }
    return scalars;
}

void clear_lazy_cache(AppState& state) {
    state.lazy_blocks.clear();
    state.lazy_frame = 0;
    if (state.lazy_source_stream.is_open()) {
        state.lazy_source_stream.close();
    }
    state.lazy_source_stream_path.clear();
}

std::vector<std::uint8_t> read_file_slice(const std::filesystem::path& path, std::size_t offset,
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

std::vector<std::uint8_t> read_file_slice_from_stream(std::ifstream& input,
                                                      const std::filesystem::path& path,
                                                      std::size_t offset, std::size_t length) {
    std::vector<std::uint8_t> bytes(length);
    if (length == 0u) {
        return bytes;
    }

    input.clear();
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

std::vector<std::uint8_t> read_lazy_file_slice(AppState& state, const std::filesystem::path& path,
                                               std::size_t offset, std::size_t length) {
    std::vector<std::uint8_t> bytes(length);
    if (length == 0u) {
        return bytes;
    }

    if (!state.lazy_source_stream.is_open() || state.lazy_source_stream_path != path) {
        if (state.lazy_source_stream.is_open()) {
            state.lazy_source_stream.close();
        }
        state.lazy_source_stream.clear();
        state.lazy_source_stream.open(path, std::ios::binary);
        state.lazy_source_stream_path = path;
    }
    if (!state.lazy_source_stream) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    state.lazy_source_stream.clear();
    state.lazy_source_stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!state.lazy_source_stream) {
        throw std::runtime_error("could not seek file: " + path.string());
    }
    state.lazy_source_stream.read(reinterpret_cast<char*>(bytes.data()),
                                  static_cast<std::streamsize>(bytes.size()));
    const std::streamsize read_count = state.lazy_source_stream.gcount();
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
        const auto oldest =
            std::min_element(state.lazy_blocks.begin(), state.lazy_blocks.end(),
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
    const std::size_t sample_bytes = data_sample_bytes(state);
    const std::size_t block_samples = lazy_block_samples(state);
    const std::size_t sample_offset = block_index * block_samples;
    if (sample_offset >= count) {
        return;
    }
    const std::size_t sample_length = std::min(block_samples, count - sample_offset);
    const std::size_t byte_offset = sample_offset * sample_bytes;
    const std::size_t byte_length = sample_length * sample_bytes;

    LazyBlock block;
    block.index = block_index;
    block.offset = sample_offset;
    block.bytes =
        read_lazy_file_slice(state, state.analysis->source_path, byte_offset, byte_length);
    if (data_sample_bits(state) == 8u) {
        block.mapped_bytes = thrystr::map_bytes_to_wrapped(block.bytes, state.mappers);
    }
    block.scalars =
        map_source_bytes_to_scalars(block.bytes, data_sample_bits(state), state.mappers);
    block.last_used_frame = state.lazy_frame;
    state.lazy_blocks.push_back(std::move(block));
    trim_lazy_cache(state);
}

void seed_lazy_cache_from_analysis(AppState& state) {
    clear_lazy_cache(state);
    if (!state.analysis || state.analysis->bytes.empty()) {
        return;
    }

    LazyBlock block;
    block.index = 0u;
    block.offset = 0u;
    block.bytes = state.analysis->bytes;
    block.mapped_bytes = state.analysis->mapped_bytes;
    block.scalars =
        map_source_bytes_to_scalars(block.bytes, data_sample_bits(state), state.mappers);
    block.last_used_frame = ++state.lazy_frame;
    state.lazy_blocks.push_back(std::move(block));
    trim_lazy_cache(state);
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
    const std::size_t block_samples = lazy_block_samples(state);
    const std::size_t first_block = first / block_samples;
    const std::size_t last_block = last / block_samples;
    const std::size_t prefetch_first = first_block == 0u ? 0u : first_block - 1u;
    const std::size_t prefetch_last = std::min((count - 1u) / block_samples, last_block + 1u);
    for (std::size_t block = prefetch_first; block <= prefetch_last; ++block) {
        load_lazy_block(state, block);
    }
}

std::optional<std::uint64_t> raw_sample_at(const AppState& state, std::size_t index) {
    if (!state.analysis || index >= source_sample_count(state)) {
        return std::nullopt;
    }

    const std::size_t sample_bytes = data_sample_bytes(state);
    const std::size_t block_samples = lazy_block_samples(state);
    const std::size_t block_index = index / block_samples;
    if (const LazyBlock* block = find_lazy_block(state, block_index)) {
        if (index >= block->offset) {
            const std::size_t local = index - block->offset;
            if (const std::optional<std::uint64_t> sample =
                    sample_from_bytes(block->bytes, local, sample_bytes)) {
                return sample;
            }
        }
    }

    if (index < state.analysis->bytes.size() / sample_bytes) {
        if (const std::optional<std::uint64_t> sample =
                sample_from_bytes(state.analysis->bytes, index, sample_bytes)) {
            return sample;
        }
    }
    return std::nullopt;
}

std::optional<thrystr::app::Scalar> scalar_at(const AppState& state, std::size_t index) {
    if (!state.analysis || index >= source_sample_count(state)) {
        return std::nullopt;
    }

    const std::size_t block_samples = lazy_block_samples(state);
    const std::size_t block_index = index / block_samples;
    if (const LazyBlock* block = find_lazy_block(state, block_index)) {
        if (index >= block->offset) {
            const std::size_t local = index - block->offset;
            if (local < block->scalars.size()) {
                return block->scalars[local];
            }
        }
    }

    if (index < state.analysis->bytes.size() / data_sample_bytes(state)) {
        const std::vector<thrystr::app::Scalar> scalars = map_source_bytes_to_scalars(
            state.analysis->bytes, data_sample_bits(state), state.mappers);
        if (index < scalars.size()) {
            return scalars[index];
        }
    }
    return std::nullopt;
}

std::uint64_t sample_from_scalar(thrystr::app::Scalar scalar, std::uint8_t sample_bits) {
    const thrystr::app::Scalar scaled =
        (scalar + static_cast<thrystr::app::Scalar>(1.0)) *
        scalar_ldexp(static_cast<thrystr::app::Scalar>(1.0), sample_bits - 1);
    if (scaled <= 0.0) {
        return 0u;
    }
    if (sample_bits == 64u &&
        scaled >= static_cast<thrystr::app::Scalar>(std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    const auto rounded = static_cast<std::uint64_t>(static_cast<long double>(scaled));
    const std::uint64_t max_value = sample_bits == 64u ? std::numeric_limits<std::uint64_t>::max()
                                                       : ((1ull << sample_bits) - 1ull);
    return std::min(rounded, max_value);
}

std::string hex_sample_text(std::uint64_t sample, std::uint8_t sample_bits) {
    char buffer[24] = {};
    const int digits = sample_bits / 4;
    std::snprintf(buffer, sizeof(buffer), "%0*llX", digits,
                  static_cast<unsigned long long>(sample));
    return buffer;
}

template <typename T> void write_binary(std::ostream& output, const T& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!output) {
        throw std::runtime_error("could not write wave settings");
    }
}

template <typename T> T read_binary(std::istream& input) {
    T value{};
    input.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!input) {
        throw std::runtime_error("could not read wave settings");
    }
    return value;
}

template <typename T> void write_binary_le(std::ostream& output, const T& value) {
    std::array<unsigned char, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(T));
    if constexpr (std::endian::native == std::endian::big) {
        std::reverse(bytes.begin(), bytes.end());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        throw std::runtime_error("could not write wave settings");
    }
}

template <typename T> T read_binary_le(std::istream& input) {
    std::array<unsigned char, sizeof(T)> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("could not read wave settings");
    }
    if constexpr (std::endian::native == std::endian::big) {
        std::reverse(bytes.begin(), bytes.end());
    }
    T value{};
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
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

void write_string_le(std::ostream& output, const std::string& value) {
    if (value.size() > kMaxWaveSettingsStringBytes) {
        throw std::runtime_error("wave settings string too large");
    }
    const auto length = static_cast<std::uint32_t>(value.size());
    write_binary_le(output, length);
    output.write(value.data(), static_cast<std::streamsize>(length));
    if (!output) {
        throw std::runtime_error("could not write wave settings string");
    }
}

std::string read_string_le(std::istream& input) {
    const std::uint32_t length = read_binary_le<std::uint32_t>(input);
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
    return type == EntityType::Data ? "raw data" : "function";
}

constexpr std::array<WaveFunctionKind, static_cast<std::size_t>(WaveFunctionKind::Count)>
    kWaveFunctionKinds = {
        WaveFunctionKind::Sine,
        WaveFunctionKind::Cosine,
        WaveFunctionKind::Tangent,
        WaveFunctionKind::Cosecant,
        WaveFunctionKind::Secant,
        WaveFunctionKind::Cotangent,
        WaveFunctionKind::ArcSine,
        WaveFunctionKind::ArcCosine,
        WaveFunctionKind::ArcTangent,
        WaveFunctionKind::ArcCosecant,
        WaveFunctionKind::ArcSecant,
        WaveFunctionKind::ArcCotangent,
        WaveFunctionKind::HyperbolicSine,
        WaveFunctionKind::HyperbolicCosine,
        WaveFunctionKind::HyperbolicTangent,
        WaveFunctionKind::HyperbolicCosecant,
        WaveFunctionKind::HyperbolicSecant,
        WaveFunctionKind::HyperbolicCotangent,
        WaveFunctionKind::InverseHyperbolicSine,
        WaveFunctionKind::InverseHyperbolicCosine,
        WaveFunctionKind::InverseHyperbolicTangent,
        WaveFunctionKind::InverseHyperbolicCosecant,
        WaveFunctionKind::InverseHyperbolicSecant,
        WaveFunctionKind::InverseHyperbolicCotangent,
        WaveFunctionKind::NaturalExponential,
        WaveFunctionKind::GeneralExponential,
        WaveFunctionKind::NaturalLog,
        WaveFunctionKind::CommonLog,
        WaveFunctionKind::BinaryLog,
        WaveFunctionKind::Square,
        WaveFunctionKind::Cube,
        WaveFunctionKind::SquareRoot,
        WaveFunctionKind::CubeRoot,
        WaveFunctionKind::Reciprocal,
        WaveFunctionKind::AbsoluteValue,
        WaveFunctionKind::Sign,
        WaveFunctionKind::Floor,
        WaveFunctionKind::Ceiling,
        WaveFunctionKind::Round,
        WaveFunctionKind::FractionalPart,
        WaveFunctionKind::Heaviside,
        WaveFunctionKind::Gamma,
        WaveFunctionKind::ErrorFunction,
        WaveFunctionKind::RiemannZeta,
        WaveFunctionKind::BesselJ0,
};

WaveFunctionKind normalize_wave_function_kind(std::uint32_t raw) {
    if (raw >= static_cast<std::uint32_t>(WaveFunctionKind::Count)) {
        return WaveFunctionKind::Sine;
    }
    return static_cast<WaveFunctionKind>(raw);
}

const char* wave_function_name(WaveFunctionKind kind) {
    switch (kind) {
    case WaveFunctionKind::Sine:
        return "sine";
    case WaveFunctionKind::Cosine:
        return "cosine";
    case WaveFunctionKind::Tangent:
        return "tangent";
    case WaveFunctionKind::Cosecant:
        return "cosecant";
    case WaveFunctionKind::Secant:
        return "secant";
    case WaveFunctionKind::Cotangent:
        return "cotangent";
    case WaveFunctionKind::ArcSine:
        return "arcsine";
    case WaveFunctionKind::ArcCosine:
        return "arccosine";
    case WaveFunctionKind::ArcTangent:
        return "arctangent";
    case WaveFunctionKind::ArcCosecant:
        return "arccosecant";
    case WaveFunctionKind::ArcSecant:
        return "arcsecant";
    case WaveFunctionKind::ArcCotangent:
        return "arccotangent";
    case WaveFunctionKind::HyperbolicSine:
        return "hyperbolic sine";
    case WaveFunctionKind::HyperbolicCosine:
        return "hyperbolic cosine";
    case WaveFunctionKind::HyperbolicTangent:
        return "hyperbolic tangent";
    case WaveFunctionKind::HyperbolicCosecant:
        return "hyperbolic cosecant";
    case WaveFunctionKind::HyperbolicSecant:
        return "hyperbolic secant";
    case WaveFunctionKind::HyperbolicCotangent:
        return "hyperbolic cotangent";
    case WaveFunctionKind::InverseHyperbolicSine:
        return "inverse hyperbolic sine";
    case WaveFunctionKind::InverseHyperbolicCosine:
        return "inverse hyperbolic cosine";
    case WaveFunctionKind::InverseHyperbolicTangent:
        return "inverse hyperbolic tangent";
    case WaveFunctionKind::InverseHyperbolicCosecant:
        return "inverse hyperbolic cosecant";
    case WaveFunctionKind::InverseHyperbolicSecant:
        return "inverse hyperbolic secant";
    case WaveFunctionKind::InverseHyperbolicCotangent:
        return "inverse hyperbolic cotangent";
    case WaveFunctionKind::NaturalExponential:
        return "natural exponential";
    case WaveFunctionKind::GeneralExponential:
        return "general exponential";
    case WaveFunctionKind::NaturalLog:
        return "natural log";
    case WaveFunctionKind::CommonLog:
        return "common log";
    case WaveFunctionKind::BinaryLog:
        return "binary log";
    case WaveFunctionKind::Square:
        return "square";
    case WaveFunctionKind::Cube:
        return "cube";
    case WaveFunctionKind::SquareRoot:
        return "square root";
    case WaveFunctionKind::CubeRoot:
        return "cube root";
    case WaveFunctionKind::Reciprocal:
        return "reciprocal";
    case WaveFunctionKind::AbsoluteValue:
        return "absolute value";
    case WaveFunctionKind::Sign:
        return "sign";
    case WaveFunctionKind::Floor:
        return "floor";
    case WaveFunctionKind::Ceiling:
        return "ceiling";
    case WaveFunctionKind::Round:
        return "round";
    case WaveFunctionKind::FractionalPart:
        return "fractional part";
    case WaveFunctionKind::Heaviside:
        return "heaviside";
    case WaveFunctionKind::Gamma:
        return "gamma";
    case WaveFunctionKind::ErrorFunction:
        return "error function";
    case WaveFunctionKind::RiemannZeta:
        return "riemann zeta";
    case WaveFunctionKind::BesselJ0:
        return "bessel j0";
    case WaveFunctionKind::Count:
        break;
    }
    return "sine";
}

double clamp_finite_function_value(double value, double limit = kFunctionValueClamp) {
    if (!std::isfinite(value)) {
        return value < 0.0 ? -limit : limit;
    }
    return std::clamp(value, -limit, limit);
}

double nonzero_for_division(double value) {
    if (std::abs(value) < 1.0e-9) {
        return value < 0.0 ? -1.0e-9 : 1.0e-9;
    }
    return value;
}

double approximate_riemann_zeta(double s) {
    s = std::clamp(s, 1.05, 12.0);
    double sum = 0.0;
    for (int n = 1; n <= 96; ++n) {
        sum += std::pow(static_cast<double>(n), -s);
    }
    return sum;
}

double standard_function_raw(WaveFunctionKind kind, double unit_x) {
    const double theta = 2.0 * std::numbers::pi * unit_x;
    const double bounded = std::clamp(std::sin(theta), -0.999999, 0.999999);
    const double limited_unit = std::clamp(unit_x, -6.0, 6.0);
    const double hyperbolic_unit = std::clamp(unit_x, -3.0, 3.0);
    const double positive = std::abs(unit_x) + 1.0e-6;
    const double positive_one = std::abs(unit_x) + 1.0;
    const double inverse_domain = std::copysign(1.0 + std::abs(std::sin(theta)),
                                                std::sin(theta) == 0.0 ? 1.0 : std::sin(theta));

    switch (kind) {
    case WaveFunctionKind::Sine:
        return std::sin(theta);
    case WaveFunctionKind::Cosine:
        return std::cos(theta);
    case WaveFunctionKind::Tangent:
        return std::tan(theta);
    case WaveFunctionKind::Cosecant:
        return 1.0 / nonzero_for_division(std::sin(theta));
    case WaveFunctionKind::Secant:
        return 1.0 / nonzero_for_division(std::cos(theta));
    case WaveFunctionKind::Cotangent:
        return 1.0 / nonzero_for_division(std::tan(theta));
    case WaveFunctionKind::ArcSine:
        return std::asin(bounded);
    case WaveFunctionKind::ArcCosine:
        return std::acos(bounded);
    case WaveFunctionKind::ArcTangent:
        return std::atan(unit_x);
    case WaveFunctionKind::ArcCosecant:
        return std::asin(1.0 / inverse_domain);
    case WaveFunctionKind::ArcSecant:
        return std::acos(1.0 / inverse_domain);
    case WaveFunctionKind::ArcCotangent:
        return std::atan(1.0 / nonzero_for_division(unit_x));
    case WaveFunctionKind::HyperbolicSine:
        return std::sinh(hyperbolic_unit);
    case WaveFunctionKind::HyperbolicCosine:
        return std::cosh(hyperbolic_unit);
    case WaveFunctionKind::HyperbolicTangent:
        return std::tanh(hyperbolic_unit);
    case WaveFunctionKind::HyperbolicCosecant:
        return 1.0 / nonzero_for_division(std::sinh(hyperbolic_unit));
    case WaveFunctionKind::HyperbolicSecant:
        return 1.0 / nonzero_for_division(std::cosh(hyperbolic_unit));
    case WaveFunctionKind::HyperbolicCotangent:
        return 1.0 / nonzero_for_division(std::tanh(hyperbolic_unit));
    case WaveFunctionKind::InverseHyperbolicSine:
        return std::asinh(unit_x);
    case WaveFunctionKind::InverseHyperbolicCosine:
        return std::acosh(positive_one);
    case WaveFunctionKind::InverseHyperbolicTangent:
        return std::atanh(bounded);
    case WaveFunctionKind::InverseHyperbolicCosecant:
        return std::asinh(1.0 / nonzero_for_division(unit_x));
    case WaveFunctionKind::InverseHyperbolicSecant: {
        const double domain = std::clamp(0.05 + 0.95 * std::abs(std::sin(theta)), 0.000001, 1.0);
        return std::acosh(1.0 / domain);
    }
    case WaveFunctionKind::InverseHyperbolicCotangent: {
        const double domain = std::copysign(1.0 + std::abs(unit_x), unit_x == 0.0 ? 1.0 : unit_x);
        return std::atanh(1.0 / domain);
    }
    case WaveFunctionKind::NaturalExponential:
        return std::exp(limited_unit);
    case WaveFunctionKind::GeneralExponential:
        return std::pow(2.0, std::clamp(unit_x, -8.0, 8.0));
    case WaveFunctionKind::NaturalLog:
        return std::log(positive);
    case WaveFunctionKind::CommonLog:
        return std::log10(positive);
    case WaveFunctionKind::BinaryLog:
        return std::log2(positive);
    case WaveFunctionKind::Square:
        return unit_x * unit_x;
    case WaveFunctionKind::Cube:
        return unit_x * unit_x * unit_x;
    case WaveFunctionKind::SquareRoot:
        return std::sqrt(positive);
    case WaveFunctionKind::CubeRoot:
        return std::cbrt(unit_x);
    case WaveFunctionKind::Reciprocal:
        return 1.0 / nonzero_for_division(unit_x);
    case WaveFunctionKind::AbsoluteValue:
        return std::abs(unit_x);
    case WaveFunctionKind::Sign:
        return unit_x > 0.0 ? 1.0 : (unit_x < 0.0 ? -1.0 : 0.0);
    case WaveFunctionKind::Floor:
        return std::floor(unit_x);
    case WaveFunctionKind::Ceiling:
        return std::ceil(unit_x);
    case WaveFunctionKind::Round:
        return std::round(unit_x);
    case WaveFunctionKind::FractionalPart:
        return unit_x - std::floor(unit_x);
    case WaveFunctionKind::Heaviside:
        return unit_x >= 0.0 ? 1.0 : 0.0;
    case WaveFunctionKind::Gamma:
        return std::tgamma(std::clamp(positive, 0.1, 6.0));
    case WaveFunctionKind::ErrorFunction:
        return std::erf(unit_x);
    case WaveFunctionKind::RiemannZeta:
        return approximate_riemann_zeta(std::abs(unit_x) + 1.05);
    case WaveFunctionKind::BesselJ0:
        return std::cyl_bessel_j(0.0, theta);
    case WaveFunctionKind::Count:
        break;
    }
    return std::sin(theta);
}

double rotated_function_raw(WaveFunctionKind kind, double unit_x, double rotation_degrees) {
    const double raw_y = clamp_finite_function_value(standard_function_raw(kind, unit_x));
    if (std::abs(rotation_degrees) <= 1.0e-9) {
        return raw_y;
    }

    const double radians = rotation_degrees * std::numbers::pi / 180.0;
    const double rotated_y = unit_x * std::sin(radians) + raw_y * std::cos(radians);
    return clamp_finite_function_value(rotated_y);
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

std::size_t scalar_count(const AppState& state) { return source_sample_count(state); }

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
        state.selection_drag_handle = 0;
        return;
    }
    state.segment.selection_start = std::min(state.segment.selection_start, count - 1u);
    state.segment.selection_end = std::min(state.segment.selection_end, count - 1u);
    if (!state.segment.active) {
        return;
    }

    const std::size_t min_length = std::min<std::size_t>(count, thrystr::app::kMinSegmentSize);
    const std::size_t first = std::min(state.segment.selection_start, state.segment.selection_end);
    const std::size_t last = std::max(state.segment.selection_start, state.segment.selection_end);
    if (last - first + 1u >= min_length) {
        return;
    }

    if (state.selection_drag_handle < 0) {
        const std::size_t anchored_end = last;
        const std::size_t expanded_start =
            anchored_end + 1u >= min_length ? anchored_end + 1u - min_length : 0u;
        state.segment.selection_start = expanded_start;
        state.segment.selection_end = anchored_end;
    } else {
        const std::size_t anchored_start = first;
        std::size_t expanded_end = std::min(count - 1u, anchored_start + min_length - 1u);
        std::size_t expanded_start = anchored_start;
        if (expanded_end - expanded_start + 1u < min_length) {
            expanded_start = expanded_end + 1u >= min_length ? expanded_end + 1u - min_length : 0u;
        }
        state.segment.selection_start = expanded_start;
        state.segment.selection_end = expanded_end;
    }
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
    state.fitted_sections.clear();
    state.fitted_workspace = {};
    state.fitted_workspace_valid = false;
    state.fitted_workspace_used_parity = false;
    state.fit_validation = {};
    state.segment = {};
    state.path[0] = '\0';
    state.selected_entity_id = 0;
    state.entity_name_id = 0;
    state.wheel_scroll_mode = true;
    state.segment_selection_mode = false;
    state.point_selection_mode = false;
    state.property_tab = PropertyTab::Entities;
    state.timeline_scroll_x = 0.0f;
    state.xline_playing = false;
    state.playhead_dragging = false;
    state.playhead_index = 0;
    state.playhead_fraction = 0.0;
    state.pending_scroll_index.reset();
    state.selected_data_points.clear();
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
    entity.name = state.path[0] == '\0' ? std::string("data")
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
    entity.name =
        name.empty() ? "function " + std::to_string(state.wave_serial++) : std::move(name);
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

double wave_function_basis_at_nm(const WaveEntity& wave, WaveFunctionKind function, double x_nm) {
    const double wavelength = wave_wavelength_at_nm(wave, x_nm);
    const double unit_x = (x_nm - wave.phase_nm) / wavelength;
    const double raw_y = rotated_function_raw(function, unit_x, wave.rotation_degrees);
    return (raw_y + 1.0) * 0.5;
}

double wave_value_at_nm(const WaveEntity& wave, double x_nm) {
    double value = wave.amplitude_offset +
                   wave.amplitude * wave_function_basis_at_nm(wave, wave.function, x_nm);
    if (wave.use_secondary_function) {
        value += wave.secondary_amplitude *
                 wave_function_basis_at_nm(wave, wave.secondary_function, x_nm);
    }
    if (!std::isfinite(value)) {
        return value < 0.0 ? -1.0e6 : 1.0e6;
    }
    return std::clamp(value, -1.0e6, 1.0e6);
}

WaveFitResult score_wave_on_range(const AppState& state, const WaveEntity& wave, std::size_t first,
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
        const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, index);
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
        const double t =
            kWaveFitWavelengthSteps > 1
                ? static_cast<double>(wi) / static_cast<double>(kWaveFitWavelengthSteps - 1)
                : 0.0;
        const double wavelength = log_lerp(min_wavelength, max_wavelength, t);
        for (int pi = 0; pi < kWaveFitPhaseSteps; ++pi) {
            const double phase_nm =
                wavelength * static_cast<double>(pi) / static_cast<double>(kWaveFitPhaseSteps);
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
        const std::size_t segment_last = first + (count * (segment + 1u)) / segments - 1u;
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
                                 ? static_cast<double>(step) /
                                       static_cast<double>(kWaveModifierWavelengthSteps - 1)
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
    const WaveFitResult final_fit =
        score_wave_on_range(state, wave.wave, analysis_selection_bounds(state).first,
                            analysis_selection_bounds(state).second);
    state.status = "Created interpolated function: " + format_count(final_fit.hits) + "/" +
                   format_count(final_fit.tested) + " point hits, " +
                   format_count(wave.wave.wavelength_modifiers.size()) + " wavelength keys";
}

void normalize_selected_data_points(AppState& state) {
    const std::size_t count = source_sample_count(state);
    if (count == 0u) {
        state.selected_data_points.clear();
        return;
    }
    for (std::size_t& index : state.selected_data_points) {
        index = std::min(index, count - 1u);
    }
    std::sort(state.selected_data_points.begin(), state.selected_data_points.end());
    state.selected_data_points.erase(
        std::unique(state.selected_data_points.begin(), state.selected_data_points.end()),
        state.selected_data_points.end());
}

void toggle_selected_data_point(AppState& state, std::size_t index) {
    if (!state.analysis || index >= source_sample_count(state)) {
        return;
    }
    normalize_selected_data_points(state);
    const auto found = std::lower_bound(state.selected_data_points.begin(),
                                        state.selected_data_points.end(), index);
    if (found != state.selected_data_points.end() && *found == index) {
        state.selected_data_points.erase(found);
        state.status = "Removed point " + format_count(index) + " from fit selection";
    } else {
        state.selected_data_points.insert(found, index);
        state.status = "Selected point " + format_count(index) + " for function fit";
    }
}

std::vector<SelectedPointSample> collect_selected_point_samples(AppState& state) {
    normalize_selected_data_points(state);
    std::vector<SelectedPointSample> samples;
    if (!state.analysis || state.selected_data_points.empty()) {
        return samples;
    }

    std::vector<std::size_t> indices = state.selected_data_points;
    if (indices.size() > kFunctionFitMaxPoints) {
        std::vector<std::size_t> reduced;
        reduced.reserve(kFunctionFitMaxPoints);
        for (std::size_t i = 0; i < kFunctionFitMaxPoints; ++i) {
            const std::size_t source_index =
                (i * (indices.size() - 1u)) / (kFunctionFitMaxPoints - 1u);
            reduced.push_back(indices[source_index]);
        }
        indices = std::move(reduced);
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    }

    samples.reserve(indices.size());
    for (std::size_t index : indices) {
        ensure_lazy_blocks(state, index, index);
        if (const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, index)) {
            samples.push_back({index, static_cast<double>(*scalar)});
        }
    }
    return samples;
}

std::uint64_t next_random_sample_seed(std::uint64_t value) {
    return value * 6364136223846793005ull + 1442695040888963407ull;
}

void select_random_sample_points(AppState& state) {
    const std::size_t count = source_sample_count(state);
    if (!state.analysis || count == 0u) {
        state.status = "Load source data before sampling points";
        return;
    }

    const int requested = std::clamp(state.random_sample_count, 1, 4096);
    state.random_sample_count = requested;
    std::uint64_t seed =
        state.random_sample_seed == 0u ? 0x544852595354ull : state.random_sample_seed;
    std::vector<std::size_t> points;
    points.reserve(static_cast<std::size_t>(requested));
    for (int i = 0; i < requested; ++i) {
        seed = next_random_sample_seed(seed);
        points.push_back(static_cast<std::size_t>(seed % count));
    }
    state.random_sample_seed = seed;
    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());
    state.selected_data_points = std::move(points);
    state.point_selection_mode = true;
    state.property_tab = PropertyTab::Points;
    state.status =
        "Selected " + format_count(state.selected_data_points.size()) + " random sample points";
}

double function_basis_with_params(WaveFunctionKind function, std::size_t point_index,
                                  double data_spatial_period_nm, double wavelength_nm,
                                  double phase_nm, double rotation_degrees) {
    const double x_nm = static_cast<double>(point_index) * data_spatial_period_nm;
    const double unit_x = (x_nm - phase_nm) / std::max(0.000001, wavelength_nm);
    const double raw_y = rotated_function_raw(function, unit_x, rotation_degrees);
    return (raw_y + 1.0) * 0.5;
}

bool solve_normal_equations(double normal[3][3], double rhs[3], int dimension,
                            double coefficients[3]) {
    double matrix[3][4] = {};
    for (int row = 0; row < dimension; ++row) {
        for (int column = 0; column < dimension; ++column) {
            matrix[row][column] = normal[row][column];
        }
        matrix[row][dimension] = rhs[row];
    }

    for (int pivot = 0; pivot < dimension; ++pivot) {
        int pivot_row = pivot;
        double pivot_abs = std::abs(matrix[pivot][pivot]);
        for (int row = pivot + 1; row < dimension; ++row) {
            const double candidate_abs = std::abs(matrix[row][pivot]);
            if (candidate_abs > pivot_abs) {
                pivot_row = row;
                pivot_abs = candidate_abs;
            }
        }
        if (pivot_abs < 1.0e-10) {
            return false;
        }
        if (pivot_row != pivot) {
            for (int column = pivot; column <= dimension; ++column) {
                std::swap(matrix[pivot][column], matrix[pivot_row][column]);
            }
        }
        const double divisor = matrix[pivot][pivot];
        for (int column = pivot; column <= dimension; ++column) {
            matrix[pivot][column] /= divisor;
        }
        for (int row = 0; row < dimension; ++row) {
            if (row == pivot) {
                continue;
            }
            const double factor = matrix[row][pivot];
            for (int column = pivot; column <= dimension; ++column) {
                matrix[row][column] -= factor * matrix[pivot][column];
            }
        }
    }

    for (int row = 0; row < dimension; ++row) {
        coefficients[row] = matrix[row][dimension];
        if (!std::isfinite(coefficients[row])) {
            return false;
        }
    }
    return true;
}

FunctionFitResult score_function_fit(const std::vector<SelectedPointSample>& samples,
                                     WaveFunctionKind function,
                                     std::optional<WaveFunctionKind> secondary_function,
                                     double data_spatial_period_nm, double wavelength_nm,
                                     double phase_nm, double rotation_degrees, double tolerance) {
    FunctionFitResult result;
    result.function = function;
    result.use_secondary_function = secondary_function.has_value();
    result.secondary_function = secondary_function.value_or(WaveFunctionKind::Cosine);
    result.data_spatial_period_nm = data_spatial_period_nm;
    result.wavelength_nm = wavelength_nm;
    result.phase_nm = phase_nm;
    result.rotation_degrees = rotation_degrees;
    result.tested = samples.size();
    if (samples.size() < 2u) {
        return result;
    }

    const int dimension = secondary_function ? 3 : 2;
    double normal[3][3] = {};
    double rhs[3] = {};
    for (const SelectedPointSample& sample : samples) {
        const double primary =
            function_basis_with_params(function, sample.index, data_spatial_period_nm,
                                       wavelength_nm, phase_nm, rotation_degrees);
        const double secondary =
            secondary_function ? function_basis_with_params(*secondary_function, sample.index,
                                                            data_spatial_period_nm, wavelength_nm,
                                                            phase_nm, rotation_degrees)
                               : 0.0;
        const double features[3] = {1.0, primary, secondary};
        for (int row = 0; row < dimension; ++row) {
            rhs[row] += features[row] * sample.value;
            for (int column = 0; column < dimension; ++column) {
                normal[row][column] += features[row] * features[column];
            }
        }
    }

    double coefficients[3] = {};
    if (!solve_normal_equations(normal, rhs, dimension, coefficients)) {
        return result;
    }

    result.valid = true;
    result.amplitude_offset = coefficients[0];
    result.amplitude = coefficients[1];
    result.secondary_amplitude = secondary_function ? coefficients[2] : 0.0;
    double abs_error_sum = 0.0;
    double max_error = 0.0;
    for (const SelectedPointSample& sample : samples) {
        const double primary =
            function_basis_with_params(function, sample.index, data_spatial_period_nm,
                                       wavelength_nm, phase_nm, rotation_degrees);
        const double secondary =
            secondary_function ? function_basis_with_params(*secondary_function, sample.index,
                                                            data_spatial_period_nm, wavelength_nm,
                                                            phase_nm, rotation_degrees)
                               : 0.0;
        const double estimate = result.amplitude_offset + result.amplitude * primary +
                                (secondary_function ? result.secondary_amplitude * secondary : 0.0);
        const double error = std::abs(estimate - sample.value);
        abs_error_sum += error;
        max_error = std::max(max_error, error);
        if (error <= tolerance) {
            ++result.hits;
        }
    }
    result.mean_error = abs_error_sum / static_cast<double>(samples.size());
    result.max_error = max_error;
    return result;
}

bool function_fit_is_better(const FunctionFitResult& candidate, const FunctionFitResult& best) {
    if (!candidate.valid) {
        return false;
    }
    if (!best.valid) {
        return true;
    }
    if (candidate.hits != best.hits) {
        return candidate.hits > best.hits;
    }
    if (std::abs(candidate.max_error - best.max_error) > 1.0e-12) {
        return candidate.max_error < best.max_error;
    }
    if (std::abs(candidate.mean_error - best.mean_error) > 1.0e-12) {
        return candidate.mean_error < best.mean_error;
    }
    return !candidate.use_secondary_function && best.use_secondary_function;
}

FunctionFitResult scan_function_fit_grid(const std::vector<SelectedPointSample>& samples,
                                         WaveFunctionKind function,
                                         std::optional<WaveFunctionKind> secondary_function,
                                         double current_period_nm, double tolerance) {
    FunctionFitResult best;
    if (samples.size() < 2u) {
        return best;
    }

    const std::size_t first_index = samples.front().index;
    const std::size_t last_index = samples.back().index;
    for (int si = 0; si < kFunctionFitSpatialPeriodSteps; ++si) {
        const double st =
            kFunctionFitSpatialPeriodSteps > 1
                ? static_cast<double>(si) / static_cast<double>(kFunctionFitSpatialPeriodSteps - 1)
                : 0.5;
        const double period_nm = log_lerp(current_period_nm * 0.1, current_period_nm * 10.0, st);
        const double span_nm =
            std::max(period_nm, static_cast<double>(last_index - first_index + 1u) * period_nm);
        const double min_wavelength = std::max(0.000001, std::min(period_nm, span_nm) / 16.0);
        const double max_wavelength =
            std::max({min_wavelength * 2.0, period_nm * 256.0, span_nm * 4.0});
        const double anchor_nm = static_cast<double>(first_index) * period_nm;
        for (int wi = 0; wi < kFunctionFitWavelengthSteps; ++wi) {
            const double wt =
                kFunctionFitWavelengthSteps > 1
                    ? static_cast<double>(wi) / static_cast<double>(kFunctionFitWavelengthSteps - 1)
                    : 0.0;
            const double wavelength_nm = log_lerp(min_wavelength, max_wavelength, wt);
            for (int pi = 0; pi < kFunctionFitPhaseSteps; ++pi) {
                const double pt =
                    kFunctionFitPhaseSteps > 1
                        ? static_cast<double>(pi) / static_cast<double>(kFunctionFitPhaseSteps - 1)
                        : 0.0;
                const double phase_nm = anchor_nm - wavelength_nm * 0.5 + wavelength_nm * pt;
                for (int ri = 0; ri < kFunctionFitRotationSteps; ++ri) {
                    const double rt = kFunctionFitRotationSteps > 1
                                          ? static_cast<double>(ri) /
                                                static_cast<double>(kFunctionFitRotationSteps - 1)
                                          : 0.5;
                    const double rotation_degrees = -90.0 + 180.0 * rt;
                    const FunctionFitResult score =
                        score_function_fit(samples, function, secondary_function, period_nm,
                                           wavelength_nm, phase_nm, rotation_degrees, tolerance);
                    if (function_fit_is_better(score, best)) {
                        best = score;
                    }
                }
            }
        }
    }
    return best;
}

FunctionFitResult fit_function_to_selected_points(AppState& state) {
    const std::vector<SelectedPointSample> samples = collect_selected_point_samples(state);
    FunctionFitResult best;
    if (samples.size() < 2u) {
        return best;
    }

    const double period_nm = data_spatial_period_nm(state);
    const double tolerance = std::max(0.000001, static_cast<double>(state.wave_tolerance));
    std::vector<FunctionFitResult> best_singles;
    best_singles.reserve(kWaveFunctionKinds.size());
    for (WaveFunctionKind function : kWaveFunctionKinds) {
        FunctionFitResult score =
            scan_function_fit_grid(samples, function, std::nullopt, period_nm, tolerance);
        if (score.valid) {
            best_singles.push_back(score);
        }
        if (function_fit_is_better(score, best)) {
            best = score;
        }
    }

    std::sort(best_singles.begin(), best_singles.end(),
              [](const FunctionFitResult& left, const FunctionFitResult& right) {
                  return function_fit_is_better(left, right);
              });

    if (state.fit_function_combinations && best_singles.size() >= 2u) {
        const std::size_t candidate_count =
            std::min<std::size_t>(best_singles.size(), kFunctionFitTopSingleCount);
        for (std::size_t a = 0; a < candidate_count; ++a) {
            for (std::size_t b = a + 1u; b < candidate_count; ++b) {
                FunctionFitResult score =
                    scan_function_fit_grid(samples, best_singles[a].function,
                                           best_singles[b].function, period_nm, tolerance);
                if (function_fit_is_better(score, best)) {
                    best = score;
                }
            }
        }
    }
    return best;
}

void apply_function_fit_result(AppState& state, Entity& entity, const FunctionFitResult& fit) {
    if (!fit.valid || entity.type != EntityType::Wave) {
        return;
    }
    if (Entity* data = data_entity(state)) {
        data->data.spatial_period_nm = std::max(0.000001, fit.data_spatial_period_nm);
    }
    entity.wave.function = fit.function;
    entity.wave.use_secondary_function = fit.use_secondary_function;
    entity.wave.secondary_function = fit.secondary_function;
    entity.wave.wavelength_nm = fit.wavelength_nm;
    entity.wave.amplitude = fit.amplitude;
    entity.wave.secondary_amplitude = fit.secondary_amplitude;
    entity.wave.amplitude_offset = fit.amplitude_offset;
    entity.wave.phase_nm = fit.phase_nm;
    entity.wave.rotation_degrees = fit.rotation_degrees;
    entity.wave.wavelength_modifiers.clear();
}

void fit_selected_points_to_wave(AppState& state) {
    if (state.selected_data_points.size() < 2u) {
        state.status = "Select at least two data points before fitting a function";
        return;
    }

    Entity* selected = find_entity(state, state.selected_entity_id);
    if (!selected || selected->type != EntityType::Wave) {
        selected =
            &create_wave_entity(state, "function fit " + std::to_string(state.wave_serial++));
    }

    state.status = "Fitting function candidates over selected points";
    const FunctionFitResult fit = fit_function_to_selected_points(state);
    if (!fit.valid) {
        state.status = "No stable function fit found for selected points";
        return;
    }

    apply_function_fit_result(state, *selected, fit);
    state.property_tab = PropertyTab::Wave;
    std::string function_label = wave_function_name(fit.function);
    if (fit.use_secondary_function) {
        function_label += " + ";
        function_label += wave_function_name(fit.secondary_function);
    }
    state.status = "Fitted " + selected->name + " with " + function_label + ": " +
                   format_count(fit.hits) + "/" + format_count(fit.tested) +
                   " selected points, max error " + std::to_string(fit.max_error);
}

std::vector<thrystr::app::Scalar> collect_scalars_for_range(AppState& state, std::size_t first,
                                                            std::size_t last) {
    std::vector<thrystr::app::Scalar> scalars;
    const std::size_t count = source_sample_count(state);
    if (!state.analysis || count == 0u || last < first) {
        return scalars;
    }
    first = std::min(first, count - 1u);
    last = std::min(last, count - 1u);
    scalars.reserve(last - first + 1u);
    const std::size_t block_size = lazy_block_samples(state);
    for (std::size_t block_first = first; block_first <= last;) {
        const std::size_t block_last =
            std::min(last, ((block_first / block_size) + 1u) * block_size - 1u);
        ensure_lazy_blocks(state, block_first, block_last);
        for (std::size_t index = block_first; index <= block_last; ++index) {
            scalars.push_back(scalar_at(state, index).value_or(0.0f));
        }
        if (block_last == last) {
            break;
        }
        block_first = block_last + 1u;
    }
    return scalars;
}

struct TrackAutoFitRange {
    std::size_t start = 0;
    std::size_t length = 0;
};

std::vector<TrackAutoFitRange> build_track_auto_fit_ranges(std::size_t sample_count,
                                                           std::size_t target_length) {
    std::vector<TrackAutoFitRange> ranges;
    if (sample_count == 0u) {
        return ranges;
    }

    const std::size_t min_length = std::min(sample_count, thrystr::app::kMinSegmentSize);
    target_length = std::max(min_length, target_length);
    if (sample_count <= target_length) {
        ranges.push_back({0u, sample_count});
        return ranges;
    }

    std::size_t start = 0u;
    while (sample_count - start > target_length + min_length) {
        ranges.push_back({start, target_length});
        start += target_length;
    }
    ranges.push_back({start, sample_count - start});
    return ranges;
}

thrystr::app::WorkspaceModel make_track_auto_fit_workspace(std::uint8_t data_track_count) {
    thrystr::app::WorkspaceModel workspace;
    workspace.active_track_id = 0u;
    workspace.parity_track_id = data_track_count;
    workspace.tracks.reserve(static_cast<std::size_t>(data_track_count) + 1u);

    for (std::uint8_t id = 0u; id < data_track_count; ++id) {
        thrystr::app::Track track;
        track.id = id;
        track.kind = thrystr::app::TrackKind::Data;
        track.name = "data track " + std::to_string(static_cast<unsigned>(id));
        track.visible = false;
        workspace.tracks.push_back(std::move(track));
    }

    thrystr::app::Track parity;
    parity.id = data_track_count;
    parity.kind = thrystr::app::TrackKind::Parity;
    parity.name = "parity";
    workspace.tracks.push_back(std::move(parity));
    return workspace;
}

thrystr::app::Section offset_section(thrystr::app::Section section, std::size_t offset) {
    section.start_index += static_cast<std::uint32_t>(offset);
    return section;
}

void merge_owned_mask(std::span<std::uint8_t> global_mask, std::span<const std::uint8_t> local_mask,
                      std::size_t start_offset, std::size_t local_count) {
    if (global_mask.empty() || local_mask.empty() || local_count == 0u) {
        return;
    }

    if (start_offset % 8u == 0u) {
        const std::size_t byte_offset = start_offset / 8u;
        if (byte_offset >= global_mask.size()) {
            return;
        }
        const std::size_t byte_count =
            std::min(local_mask.size(), global_mask.size() - byte_offset);
        for (std::size_t i = 0; i < byte_count; ++i) {
            global_mask[byte_offset + i] |= local_mask[i];
        }
        return;
    }

    for (std::size_t local_index = 0; local_index < local_count; ++local_index) {
        if (thrystr::app::get_owned_bit(local_mask, local_index)) {
            thrystr::app::set_owned_bit(global_mask, start_offset + local_index, true);
        }
    }
}

void accumulate_validation(thrystr::app::ValidationReport& aggregate,
                           const thrystr::app::ValidationReport& segment,
                           std::size_t start_offset) {
    const double residual_sum =
        aggregate.mean_residual * static_cast<double>(aggregate.checked_samples) +
        segment.mean_residual * static_cast<double>(segment.checked_samples);
    aggregate.max_residual = std::max(aggregate.max_residual, segment.max_residual);
    aggregate.checked_samples += segment.checked_samples;
    aggregate.mean_residual = aggregate.checked_samples == 0u
                                  ? 0.0
                                  : residual_sum / static_cast<double>(aggregate.checked_samples);
    if (!segment.pass) {
        aggregate.pass = false;
    }
    for (thrystr::app::ValidationIssue issue : segment.issues) {
        if (aggregate.issues.size() >= kMaxFitValidationIssues) {
            break;
        }
        issue.sample_index += start_offset;
        aggregate.issues.push_back(std::move(issue));
    }
}

void apply_segment_fit_patch(AppState& state, SegmentFitPatch&& patch) {
    if (!state.fitted_workspace_valid || state.fitted_workspace.tracks.empty()) {
        state.fitted_workspace = make_track_auto_fit_workspace(kTrackAutoFitMaxDataTracks);
        state.fitted_workspace_valid = true;
        state.fitted_workspace_used_parity = true;
    }

    for (const thrystr::app::Track& patch_track : patch.workspace.tracks) {
        const std::uint8_t target_id = patch_track.kind == thrystr::app::TrackKind::Parity
                                           ? state.fitted_workspace.parity_track_id
                                           : patch_track.id;
        thrystr::app::Track* target = thrystr::app::find_track(state.fitted_workspace, target_id);
        if (!target) {
            continue;
        }

        if (patch_track.kind == thrystr::app::TrackKind::Parity) {
            for (const thrystr::app::Section& section : patch_track.sections) {
                target->sections.push_back(offset_section(section, patch.start_offset));
            }
            continue;
        }

        if (patch_track.kind != thrystr::app::TrackKind::Data) {
            continue;
        }
        if (target->owned_mask.empty()) {
            thrystr::app::reset_owned_mask(target->owned_mask, source_sample_count(state));
        }
        for (const thrystr::app::Section& section : patch_track.sections) {
            const thrystr::app::Section global_section =
                offset_section(section, patch.start_offset);
            target->sections.push_back(global_section);
            state.fitted_sections.push_back(global_section);
        }
        if (!patch_track.sections.empty()) {
            target->visible = true;
        }
        merge_owned_mask(target->owned_mask, patch_track.owned_mask, patch.start_offset,
                         patch.sample_count);
    }

    state.fitted_workspace_used_parity =
        state.fitted_workspace.parity_track_id != thrystr::app::kNoParityTrack;
    accumulate_validation(state.fit_validation, patch.validation, patch.start_offset);
}

std::size_t data_track_count(const thrystr::app::WorkspaceModel& workspace) {
    return static_cast<std::size_t>(std::count_if(
        workspace.tracks.begin(), workspace.tracks.end(), [](const thrystr::app::Track& track) {
            return track.kind == thrystr::app::TrackKind::Data && !track.sections.empty();
        }));
}

std::size_t data_track_span(const thrystr::app::WorkspaceModel& workspace) {
    std::size_t span = 0u;
    for (const thrystr::app::Track& track : workspace.tracks) {
        if (track.kind != thrystr::app::TrackKind::Data || track.sections.empty()) {
            continue;
        }
        span = std::max(span, static_cast<std::size_t>(track.id) + 1u);
    }
    return span;
}

EncodingEstimate estimate_encoded_output_size(const AppState& state) {
    EncodingEstimate estimate;
    estimate.source_bytes =
        state.analysis ? static_cast<std::size_t>(std::min<std::uintmax_t>(
                             state.analysis->source_size,
                             static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())))
                       : 0u;
    estimate.covered_samples = state.fit_validation.checked_samples;

    if (!state.fitted_workspace_valid || state.fitted_workspace.tracks.empty()) {
        return estimate;
    }

    estimate.encoded_bytes = kEncodedWorkspaceHeaderBytes;
    for (const thrystr::app::Track& track : state.fitted_workspace.tracks) {
        if (track.kind == thrystr::app::TrackKind::Data && track.sections.empty()) {
            continue;
        }
        if (track.kind == thrystr::app::TrackKind::Parity && track.sections.empty()) {
            continue;
        }
        estimate.encoded_bytes += kEncodedTrackHeaderBytes;
        estimate.encoded_bytes += track.sections.size() * kEncodedSectionBytes;
        estimate.curve_points += track.sections.size() * 2u;
        if (track.kind == thrystr::app::TrackKind::Data) {
            ++estimate.data_tracks;
            estimate.data_sections += track.sections.size();
        } else if (track.kind == thrystr::app::TrackKind::Parity) {
            estimate.parity_sections += track.sections.size();
        }
    }

    if (estimate.covered_samples == 0u) {
        for (const thrystr::app::Section& section : state.fitted_sections) {
            estimate.covered_samples += section.length;
        }
    }
    if (estimate.source_bytes > 0u) {
        estimate.source_ratio = static_cast<double>(estimate.encoded_bytes) /
                                static_cast<double>(estimate.source_bytes);
    }
    const std::size_t covered_bytes = estimate.covered_samples * data_sample_bytes(state);
    if (covered_bytes > 0u) {
        estimate.covered_ratio =
            static_cast<double>(estimate.encoded_bytes) / static_cast<double>(covered_bytes);
    }
    return estimate;
}

const char* encoding_target_label(double ratio) {
    if (ratio <= 0.0) {
        return "no fit";
    }
    if (ratio < 0.05) {
        return "under target";
    }
    if (ratio <= 0.20) {
        return "target";
    }
    return "over target";
}

void append_auto_fit_activity_locked(AutoFitJob& job, std::string message) {
    job.activity_log.push_back(std::move(message));
    if (job.activity_log.size() > kAutoFitActivityMaxItems) {
        job.activity_log.erase(
            job.activity_log.begin(),
            job.activity_log.begin() +
                static_cast<std::ptrdiff_t>(job.activity_log.size() - kAutoFitActivityMaxItems));
    }
}

void append_auto_fit_activity(AutoFitJob& job, std::string message) {
    std::scoped_lock lock(job.result_mutex);
    append_auto_fit_activity_locked(job, std::move(message));
}

void store_auto_fit_stage(AutoFitJob& job, std::size_t segment_index,
                          const TrackAutoFitRange& range, std::string operation,
                          bool append_log = true) {
    std::scoped_lock lock(job.result_mutex);
    job.active_segment_index = segment_index;
    job.active_segment_start = range.start;
    job.active_segment_length = range.length;
    job.active_operation = std::move(operation);
    job.progress_label = "segment " + format_count(segment_index + 1u) + "/" +
                         format_count(std::max<std::size_t>(1u, job.total_segments));
    if (append_log) {
        append_auto_fit_activity_locked(job, job.progress_label + ": " + job.active_operation +
                                                 " [" + format_count(range.start) + ".." +
                                                 format_count(range.start + range.length - 1u) +
                                                 "]");
    }
    job.partial_dirty = true;
}

void clear_auto_fit_activity(AutoFitJob& job) {
    job.active_segment_index = 0u;
    job.active_segment_start = 0u;
    job.active_segment_length = 0u;
    job.progress_label.clear();
    job.active_operation.clear();
    job.activity_log.clear();
}

std::size_t workspace_section_count(const thrystr::app::WorkspaceModel& workspace) {
    std::size_t count = 0u;
    for (const thrystr::app::Track& track : workspace.tracks) {
        count += track.sections.size();
    }
    return count;
}

AutoFitProgressSnapshot auto_fit_progress_snapshot(AppState& state) {
    AutoFitProgressSnapshot snapshot;
    AutoFitJob& job = state.auto_fit_job;
    snapshot.running = job.running.load();
    std::scoped_lock lock(job.result_mutex);
    snapshot.multi_track = job.multi_track;
    snapshot.cancelled = job.cancelled;
    snapshot.processed_samples = job.processed_samples;
    snapshot.total_samples = job.total_samples;
    snapshot.completed_segments = job.completed_segments;
    snapshot.total_segments = job.total_segments;
    snapshot.active_segment_index = job.active_segment_index;
    snapshot.active_segment_start = job.active_segment_start;
    snapshot.active_segment_length = job.active_segment_length;
    snapshot.progress_label = job.progress_label;
    snapshot.active_operation = job.active_operation;
    snapshot.error = job.error;
    snapshot.activity_log = job.activity_log;
    return snapshot;
}

void store_single_track_job_result(AutoFitJob& job, thrystr::app::ConvergentFitResult fit) {
    const bool cancelled = fit.cancelled;
    std::scoped_lock lock(job.result_mutex);
    job.sections = std::move(fit.sections);
    job.cancelled = cancelled;
    job.processed_samples = job.total_samples;
    job.completed_segments = job.total_segments == 0u ? 1u : job.total_segments;
    job.active_operation = cancelled ? "cancelled" : "result ready";
    append_auto_fit_activity_locked(job, cancelled ? "Single-track fit cancelled"
                                                   : "Single-track fit result ready");
}

void store_segment_fit_patch(AutoFitJob& job, SegmentFitPatch patch) {
    std::scoped_lock lock(job.result_mutex);
    job.processed_samples += patch.sample_count;
    job.completed_segments = std::max(job.completed_segments, patch.segment_index + 1u);
    job.used_parity = job.used_parity || patch.used_parity;
    job.cancelled = job.cancelled || patch.cancelled;
    job.progress_label = "segment " + format_count(job.completed_segments) + "/" +
                         format_count(std::max<std::size_t>(1u, job.total_segments));
    job.active_operation = "published fitted curves";
    append_auto_fit_activity_locked(
        job, job.progress_label + ": published " +
                 format_count(workspace_section_count(patch.workspace)) + " sections");
    job.pending_patches.push_back(std::move(patch));
    job.partial_dirty = true;
}

void store_auto_fit_job_error(AutoFitJob& job, std::string message) {
    std::scoped_lock lock(job.result_mutex);
    job.error = std::move(message);
    append_auto_fit_activity_locked(job, "error: " + job.error);
}

thrystr::app::ValidationReport validate_fitted_sections(AppState& state) {
    thrystr::app::ValidationReport report;
    if (!state.analysis || state.fitted_sections.empty()) {
        report.pass = false;
        report.issues.push_back({"no fitted sections to validate", 0, 0, 0});
        return report;
    }

    std::vector<thrystr::app::Section> sorted = state.fitted_sections;
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        return left.start_index < right.start_index;
    });

    std::uint64_t expected_start = sorted.front().start_index;
    double residual_sum = 0.0;
    std::size_t residual_count = 0;
    for (std::size_t section_index = 0; section_index < sorted.size(); ++section_index) {
        const thrystr::app::Section& section = sorted[section_index];
        if (section.start_index != expected_start) {
            report.pass = false;
            report.issues.push_back({"fitted section coverage has a gap or overlap", 0,
                                     section_index, static_cast<std::size_t>(expected_start)});
            expected_start = section.start_index;
        }
        if (section.length == 0u) {
            report.pass = false;
            report.issues.push_back(
                {"fitted section has zero length", 0, section_index, section.start_index});
            continue;
        }
        const std::size_t section_last =
            static_cast<std::size_t>(thrystr::app::section_end(section) - 1u);
        ensure_lazy_blocks(state, section.start_index, section_last);
        for (std::size_t index = section.start_index; index < thrystr::app::section_end(section);
             ++index) {
            const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, index);
            if (!scalar) {
                report.pass = false;
                report.issues.push_back(
                    {"fitted section sample is not loaded", 0, section_index, index});
                continue;
            }
            const double residual =
                std::abs(static_cast<double>(thrystr::app::wave_value_at_index(section, index)) -
                         static_cast<double>(*scalar));
            report.max_residual = std::max(report.max_residual, residual);
            residual_sum += residual;
            ++residual_count;
            if (residual > section.fit_tolerance) {
                report.pass = false;
                report.issues.push_back(
                    {"fitted section residual exceeds tolerance", 0, section_index, index});
            }
        }
        expected_start = thrystr::app::section_end(section);
    }
    report.checked_samples = residual_count;
    report.mean_residual =
        residual_count == 0u ? 0.0 : residual_sum / static_cast<double>(residual_count);
    return report;
}

std::vector<thrystr::app::Scalar>
load_track_auto_fit_scalars(std::ifstream& input, const std::filesystem::path& path,
                            const std::vector<thrystr::ValueMapper>& mappers,
                            const TrackAutoFitRange& range, std::uint8_t sample_bits) {
    std::vector<std::uint8_t> bytes = read_file_slice_from_stream(
        input, path, range.start * static_cast<std::size_t>(sample_bits / 8u),
        range.length * static_cast<std::size_t>(sample_bits / 8u));
    return map_source_bytes_to_scalars(bytes, sample_bits, mappers);
}

void run_track_auto_fit_worker(AutoFitJob& job, std::filesystem::path source_path,
                               std::vector<thrystr::ValueMapper> mappers,
                               std::vector<TrackAutoFitRange> ranges, double tolerance,
                               double spacing_nm, std::size_t max_section_length,
                               std::uint8_t sample_bits) {
    try {
        append_auto_fit_activity(job, "opening source stream: " + source_path.filename().string());
        std::ifstream input(source_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("could not open file: " + source_path.string());
        }

        for (std::size_t segment_index = 0; segment_index < ranges.size(); ++segment_index) {
            if (job.cancel_requested.load()) {
                std::scoped_lock lock(job.result_mutex);
                job.cancelled = true;
                append_auto_fit_activity_locked(job, "cancel requested before next segment");
                break;
            }

            const TrackAutoFitRange& range = ranges[segment_index];
            store_auto_fit_stage(job, segment_index, range, "loading source samples");
            std::vector<thrystr::app::Scalar> samples =
                load_track_auto_fit_scalars(input, source_path, mappers, range, sample_bits);
            if (samples.empty()) {
                store_auto_fit_stage(job, segment_index, range, "skipped empty source range");
                continue;
            }

            store_auto_fit_stage(job, segment_index, range,
                                 "partitioning tracks and fitting functions");
            thrystr::app::MultiTrackOptions track_options;
            track_options.tolerance = tolerance;
            track_options.default_spacing_nm = spacing_nm;
            track_options.max_section_length = std::min(max_section_length, samples.size());
            track_options.min_segment_length =
                std::min<std::size_t>(thrystr::app::kMinSegmentSize, samples.size());
            track_options.min_data_tracks = kTrackAutoFitMinDataTracks;
            track_options.max_data_tracks = kTrackAutoFitMaxDataTracks;
            track_options.allow_single_track_fallback = false;
            track_options.cancel_requested = &job.cancel_requested;

            thrystr::app::MultiTrackFitResult fit =
                thrystr::app::fit_multi_track_sections(samples, track_options);
            if (fit.cancelled) {
                std::scoped_lock lock(job.result_mutex);
                job.cancelled = true;
                append_auto_fit_activity_locked(job, "cancelled while fitting current segment");
                break;
            }

            store_auto_fit_stage(job, segment_index, range, "validating fitted reconstruction");
            thrystr::app::ValidationReport validation =
                thrystr::app::validate_tracks(samples, fit.workspace, track_options.parity_margin);
            SegmentFitPatch patch;
            patch.start_offset = range.start;
            patch.sample_count = samples.size();
            patch.segment_index = segment_index;
            patch.workspace = std::move(fit.workspace);
            patch.validation = std::move(validation);
            patch.used_parity = fit.used_parity;
            patch.cancelled = fit.cancelled;
            store_segment_fit_patch(job, std::move(patch));
        }
        append_auto_fit_activity(job, "track auto-fit worker finished");
    } catch (const std::exception& error) {
        store_auto_fit_job_error(job, error.what());
    }
    job.done.store(true);
}

void apply_auto_fit_partial_if_ready(AppState& state) {
    AutoFitJob& job = state.auto_fit_job;
    if (!job.running.load() || !job.multi_track) {
        return;
    }

    std::vector<SegmentFitPatch> patches;
    std::size_t processed_samples = 0u;
    std::size_t total_samples = 0u;
    std::size_t completed_segments = 0u;
    std::size_t total_segments = 0u;
    std::string progress_label;
    std::string active_operation;
    bool cancelled = false;
    {
        std::scoped_lock lock(job.result_mutex);
        if (!job.partial_dirty && job.pending_patches.empty()) {
            return;
        }
        patches = std::move(job.pending_patches);
        job.pending_patches.clear();
        job.partial_dirty = false;
        processed_samples = job.processed_samples;
        total_samples = job.total_samples;
        completed_segments = job.completed_segments;
        total_segments = job.total_segments;
        progress_label = job.progress_label;
        active_operation = job.active_operation;
        cancelled = job.cancelled;
    }

    for (SegmentFitPatch& patch : patches) {
        apply_segment_fit_patch(state, std::move(patch));
    }

    if (total_samples > 0u) {
        state.segment.active = true;
        state.segment.selection_start = 0u;
        state.segment.selection_end = total_samples - 1u;
    }
    state.show_reconstruction_only = true;
    state.status = "Track auto-fit building " +
                   (progress_label.empty() ? std::string("segments") : progress_label) + ": " +
                   format_count(processed_samples) + "/" + format_count(total_samples) + " samples";
    if (!active_operation.empty()) {
        state.status += ", " + active_operation;
    }
    if (total_segments > 0u) {
        state.status += " (" + format_count(completed_segments) + "/" +
                        format_count(total_segments) + " segments)";
    }
    const EncodingEstimate encoding = estimate_encoded_output_size(state);
    if (encoding.encoded_bytes > 0u) {
        state.status += ", encoded " + format_bytes(encoding.encoded_bytes) + " (" +
                        format_percent(encoding.source_ratio) + " of source)";
    }
    if (cancelled) {
        state.status += " cancelling";
    }
}

void auto_fit_current_range(AppState& state, bool multi_track = false) {
    if (!state.analysis || source_sample_count(state) == 0u) {
        state.status = "Load source data before auto-fit";
        return;
    }
    if (state.auto_fit_job.running.load()) {
        state.status = "Auto-fit is already running";
        return;
    }
    if (state.auto_fit_job.worker.joinable()) {
        state.auto_fit_job.worker.join();
    }

    AutoFitJob& job = state.auto_fit_job;
    job.cancel_requested.store(false);
    job.done.store(false);
    job.running.store(true);
    job.cancelled = false;
    job.multi_track = multi_track;
    job.used_parity = false;
    job.partial_dirty = false;
    job.start_offset = 0u;
    job.requested_last = 0u;
    job.capped_last = 0u;
    job.processed_samples = 0u;
    job.total_samples = 0u;
    job.completed_segments = 0u;
    job.total_segments = 0u;
    job.active_segment_index = 0u;
    job.active_segment_start = 0u;
    job.active_segment_length = 0u;
    job.progress_label.clear();
    job.active_operation.clear();
    job.error.clear();
    {
        std::scoped_lock lock(job.result_mutex);
        job.sections.clear();
        job.workspace = {};
        job.validation = {};
        job.pending_patches.clear();
        clear_auto_fit_activity(job);
    }

    const double tolerance = std::max(1.0e-9, static_cast<double>(state.wave_tolerance));
    const double spacing_nm = data_spatial_period_nm(state);
    if (multi_track) {
        const std::size_t count = source_sample_count(state);
        const std::size_t target_length =
            std::max<std::size_t>(lazy_block_samples(state), thrystr::app::kMinSegmentSize);
        std::vector<TrackAutoFitRange> ranges = build_track_auto_fit_ranges(count, target_length);
        if (ranges.empty() || state.analysis->source_path.empty()) {
            job.running.store(false);
            state.status = "Track auto-fit needs a source file";
            return;
        }

        try {
            state.fitted_workspace = make_track_auto_fit_workspace(kTrackAutoFitMaxDataTracks);
        } catch (const std::exception& error) {
            job.running.store(false);
            state.status =
                std::string("Track auto-fit workspace allocation failed: ") + error.what();
            return;
        }
        state.fitted_sections.clear();
        state.fitted_workspace_valid = true;
        state.fitted_workspace_used_parity = true;
        state.fit_validation = {};
        state.show_fit_validation = false;
        state.show_reconstruction_only = true;
        state.segment.active = true;
        state.segment.selection_start = 0u;
        state.segment.selection_end = count - 1u;

        job.start_offset = 0u;
        job.requested_last = count - 1u;
        job.capped_last = count - 1u;
        job.total_samples = count;
        job.total_segments = ranges.size();
        job.progress_label = "segment 0/" + format_count(ranges.size());
        job.active_operation = "queued";
        append_auto_fit_activity(job, "track auto-fit queued: " + format_count(count) +
                                          " samples across " + format_count(ranges.size()) +
                                          " segments");
        state.status =
            "Track auto-fit segmenting whole file: 0/" + format_count(ranges.size()) + " segments";

        const std::filesystem::path source_path = state.analysis->source_path;
        std::vector<thrystr::ValueMapper> mappers = state.mappers;
        const std::uint8_t sample_bits = data_sample_bits(state);
        job.worker =
            std::thread([&job, source_path, mappers = std::move(mappers),
                         ranges = std::move(ranges), tolerance, spacing_nm, sample_bits]() mutable {
                run_track_auto_fit_worker(job, source_path, std::move(mappers), std::move(ranges),
                                          tolerance, spacing_nm, kAutoFitMaxSamples, sample_bits);
            });
        return;
    }

    clamp_segment(state);
    const auto [first, requested_last] = analysis_selection_bounds(state);
    const std::size_t capped_last = std::min(requested_last, first + kAutoFitMaxSamples - 1u);
    std::vector<thrystr::app::Scalar> scalars =
        collect_scalars_for_range(state, first, capped_last);
    if (scalars.empty()) {
        job.running.store(false);
        state.status = "No scalar samples available for auto-fit";
        return;
    }

    thrystr::app::ConvergentFitOptions options;
    options.tolerance = tolerance;
    options.default_spacing_nm = spacing_nm;
    options.max_section_length = std::min<std::size_t>(4096u, scalars.size());

    job.start_offset = first;
    job.requested_last = requested_last;
    job.capped_last = capped_last;
    job.total_samples = scalars.size();
    job.total_segments = 1u;
    job.active_segment_start = first;
    job.active_segment_length = scalars.size();
    job.progress_label = "single section fit";
    job.active_operation = "queued";
    append_auto_fit_activity(job, "single-track auto-fit queued: " + format_count(scalars.size()) +
                                      " samples");

    state.status = "Auto-fit running on " + format_count(scalars.size()) + " samples";
    job.worker = std::thread([&job, samples = std::move(scalars), options]() mutable {
        try {
            {
                std::scoped_lock lock(job.result_mutex);
                job.active_operation = "fitting single-track sections";
                append_auto_fit_activity_locked(job, "fitting single-track sections");
            }
            options.cancel_requested = &job.cancel_requested;
            const thrystr::app::ConvergentFitResult fit =
                thrystr::app::fit_convergent_sections(samples, options);
            store_single_track_job_result(job, std::move(fit));
        } catch (const std::exception& error) {
            store_auto_fit_job_error(job, error.what());
        }
        job.done.store(true);
    });
}

void cancel_auto_fit(AppState& state) {
    if (!state.auto_fit_job.running.load()) {
        return;
    }
    state.auto_fit_job.cancel_requested.store(true);
    state.status = "Cancelling auto-fit";
}

void finish_auto_fit_if_ready(AppState& state) {
    AutoFitJob& job = state.auto_fit_job;
    apply_auto_fit_partial_if_ready(state);
    if (!job.running.load() || !job.done.load()) {
        return;
    }
    if (job.worker.joinable()) {
        job.worker.join();
    }

    std::vector<thrystr::app::Section> sections;
    bool cancelled = false;
    bool multi_track = false;
    bool used_parity = false;
    std::string error;
    {
        std::scoped_lock lock(job.result_mutex);
        sections = job.sections;
        cancelled = job.cancelled;
        multi_track = job.multi_track;
        used_parity = job.used_parity;
        error = job.error;
    }
    job.running.store(false);
    job.done.store(false);

    if (!error.empty()) {
        state.status = "Auto-fit failed: " + error;
        return;
    }

    const std::size_t local_count =
        job.capped_last >= job.start_offset ? job.capped_last - job.start_offset + 1u : 0u;
    if (multi_track) {
        state.fitted_workspace_valid = !state.fitted_workspace.tracks.empty();
        state.fitted_workspace_used_parity =
            used_parity || state.fitted_workspace.parity_track_id != thrystr::app::kNoParityTrack;
        used_parity = state.fitted_workspace_used_parity;
    } else {
        state.fitted_sections = std::move(sections);
        for (thrystr::app::Section& section : state.fitted_sections) {
            section.start_index += static_cast<std::uint32_t>(job.start_offset);
        }
        state.fitted_workspace = {};
        thrystr::app::Track track;
        track.id = 0u;
        track.kind = thrystr::app::TrackKind::Data;
        track.name = "single track";
        track.sections = state.fitted_sections;
        state.fitted_workspace.tracks.push_back(std::move(track));
        state.fitted_workspace.active_track_id = 0u;
        state.fitted_workspace.parity_track_id = thrystr::app::kNoParityTrack;
        state.fitted_workspace_valid = true;
        state.fitted_workspace_used_parity = false;
        state.fit_validation = validate_fitted_sections(state);
    }
    if (local_count > 0u) {
        state.segment.active = true;
        state.segment.selection_start = job.start_offset;
        state.segment.selection_end = job.capped_last;
        clamp_segment(state);
    }
    state.show_reconstruction_only = true;
    state.status = std::string(cancelled     ? (multi_track ? "Track auto-fit cancelled: "
                                                            : "Auto-fit cancelled: ")
                               : multi_track ? "Track auto-fit: "
                                             : "Auto-fit: ") +
                   format_count(state.fitted_sections.size()) + " sections, " +
                   format_count(state.fit_validation.checked_samples) + " samples, max residual " +
                   std::to_string(state.fit_validation.max_residual);
    if (multi_track) {
        state.status += ", " + format_count(data_track_count(state.fitted_workspace)) + " tracks";
        state.status += used_parity ? " + parity" : " single-track fallback";
    }
    const EncodingEstimate encoding = estimate_encoded_output_size(state);
    if (encoding.encoded_bytes > 0u) {
        state.status += ", encoded " + format_bytes(encoding.encoded_bytes) + " (" +
                        format_percent(encoding.source_ratio) + " of source)";
    }
    if (job.requested_last != job.capped_last) {
        state.status += " (range capped)";
    }
}

void load_path(AppState& state);

bool should_defer_phase_fit_on_load(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    return !error && file_size >= kLargeSourcePhaseFitDeferBytes;
}

thrystr::Analysis load_lazy_analysis_window(const std::filesystem::path& path,
                                            std::size_t block_bytes, float max_slope,
                                            double wave_scale, double wave_tolerance,
                                            int phase_steps, std::size_t max_phase_test_points,
                                            std::span<const thrystr::ValueMapper> mappers,
                                            std::uint8_t sample_bits) {
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
    const std::size_t sample_bytes =
        static_cast<std::size_t>(normalize_sample_bits(sample_bits) / 8u);
    const std::size_t length = (std::min(block_bytes, source_size) / sample_bytes) * sample_bytes;
    analysis.bytes = read_file_slice(path, 0u, length);
    if (normalize_sample_bits(sample_bits) == 8u) {
        analysis.mapped_bytes = thrystr::map_bytes_to_wrapped(analysis.bytes, mappers);
    } else {
        analysis.mapped_bytes = analysis.bytes;
    }
    const std::vector<thrystr::app::Scalar> source_scalars =
        map_source_bytes_to_scalars(analysis.bytes, sample_bits, mappers);
    analysis.scalars.clear();
    analysis.scalars.reserve(source_scalars.size());
    for (const thrystr::app::Scalar scalar : source_scalars) {
        analysis.scalars.push_back(static_cast<float>(scalar));
    }
    analysis.window =
        thrystr::find_highest_entropy_window(analysis.mapped_bytes, analysis.mapped_bytes.size());
    analysis.window.offset = 0u;
    analysis.window.length = analysis.scalars.size();
    const std::size_t source_samples = source_size / sample_bytes;
    analysis.window_count = source_samples >= analysis.window.length
                                ? source_samples - analysis.window.length + 1u
                                : 0u;
    analysis.max_delta_sample_index = analysis.window.delta_index / sample_bytes;
    analysis.max_abs_scalar_delta = thrystr::max_abs_scalar_delta(analysis.scalars);
    analysis.x_scale = thrystr::compute_x_scale(analysis.scalars, max_slope);
    analysis.wave_scale = wave_scale;
    analysis.sine = thrystr::fit_wave_phase(analysis.scalars, false, wave_scale, wave_tolerance,
                                            phase_steps, max_phase_test_points);
    analysis.cosine = thrystr::fit_wave_phase(analysis.scalars, true, wave_scale, wave_tolerance,
                                              phase_steps, max_phase_test_points);
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
            analysis.scalars, false, analysis.wave_scale, static_cast<double>(state.wave_tolerance),
            state.phase_steps, static_cast<std::size_t>(state.phase_test_points));
        analysis.cosine = thrystr::fit_wave_phase(
            analysis.scalars, true, analysis.wave_scale, static_cast<double>(state.wave_tolerance),
            state.phase_steps, static_cast<std::size_t>(state.phase_test_points));
        state.status = "Fitted function phases";
    } catch (const std::exception& error) {
        state.status = error.what();
    }
}

void save_wave_settings(AppState& state, const std::filesystem::path& path) {
    const std::filesystem::path output_path = with_wave_settings_extension(path);
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open wave settings for write: " + output_path.string());
    }

    output.write(kWaveSettingsMagic.data(),
                 static_cast<std::streamsize>(kWaveSettingsMagic.size()));
    const auto write_value = [&]<typename T>(const T& value) { write_binary_le(output, value); };
    write_value(kWaveSettingsVersion);
    write_value(kWaveSettingsEndianStamp);
    write_value(state.max_slope);
    write_value(state.wave_scale);
    write_value(state.wave_tolerance);
    write_value(state.phase_steps);
    write_value(state.phase_test_points);
    write_value(state.zoom_x);
    write_value(state.zoom_y);

    const std::uint8_t show_points = state.show_points ? 1 : 0;
    const std::uint8_t show_lines = state.show_lines ? 1 : 0;
    constexpr std::uint8_t reserved_byte = 1;
    write_value(show_points);
    write_value(show_lines);
    write_value(reserved_byte);
    write_value(reserved_byte);

    const double sine_phase = state.analysis ? state.analysis->sine.phase_radians : 0.0;
    const double cosine_phase = state.analysis ? state.analysis->cosine.phase_radians : 0.0;
    write_value(sine_phase);
    write_value(cosine_phase);

    const auto mapper_count = static_cast<std::uint32_t>(state.mappers.size());
    write_value(mapper_count);
    for (const thrystr::ValueMapper& mapper : state.mappers) {
        const auto kind = static_cast<std::uint32_t>(mapper.kind);
        const std::uint8_t enabled = mapper.enabled ? 1 : 0;
        write_value(kind);
        write_value(mapper.operand);
        write_value(enabled);
    }

    const auto entity_count = static_cast<std::uint32_t>(state.entities.size());
    write_value(entity_count);
    for (const Entity& entity : state.entities) {
        write_value(static_cast<std::int32_t>(entity.id));
        write_value(static_cast<std::uint32_t>(entity.type));
        write_value(static_cast<std::uint8_t>(entity.visible ? 1 : 0));
        write_string_le(output, entity.name);
        write_value(entity.data.spatial_period_nm);
        write_value(entity.data.sample_bits);
        write_value(static_cast<std::uint32_t>(entity.wave.function));
        write_value(static_cast<std::uint8_t>(entity.wave.use_secondary_function ? 1u : 0u));
        write_value(static_cast<std::uint32_t>(entity.wave.secondary_function));
        write_value(entity.wave.wavelength_nm);
        write_value(entity.wave.amplitude);
        write_value(entity.wave.secondary_amplitude);
        write_value(entity.wave.amplitude_offset);
        write_value(entity.wave.phase_nm);
        write_value(entity.wave.rotation_degrees);
        const auto modifier_count =
            static_cast<std::uint32_t>(entity.wave.wavelength_modifiers.size());
        write_value(modifier_count);
        for (const auto& modifier : entity.wave.wavelength_modifiers) {
            write_value(modifier.first);
            write_value(modifier.second);
        }
    }
    write_value(static_cast<std::int32_t>(state.selected_entity_id));
    write_value(static_cast<std::uint8_t>(state.segment.active ? 1 : 0));
    write_value(static_cast<std::uint64_t>(state.segment.selection_start));
    write_value(static_cast<std::uint64_t>(state.segment.selection_end));
    write_string_le(output, state.path);

    const auto section_count = static_cast<std::uint32_t>(state.fitted_sections.size());
    write_value(section_count);
    for (const thrystr::app::Section& section : state.fitted_sections) {
        write_value(section.start_index);
        write_value(section.length);
        write_value(section.section_spacing_nm);
        write_value(section.wave_wavelength_nm);
        write_value(section.wave_amplitude);
        write_value(section.wave_amplitude_offset);
        write_value(section.wave_phase_nm);
        write_value(section.fit_tolerance);
        write_value(section.max_residual);
        write_value(section.mean_residual);
    }

    const thrystr::app::WorkspaceModel* workspace_to_save =
        state.fitted_workspace_valid ? &state.fitted_workspace : nullptr;
    thrystr::app::WorkspaceModel fallback_workspace;
    if (!workspace_to_save && !state.fitted_sections.empty()) {
        thrystr::app::Track track;
        track.id = 0u;
        track.kind = thrystr::app::TrackKind::Data;
        track.name = "single track";
        track.sections = state.fitted_sections;
        fallback_workspace.tracks.push_back(std::move(track));
        fallback_workspace.active_track_id = 0u;
        fallback_workspace.parity_track_id = thrystr::app::kNoParityTrack;
        workspace_to_save = &fallback_workspace;
    }

    const std::uint8_t track_count =
        workspace_to_save ? static_cast<std::uint8_t>(std::min(workspace_to_save->tracks.size(),
                                                               thrystr::app::kMaxTracks))
                          : 0u;
    write_value(track_count);
    write_value(workspace_to_save ? workspace_to_save->parity_track_id
                                  : thrystr::app::kNoParityTrack);
    if (workspace_to_save) {
        for (std::size_t track_index = 0; track_index < track_count; ++track_index) {
            const thrystr::app::Track& track = workspace_to_save->tracks[track_index];
            write_value(track.id);
            write_value(static_cast<std::uint8_t>(track.kind));
            write_value(static_cast<std::uint8_t>(track.visible ? 1u : 0u));
            write_string_le(output, track.name);
            if (track.kind == thrystr::app::TrackKind::Data && !track.owned_mask.empty()) {
                const auto mask_size = static_cast<std::uint32_t>(track.owned_mask.size());
                write_value(mask_size);
                output.write(reinterpret_cast<const char*>(track.owned_mask.data()),
                             static_cast<std::streamsize>(track.owned_mask.size()));
                if (!output) {
                    throw std::runtime_error("could not write wave settings mask");
                }
            } else {
                write_value(static_cast<std::uint32_t>(0u));
            }
            const auto track_section_count = static_cast<std::uint32_t>(track.sections.size());
            write_value(track_section_count);
            for (const thrystr::app::Section& section : track.sections) {
                write_value(section.start_index);
                write_value(section.length);
                write_value(section.section_spacing_nm);
                write_value(section.wave_wavelength_nm);
                write_value(section.wave_amplitude);
                write_value(section.wave_amplitude_offset);
                write_value(section.wave_phase_nm);
                write_value(section.fit_tolerance);
                write_value(section.max_residual);
                write_value(section.mean_residual);
            }
        }
    }

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

    const std::uint32_t version = read_binary_le<std::uint32_t>(input);
    const bool portable_payload = version >= 4;
    if (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 &&
        version != 6 && version != kWaveSettingsVersion) {
        throw std::runtime_error("unsupported wave settings version");
    }
    if (portable_payload) {
        const std::uint32_t stamp = read_binary_le<std::uint32_t>(input);
        if (stamp != kWaveSettingsEndianStamp) {
            throw std::runtime_error("wave settings endian stamp mismatch");
        }
    }

    const auto read_value = [&]<typename T>() -> T {
        return portable_payload ? read_binary_le<T>(input) : read_binary<T>(input);
    };
    const auto read_text = [&]() -> std::string {
        return portable_payload ? read_string_le(input) : read_string(input);
    };

    state.max_slope = read_value.template operator()<float>();
    state.wave_scale = read_value.template operator()<float>();
    state.wave_tolerance = read_value.template operator()<float>();
    state.phase_steps = read_value.template operator()<int>();
    state.phase_test_points = read_value.template operator()<int>();
    state.zoom_x = read_value.template operator()<float>();
    state.zoom_y = read_value.template operator()<float>();
    state.show_points = read_value.template operator()<std::uint8_t>() != 0;
    state.show_lines = read_value.template operator()<std::uint8_t>() != 0;
    (void)read_value.template operator()<std::uint8_t>();
    (void)read_value.template operator()<std::uint8_t>();
    const double sine_phase = read_value.template operator()<double>();
    const double cosine_phase = read_value.template operator()<double>();

    state.entities.clear();
    state.selected_entity_id = 0;
    state.entity_name_id = 0;
    state.next_entity_id = 1;
    state.wave_serial = 1;
    state.segment = {};
    state.fitted_sections.clear();
    state.fitted_workspace = {};
    state.fitted_workspace_valid = false;
    state.fitted_workspace_used_parity = false;
    state.fit_validation = {};

    const std::uint32_t mapper_count = read_value.template operator()<std::uint32_t>();
    if (mapper_count > kMaxWaveSettingsItems) {
        throw std::runtime_error("wave settings mapper count too large");
    }
    state.mappers.clear();
    state.mappers.reserve(mapper_count);
    for (std::uint32_t i = 0; i < mapper_count; ++i) {
        const auto kind =
            static_cast<thrystr::ValueMapperKind>(read_value.template operator()<std::uint32_t>());
        const double operand = read_value.template operator()<double>();
        const bool enabled = read_value.template operator()<std::uint8_t>() != 0;
        state.mappers.push_back({kind, operand, enabled});
    }

    if (version >= 2) {
        const std::uint32_t entity_count = read_value.template operator()<std::uint32_t>();
        if (entity_count > kMaxWaveSettingsItems) {
            throw std::runtime_error("wave settings entity count too large");
        }
        state.entities.reserve(entity_count);
        int max_entity_id = 0;
        int wave_count = 0;
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            Entity entity;
            entity.id = read_value.template operator()<std::int32_t>();
            entity.type = static_cast<EntityType>(read_value.template operator()<std::uint32_t>());
            entity.visible = read_value.template operator()<std::uint8_t>() != 0;
            entity.name = read_text();
            entity.data.spatial_period_nm = read_value.template operator()<double>();
            entity.data.sample_bits =
                version >= 6 ? normalize_sample_bits(read_value.template operator()<std::uint8_t>())
                             : 8u;
            if (version >= 7) {
                entity.wave.function =
                    normalize_wave_function_kind(read_value.template operator()<std::uint32_t>());
                entity.wave.use_secondary_function =
                    read_value.template operator()<std::uint8_t>() != 0u;
                entity.wave.secondary_function =
                    normalize_wave_function_kind(read_value.template operator()<std::uint32_t>());
            }
            entity.wave.wavelength_nm = read_value.template operator()<double>();
            entity.wave.amplitude = read_value.template operator()<double>();
            if (version >= 7) {
                entity.wave.secondary_amplitude = read_value.template operator()<double>();
            }
            entity.wave.amplitude_offset = read_value.template operator()<double>();
            entity.wave.phase_nm = read_value.template operator()<double>();
            if (version >= 7) {
                entity.wave.rotation_degrees = read_value.template operator()<double>();
            }
            const std::uint32_t modifier_count = read_value.template operator()<std::uint32_t>();
            if (modifier_count > kMaxWaveSettingsItems) {
                throw std::runtime_error("wave settings modifier count too large");
            }
            entity.wave.wavelength_modifiers.reserve(modifier_count);
            for (std::uint32_t modifier_index = 0; modifier_index < modifier_count;
                 ++modifier_index) {
                const double x_nm = read_value.template operator()<double>();
                const double delta_nm = read_value.template operator()<double>();
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
        state.selected_entity_id = read_value.template operator()<std::int32_t>();
        state.segment.active = read_value.template operator()<std::uint8_t>() != 0;
        state.segment.selection_start =
            static_cast<std::size_t>(read_value.template operator()<std::uint64_t>());
        state.segment.selection_end =
            static_cast<std::size_t>(read_value.template operator()<std::uint64_t>());
        const std::string source_path = read_text();
        if (!source_path.empty()) {
            copy_to_buffer(state.path, source_path);
        }
        if (version >= 3) {
            const std::uint32_t section_count = read_value.template operator()<std::uint32_t>();
            if (section_count > kMaxWaveSettingsItems) {
                throw std::runtime_error("wave settings section count too large");
            }
            state.fitted_sections.reserve(section_count);
            for (std::uint32_t section_index = 0; section_index < section_count; ++section_index) {
                thrystr::app::Section section;
                section.start_index = read_value.template operator()<std::uint32_t>();
                section.length = read_value.template operator()<std::uint32_t>();
                section.section_spacing_nm = read_value.template operator()<double>();
                section.wave_wavelength_nm = read_value.template operator()<double>();
                section.wave_amplitude = read_value.template operator()<double>();
                section.wave_amplitude_offset = read_value.template operator()<double>();
                section.wave_phase_nm = read_value.template operator()<double>();
                section.fit_tolerance = read_value.template operator()<double>();
                section.max_residual = read_value.template operator()<double>();
                section.mean_residual = read_value.template operator()<double>();
                state.fitted_sections.push_back(section);
            }
        }
        if (version >= 5) {
            thrystr::app::WorkspaceModel workspace;
            const std::uint8_t track_count = read_value.template operator()<std::uint8_t>();
            workspace.parity_track_id = read_value.template operator()<std::uint8_t>();
            if (track_count > thrystr::app::kMaxTracks) {
                throw std::runtime_error("wave settings track count too large");
            }
            workspace.tracks.reserve(track_count);
            for (std::uint8_t track_index = 0; track_index < track_count; ++track_index) {
                thrystr::app::Track track;
                track.id = read_value.template operator()<std::uint8_t>();
                track.kind = static_cast<thrystr::app::TrackKind>(
                    read_value.template operator()<std::uint8_t>());
                track.visible = read_value.template operator()<std::uint8_t>() != 0u;
                track.name = read_text();
                const std::uint32_t mask_size = read_value.template operator()<std::uint32_t>();
                if (mask_size > kMaxWaveSettingsMaskBytes) {
                    throw std::runtime_error("wave settings owned mask too large");
                }
                if (mask_size > 0u) {
                    track.owned_mask.resize(mask_size);
                    input.read(reinterpret_cast<char*>(track.owned_mask.data()),
                               static_cast<std::streamsize>(track.owned_mask.size()));
                    if (!input) {
                        throw std::runtime_error("could not read wave settings owned mask");
                    }
                }
                const std::uint32_t track_section_count =
                    read_value.template operator()<std::uint32_t>();
                if (track_section_count > kMaxWaveSettingsItems) {
                    throw std::runtime_error("wave settings track section count too large");
                }
                track.sections.reserve(track_section_count);
                for (std::uint32_t section_index = 0; section_index < track_section_count;
                     ++section_index) {
                    thrystr::app::Section section;
                    section.start_index = read_value.template operator()<std::uint32_t>();
                    section.length = read_value.template operator()<std::uint32_t>();
                    section.section_spacing_nm = read_value.template operator()<double>();
                    section.wave_wavelength_nm = read_value.template operator()<double>();
                    section.wave_amplitude = read_value.template operator()<double>();
                    section.wave_amplitude_offset = read_value.template operator()<double>();
                    section.wave_phase_nm = read_value.template operator()<double>();
                    section.fit_tolerance = read_value.template operator()<double>();
                    section.max_residual = read_value.template operator()<double>();
                    section.mean_residual = read_value.template operator()<double>();
                    track.sections.push_back(section);
                }
                workspace.tracks.push_back(std::move(track));
            }
            state.fitted_workspace = std::move(workspace);
            state.fitted_workspace_valid = !state.fitted_workspace.tracks.empty();
            state.fitted_workspace_used_parity =
                state.fitted_workspace.parity_track_id != thrystr::app::kNoParityTrack;
            if (state.fitted_workspace_valid && state.fitted_sections.empty()) {
                for (const thrystr::app::Track& track : state.fitted_workspace.tracks) {
                    if (track.kind == thrystr::app::TrackKind::Data) {
                        state.fitted_sections = track.sections;
                        break;
                    }
                }
            }
        } else if (!state.fitted_sections.empty()) {
            state.fitted_workspace = {};
            thrystr::app::Track track;
            track.id = 0u;
            track.kind = thrystr::app::TrackKind::Data;
            track.name = "single track";
            track.sections = state.fitted_sections;
            state.fitted_workspace.tracks.push_back(std::move(track));
            state.fitted_workspace.active_track_id = 0u;
            state.fitted_workspace.parity_track_id = thrystr::app::kNoParityTrack;
            state.fitted_workspace_valid = true;
            state.fitted_workspace_used_parity = false;
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
        Entity& data = ensure_data_entity(state);
        data.data.sample_bits = normalize_sample_bits(data.data.sample_bits);
        const std::filesystem::path source_path(state.path);
        const bool phase_fit_deferred = should_defer_phase_fit_on_load(source_path);
        state.lazy_block_mib = std::clamp(state.lazy_block_mib, 1, 10);
        const std::size_t block_bytes = lazy_block_bytes(state);
        state.analysis = load_lazy_analysis_window(
            source_path, block_bytes, state.max_slope, static_cast<double>(state.wave_scale),
            static_cast<double>(state.wave_tolerance), state.phase_steps,
            phase_fit_deferred ? 0u : static_cast<std::size_t>(state.phase_test_points),
            state.mappers, data.data.sample_bits);
        seed_lazy_cache_from_analysis(state);

        const auto& analysis = *state.analysis;
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
        state.status = "Loaded " + analysis.source_path.filename().string() + " / lazy " +
                       format_bytes(block_bytes) + " / source " +
                       format_bytes(analysis.source_size) + " / " +
                       format_count(data.data.sample_bits) + "-bit points";
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
                                 : palette::ink::primary;
    ImGui::PushStyleColor(ImGuiCol_Text, palette::to_vec4(text_color));
    const bool clicked = thrystr::gui::icon_button(glyph, tooltip);
    ImGui::PopStyleColor();
    return clicked;
}

bool window_control_button(WindowControl control, const char* tooltip,
                           thrystr::gui::WindowHandle window) {
    const float size = kWindowControlButtonSize;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const char* id = control == WindowControl::Minimize   ? "##window_minimize"
                     : control == WindowControl::Maximize ? "##window_maximize"
                                                          : "##window_close";
    ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();
    auto* draw = ImGui::GetWindowDrawList();

    const ImU32 fill =
        control == WindowControl::Close && hovered
            ? palette::with_alpha(palette::status::destructive, active ? 0.70f : 0.52f)
        : hovered ? palette::surface::control
                  : kTransparent;
    const ImU32 ink = hovered ? palette::ink::primary : palette::ink::muted;
    draw->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), fill, palette::radii::ctrl);

    const ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
    if (control == WindowControl::Minimize) {
        draw->AddLine(ImVec2(center.x - 5.0f, center.y + 4.0f),
                      ImVec2(center.x + 5.0f, center.y + 4.0f), ink, 1.6f);
    } else if (control == WindowControl::Maximize) {
        const bool maximized = window && thrystr::gui::is_maximized(window);
        if (maximized) {
            draw->AddRect(ImVec2(center.x - 3.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 3.0f), ink, 0.0f, 0, 1.4f);
            draw->AddRect(ImVec2(center.x - 6.0f, center.y - 2.0f),
                          ImVec2(center.x + 2.0f, center.y + 6.0f), ink, 0.0f, 0, 1.4f);
        } else {
            draw->AddRect(ImVec2(center.x - 5.0f, center.y - 5.0f),
                          ImVec2(center.x + 5.0f, center.y + 5.0f), ink, 0.0f, 0, 1.5f);
        }
    } else {
        draw->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f),
                      ImVec2(center.x + 5.0f, center.y + 5.0f), ink, 1.6f);
        draw->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f),
                      ImVec2(center.x - 5.0f, center.y + 5.0f), ink, 1.6f);
    }

    if (hovered && tooltip && tooltip[0] != '\0') {
        thrystr::gui::tooltip(tooltip);
    }
    return clicked;
}

float window_control_group_width(int button_count) {
    if (button_count <= 0) {
        return kWindowControlRightMargin;
    }
    const float gaps = static_cast<float>(button_count - 1) * ImGui::GetStyle().ItemSpacing.x;
    return static_cast<float>(button_count) * kWindowControlButtonSize + gaps +
           kWindowControlRightMargin;
}

ImVec2 begin_titlebar_chrome(const char* id) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport.x, kTopChromeHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, palette::pad::window);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, palette::radii::none);
    ImGui::Begin(id, nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(2);
    return viewport;
}

void draw_titlebar_background(const ImVec2& viewport) {
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(viewport.x, kTopChromeHeight),
                        palette::surface::deep);
    draw->AddLine(ImVec2(0.0f, kTopChromeHeight - 1.0f),
                  ImVec2(viewport.x, kTopChromeHeight - 1.0f), palette::border::separator, 1.0f);
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
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "%s", tagline);
}

void draw_titlebar(AppState& state, thrystr::gui::WindowHandle window) {
    const ImVec2 viewport = begin_titlebar_chrome("##titlebar");
    draw_titlebar_background(viewport);
    draw_titlebar_wordmark(state, "/ function workspace");

    ImGui::SameLine(188.0f);
    if (thrystr::gui::ghost_button("File", ImVec2(52.0f, 0.0f))) {
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
        if (ImGui::MenuItem("Save Function Data...")) {
            request_file_dialog(state, DialogPurpose::SaveWave);
        }
        thrystr::gui::muted_separator();
        if (ImGui::MenuItem("Exit")) {
            state.request_close = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (toolbar_icon_button(thrystr::gui::icons::kFile, "Load source data")) {
        request_file_dialog(state, DialogPurpose::OpenSource);
    }
    ImGui::SameLine();
    if (toolbar_icon_button(thrystr::gui::icons::kFolder, "Load function data")) {
        request_file_dialog(state, DialogPurpose::LoadWave);
    }
    ImGui::SameLine();
    if (toolbar_icon_button(thrystr::gui::icons::kSave, "Save function data")) {
        request_file_dialog(state, DialogPurpose::SaveWave);
    }

    ImGui::SameLine();
    if (thrystr::gui::ghost_button("Create", ImVec2(70.0f, 0.0f))) {
        ImGui::OpenPopup("##create_menu");
    }
    if (ImGui::BeginPopup("##create_menu")) {
        if (ImGui::MenuItem("Function", "Ctrl/Cmd+W")) {
            create_wave_entity(state);
        }
        if (ImGui::MenuItem("Interpolated Function")) {
            create_interpolated_wave(state);
        }
        if (ImGui::MenuItem("X-Line Section")) {
            create_section(state);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (thrystr::gui::ghost_button("Settings", ImVec2(82.0f, 0.0f))) {
        ImGui::OpenPopup("##settings_menu");
    }
    if (ImGui::BeginPopup("##settings_menu")) {
        if (ImGui::MenuItem("Preferences...")) {
            state.show_settings = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (thrystr::gui::ghost_button("Help", ImVec2(58.0f, 0.0f))) {
        state.show_docs = true;
    }

    ImGui::SameLine();
    const std::string source_name =
        state.path[0] == '\0'
            ? std::string("source: none")
            : std::string("source: ") + std::filesystem::path(state.path).filename().string();
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "%s", source_name.c_str());

    ImGui::SameLine();
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "%s", state.status.c_str());

    const float controls_width = window_control_group_width(3);
    ImGui::SetCursorPos(ImVec2(viewport.x - controls_width, 8.0f));
    if (window_control_button(WindowControl::Minimize, "Minimize", window)) {
        thrystr::gui::minimize_window(window);
    }
    ImGui::SameLine();
    if (window_control_button(WindowControl::Maximize, "Maximize", window)) {
        thrystr::gui::toggle_maximized_window(window);
    }
    ImGui::SameLine();
    if (window_control_button(WindowControl::Close, "Close", window)) {
        state.request_close = true;
    }

    ImGui::End();
}

bool value_bar_double(const char* label, double* value, double step_per_pixel,
                      const char* format = "%.4f", ImFont* mono_font = nullptr) {
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
    const ImU32 fill = active    ? palette::surface::control_hi
                       : hovered ? palette::surface::control
                                 : palette::surface::panel_alt;
    draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), fill, palette::radii::ctrl);
    draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), palette::border::default_,
                  palette::radii::ctrl);
    char text[128] = {};
    std::snprintf(text, sizeof(text), format, *value);
    if (mono_font) {
        ImGui::PushFont(mono_font);
    }
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    draw->AddText(
        ImVec2(pos.x + size.x - text_size.x - 10.0f, pos.y + (size.y - text_size.y) * 0.5f),
        palette::ink::primary, text);
    if (mono_font) {
        ImGui::PopFont();
    }
    if (hovered) {
        thrystr::gui::tooltip("Drag left or right to adjust");
    }
    ImGui::PopID();
    return changed;
}

bool value_bar_int(const char* label, int* value, double step_per_pixel,
                   ImFont* mono_font = nullptr) {
    double editable = static_cast<double>(*value);
    const bool changed = value_bar_double(label, &editable, step_per_pixel, "%.0f", mono_font);
    if (changed) {
        *value = static_cast<int>(std::llround(editable));
    }
    return changed;
}

void draw_value_mapper_stack(AppState& state) {
    thrystr::gui::section_header("Value Mappers");

    bool mapper_changed = false;
    int remove_index = -1;
    int move_from = -1;
    int move_to = -1;

    if (ImGui::BeginTable("##mapper_table", 4,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
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
            mapper_changed |= ImGui::DragScalar("##operand", ImGuiDataType_Double, &mapper.operand,
                                                0.1f, nullptr, nullptr, "%.6f");

            ImGui::TableSetColumnIndex(3);
            if (thrystr::gui::icon_button(thrystr::gui::icons::kChevronRight, "Move up", 22.0f) &&
                i > 0) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i - 1);
            }
            ImGui::SameLine();
            if (thrystr::gui::icon_button(thrystr::gui::icons::kChevronDown, "Move down", 22.0f) &&
                i + 1 < state.mappers.size()) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i + 1);
            }
            ImGui::SameLine();
            if (thrystr::gui::icon_button(thrystr::gui::icons::kX, "Remove", 22.0f)) {
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

    if (thrystr::gui::ghost_button("+ add")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Add, 1.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (thrystr::gui::ghost_button("- sub")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Subtract, 1.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (thrystr::gui::ghost_button("* mul")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Multiply, 2.0, true});
        mapper_changed = true;
    }
    ImGui::SameLine();
    if (thrystr::gui::ghost_button("/ div")) {
        state.mappers.push_back({thrystr::ValueMapperKind::Divide, 2.0, true});
        mapper_changed = true;
    }

    thrystr::gui::key_value_row("tail", "uint %% 256 -> [-1, 1)");

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
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    thrystr::gui::section_header("Inspector");
    thrystr::gui::section_header("Analysis");
    if (state.fonts.mono) {
        ImGui::PushFont(state.fonts.mono);
    }
    if (state.analysis) {
        const thrystr::Analysis& a = *state.analysis;
        thrystr::gui::key_value_row("source", "%s", format_bytes(a.source_size).c_str());
        thrystr::gui::key_value_row("windows", "%s", format_count(a.window_count).c_str());
        thrystr::gui::key_value_row("offset", "%s", format_count(a.window.offset).c_str());
        thrystr::gui::key_value_row("window samples", "%s", format_count(a.window.length).c_str());
        thrystr::gui::key_value_row("max delta", "%u", static_cast<unsigned>(a.window.max_delta));
        thrystr::gui::key_value_row("x scale", "%.3f", static_cast<double>(a.x_scale));
        thrystr::gui::key_value_row("points", "%s",
                                    format_count(source_sample_count(state)).c_str());
        thrystr::gui::key_value_row("mappers", "%s", format_count(a.mappers.size()).c_str());
    } else {
        thrystr::gui::status_row("state", "empty", thrystr::gui::StatusTone::Muted);
    }

    if (state.analysis) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        thrystr::gui::section_header("Legacy Fit");
        const thrystr::Analysis& a = *state.analysis;
        thrystr::gui::key_value_row("scale", "%.4f", a.wave_scale);
        thrystr::gui::key_value_row("sine hits", "%s", format_count(a.sine.hits).c_str());
        thrystr::gui::key_value_row("sine phase", "%.4f", a.sine.phase_radians);
        thrystr::gui::key_value_row("cos hits", "%s", format_count(a.cosine.hits).c_str());
        thrystr::gui::key_value_row("cos phase", "%.4f", a.cosine.phase_radians);
        thrystr::gui::key_value_row("tested", "%s", format_count(a.sine.tested_points).c_str());
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
    ImGui::PushStyleColor(ImGuiCol_PopupBg, palette::to_vec4(palette::surface::panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, palette::pad::window);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, palette::radii::modal);
    if (ImGui::BeginPopupModal("Settings", &open, ImGuiWindowFlags_NoCollapse)) {
        thrystr::gui::section_header("Overlays");
        thrystr::gui::pill_toggle("Inspector overlay", &state.show_inspector_overlay);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        thrystr::gui::section_header("Render");
        thrystr::gui::pill_toggle("Data points", &state.show_points);
        ImGui::SameLine();
        thrystr::gui::pill_toggle("Data lines", &state.show_lines);
        ImGui::SameLine();
        thrystr::gui::pill_toggle("Reconstruction", &state.show_reconstruction_only);
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
        if (thrystr::gui::accent_button("Done", ImVec2(96.0f, 0.0f))) {
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

void draw_fit_validation_overlay(AppState& state) {
    if (!state.show_fit_validation) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(680.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Fit Validation", &state.show_fit_validation, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const thrystr::app::ValidationReport& report = state.fit_validation;
    thrystr::gui::status_row("coverage", report.pass ? "PASS" : "FAIL",
                             report.pass ? thrystr::gui::StatusTone::Success
                                         : thrystr::gui::StatusTone::Destructive);
    thrystr::gui::key_value_row("sections", "%s",
                                format_count(state.fitted_sections.size()).c_str());
    if (state.fitted_workspace_valid) {
        thrystr::gui::key_value_row("tracks", "%s%s",
                                    format_count(data_track_count(state.fitted_workspace)).c_str(),
                                    state.fitted_workspace_used_parity ? " + parity" : "");
    }
    thrystr::gui::key_value_row("samples", "%s", format_count(report.checked_samples).c_str());
    thrystr::gui::key_value_row("max residual", "%.8f", report.max_residual);
    thrystr::gui::key_value_row("mean residual", "%.8f", report.mean_residual);
    const EncodingEstimate encoding = estimate_encoded_output_size(state);
    if (encoding.encoded_bytes > 0u) {
        thrystr::gui::key_value_row("encoded size", "%s",
                                    format_bytes(encoding.encoded_bytes).c_str());
        thrystr::gui::key_value_row("source ratio", "%s %s",
                                    format_percent(encoding.source_ratio).c_str(),
                                    encoding_target_label(encoding.source_ratio));
        thrystr::gui::key_value_row("covered ratio", "%s",
                                    format_percent(encoding.covered_ratio).c_str());
        thrystr::gui::key_value_row("curve points", "%s",
                                    format_count(encoding.curve_points).c_str());
        thrystr::gui::key_value_row("data sections", "%s",
                                    format_count(encoding.data_sections).c_str());
        thrystr::gui::key_value_row("parity sections", "%s",
                                    format_count(encoding.parity_sections).c_str());
    }

    if (ImGui::BeginTable("##fit_validation_sections", 6,
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
                          ImVec2(0.0f, 190.0f))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("start", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("length", ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableSetupColumn("spacing", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("lambda", ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn("residual", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < state.fitted_sections.size(); ++i) {
            const thrystr::app::Section& section = state.fitted_sections[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i + 1u);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", section.start_index);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", section.length);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4f", section.section_spacing_nm);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.4f", section.wave_wavelength_nm);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextColored(palette::to_vec4(section.max_residual > section.fit_tolerance
                                                    ? palette::status::destructive
                                                    : palette::ink::primary),
                               "%.8f", section.max_residual);
        }
        ImGui::EndTable();
    }

    if (!report.issues.empty()) {
        thrystr::gui::section_header("Issues");
        if (ImGui::BeginChild("##fit_validation_issues", ImVec2(0.0f, 100.0f), false)) {
            for (const thrystr::app::ValidationIssue& issue : report.issues) {
                ImGui::TextWrapped("%s @ sample %zu", issue.message.c_str(), issue.sample_index);
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

void draw_activity_spinner(float radius, ImU32 color) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 center(pos.x + radius, pos.y + radius);
    const double time = ImGui::GetTime();
    for (int i = 0; i < 12; ++i) {
        const float t = static_cast<float>(i) / 12.0f;
        const float angle = static_cast<float>(time * 6.0 + static_cast<double>(i) * 0.523599);
        const float alpha = 0.18f + 0.82f * t;
        const float x = center.x + std::cos(angle) * radius * 0.62f;
        const float y = center.y + std::sin(angle) * radius * 0.62f;
        draw->AddCircleFilled(ImVec2(x, y), 2.4f, palette::with_alpha(color, alpha), 12);
    }
    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));
}

void draw_auto_fit_activity_dialog(AppState& state) {
    const AutoFitProgressSnapshot progress = auto_fit_progress_snapshot(state);
    if (!progress.running) {
        return;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2(std::max(24.0f, viewport.x - kToolboxWidth - 456.0f), kTopChromeHeight + 24.0f),
        ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(430.0f, 360.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin("Auto-Fit Activity", nullptr,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    ImGui::BeginGroup();
    draw_activity_spinner(14.0f, palette::status::info);
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(progress.multi_track ? "Auto-Fit Tracks" : "Auto-Fit All");
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "%s",
                       progress.active_operation.empty() ? "working"
                                                         : progress.active_operation.c_str());
    ImGui::EndGroup();

    const float ratio = progress.total_samples == 0u
                            ? 0.0f
                            : std::clamp(static_cast<float>(progress.processed_samples) /
                                             static_cast<float>(progress.total_samples),
                                         0.0f, 1.0f);
    ImGui::ProgressBar(ratio, ImVec2(-FLT_MIN, 0.0f));
    if (progress.total_segments > 0u) {
        thrystr::gui::key_value_row("segment", "%s/%s",
                                    format_count(progress.completed_segments).c_str(),
                                    format_count(progress.total_segments).c_str());
        thrystr::gui::key_value_row("active", "%s/%s",
                                    format_count(progress.active_segment_index + 1u).c_str(),
                                    format_count(progress.total_segments).c_str());
    }
    if (progress.total_samples > 0u) {
        thrystr::gui::key_value_row("samples", "%s/%s",
                                    format_count(progress.processed_samples).c_str(),
                                    format_count(progress.total_samples).c_str());
    }
    if (progress.active_segment_length > 0u) {
        thrystr::gui::key_value_row(
            "range", "%s..%s", format_count(progress.active_segment_start).c_str(),
            format_count(progress.active_segment_start + progress.active_segment_length - 1u)
                .c_str());
    }
    if (thrystr::gui::ghost_button("Cancel Fit", ImVec2(112.0f, 0.0f))) {
        cancel_auto_fit(state);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    thrystr::gui::section_header("Operations");
    if (ImGui::BeginChild("##auto_fit_activity_log", ImVec2(0.0f, 0.0f), true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        if (state.fonts.mono) {
            ImGui::PushFont(state.fonts.mono);
        }
        for (const std::string& line : progress.activity_log) {
            ImGui::TextUnformatted(line.c_str());
        }
        if (progress.activity_log.empty()) {
            ImGui::TextColored(palette::to_vec4(palette::ink::muted),
                               "waiting for worker activity");
        } else {
            ImGui::SetScrollHereY(1.0f);
        }
        if (state.fonts.mono) {
            ImGui::PopFont();
        }
    }
    ImGui::EndChild();
    ImGui::End();
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
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse);

    const std::array<thrystr::gui::SplashAction, 3> actions = {{
        {"New workspace", "Ctrl+N"},
        {"Open workspace", ""},
        {"Load source", ""},
    }};
    const thrystr::gui::SplashChoice splash_choice = thrystr::gui::splash(
        "thrystr", "Scalar function workspace", {},
        std::span<const thrystr::gui::SplashAction>(actions.data(), actions.size()),
        static_cast<ImTextureID>(state.splash_hero_texture), state.splash_hero_size,
        state.fonts.hero);

    ImGui::End();

    if (splash_choice.kind == thrystr::gui::SplashChoice::Kind::Action) {
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

void draw_splash_titlebar(AppState& state, thrystr::gui::WindowHandle window) {
    const ImVec2 viewport = begin_titlebar_chrome("##splash_titlebar");
    draw_titlebar_background(viewport);
    draw_titlebar_wordmark(state, "/ start");

    ImGui::SetCursorPos(ImVec2(viewport.x - window_control_group_width(1), 8.0f));
    if (window_control_button(WindowControl::Close, "Close", window)) {
        state.request_close = true;
    }

    ImGui::End();
}

StartupAction draw_splash_window(AppState& state, thrystr::gui::WindowHandle window,
                                 const thrystr::gui::ResizeCursorSet& cursors) {
    StartupAction choice = draw_splash(state);
    draw_splash_titlebar(state, window);
    handle_custom_chrome(state, window, cursors, false);
    return choice;
}

bool wave_function_selector(const char* label, WaveFunctionKind* function) {
    bool changed = false;
    const char* current = wave_function_name(*function);
    if (ImGui::BeginCombo(label, current)) {
        for (WaveFunctionKind candidate : kWaveFunctionKinds) {
            const bool selected = candidate == *function;
            if (ImGui::Selectable(wave_function_name(candidate), selected)) {
                *function = candidate;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool property_tab_enabled(const AppState& state, PropertyTab tab, const Entity* selected) {
    switch (tab) {
    case PropertyTab::Entities:
    case PropertyTab::View:
        return true;
    case PropertyTab::Data:
    case PropertyTab::Mappers:
        return data_entity(state) != nullptr || state.analysis.has_value();
    case PropertyTab::Wave:
        return selected && selected->type == EntityType::Wave;
    case PropertyTab::Fit:
    case PropertyTab::Points:
        return state.analysis.has_value();
    }
    return true;
}

const char* property_tab_label(PropertyTab tab) {
    switch (tab) {
    case PropertyTab::Entities:
        return "Layers";
    case PropertyTab::Data:
        return "Data";
    case PropertyTab::Wave:
        return "Function";
    case PropertyTab::Fit:
        return "Fit";
    case PropertyTab::Points:
        return "Points";
    case PropertyTab::Mappers:
        return "Map";
    case PropertyTab::View:
        return "View";
    }
    return "Layers";
}

void draw_property_tabs(AppState& state, const Entity* selected) {
    constexpr std::array<PropertyTab, 7> tabs = {
        PropertyTab::Entities, PropertyTab::Data,    PropertyTab::Wave, PropertyTab::Fit,
        PropertyTab::Points,   PropertyTab::Mappers, PropertyTab::View,
    };
    if (!property_tab_enabled(state, state.property_tab, selected)) {
        state.property_tab = PropertyTab::Entities;
    }

    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const PropertyTab tab = tabs[i];
        const bool enabled = property_tab_enabled(state, tab, selected);
        if (!enabled) {
            ImGui::BeginDisabled();
        }
        if (playback_speed_button(property_tab_label(tab), state.property_tab == tab, 72.0f)) {
            state.property_tab = tab;
        }
        if (!enabled) {
            ImGui::EndDisabled();
        }
        if (i + 1u < tabs.size() && (i % 3u) != 2u) {
            ImGui::SameLine();
        }
    }
}

void draw_entity_toolbox(AppState& state) {
    if (!state.workspace_open) {
        return;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(std::max(0.0f, viewport.x - kToolboxWidth), kTopChromeHeight));
    ImGui::SetNextWindowSize(ImVec2(std::min(kToolboxWidth, viewport.x),
                                    std::max(120.0f, viewport.y - kTopChromeHeight)));
    ImGui::Begin("##toolbox", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse);

    if (!find_entity(state, state.selected_entity_id) && !state.entities.empty()) {
        select_entity(state, state.entities.front().id);
    }
    Entity* selected = find_entity(state, state.selected_entity_id);

    thrystr::gui::section_header("Properties");
    draw_property_tabs(state, selected);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    Entity* data = data_entity(state);
    switch (state.property_tab) {
    case PropertyTab::Entities:
        thrystr::gui::section_header("Layers");
        if (thrystr::gui::accent_button("+ Function", ImVec2(112.0f, 0.0f))) {
            create_wave_entity(state);
        }
        ImGui::SameLine();
        if (thrystr::gui::ghost_button("Fit Function", ImVec2(116.0f, 0.0f))) {
            create_interpolated_wave(state);
        }
        ImGui::SameLine();
        if (thrystr::gui::ghost_button("Section", ImVec2(92.0f, 0.0f))) {
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
                selected = &entity;
            }
            ImGui::PopID();
        }
        selected = find_entity(state, state.selected_entity_id);
        if (selected) {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            thrystr::gui::section_header("Selection");
            sync_entity_name(state);
            if (ImGui::InputText("Name", state.entity_name, sizeof(state.entity_name))) {
                selected->name = state.entity_name;
            }
            thrystr::gui::pill_toggle("Visible", &selected->visible);
        }
        break;

    case PropertyTab::Data:
        thrystr::gui::section_header("Data");
        if (state.analysis) {
            const thrystr::Analysis& a = *state.analysis;
            thrystr::gui::key_value_row("points", "%s",
                                        format_count(source_sample_count(state)).c_str());
            thrystr::gui::key_value_row("loaded blocks", "%s",
                                        format_count(state.lazy_blocks.size()).c_str());
            thrystr::gui::key_value_row("block size", "%s",
                                        format_bytes(lazy_block_bytes(state)).c_str());
            thrystr::gui::key_value_row("source", "%s", format_bytes(a.source_size).c_str());
        } else {
            thrystr::gui::status_row("source", "none", thrystr::gui::StatusTone::Muted);
        }
        if (data && value_bar_double("point spacing nm", &data->data.spatial_period_nm, 0.01,
                                     "%.6f", state.fonts.mono)) {
            data->data.spatial_period_nm = std::max(0.000001, data->data.spatial_period_nm);
        }
        if (data) {
            data->data.sample_bits = normalize_sample_bits(data->data.sample_bits);
            thrystr::gui::key_value_row("point width", "%u-bit",
                                        static_cast<unsigned>(data->data.sample_bits));
            bool sample_width_changed = false;
            for (const std::uint8_t bits :
                 {std::uint8_t{8}, std::uint8_t{16}, std::uint8_t{32}, std::uint8_t{64}}) {
                if (bits != 8u) {
                    ImGui::SameLine();
                }
                const std::string label = format_count(bits);
                if (playback_speed_button(label.c_str(), data->data.sample_bits == bits, 42.0f)) {
                    data->data.sample_bits = bits;
                    sample_width_changed = true;
                }
            }
            if (sample_width_changed) {
                state.fitted_sections.clear();
                state.fitted_workspace = {};
                state.fitted_workspace_valid = false;
                state.fitted_workspace_used_parity = false;
                state.fit_validation = {};
                clear_lazy_cache(state);
                reload_if_file_selected(state);
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        thrystr::gui::section_header("Load Params");
        {
            bool params_changed = false;
            bool lazy_block_changed = false;
            double max_slope = static_cast<double>(state.max_slope);
            double wave_tolerance = static_cast<double>(state.wave_tolerance);
            params_changed |=
                value_bar_double("max slope", &max_slope, 0.001, "%.4f", state.fonts.mono);
            params_changed |= value_bar_double("wave tolerance", &wave_tolerance, 0.0001, "%.5f",
                                               state.fonts.mono);
            params_changed |=
                value_bar_int("phase steps", &state.phase_steps, 4.0, state.fonts.mono);
            params_changed |=
                value_bar_int("phase samples", &state.phase_test_points, 512.0, state.fonts.mono);
            lazy_block_changed |=
                value_bar_int("lazy block MiB", &state.lazy_block_mib, 0.05, state.fonts.mono);
            state.max_slope = static_cast<float>(max_slope);
            state.wave_tolerance = static_cast<float>(wave_tolerance);
            if (lazy_block_changed) {
                state.lazy_block_mib = std::clamp(state.lazy_block_mib, 1, 10);
                reload_if_file_selected(state);
            }
            if (params_changed) {
                refresh_analysis_params(state);
            }
            if (thrystr::gui::ghost_button("Fit Phases", ImVec2(112.0f, 0.0f))) {
                fit_wave_phases(state);
            }
        }
        break;

    case PropertyTab::Wave:
        if (!selected || selected->type != EntityType::Wave) {
            thrystr::gui::status_row("function", "select a function",
                                     thrystr::gui::StatusTone::Muted);
            break;
        }
        thrystr::gui::section_header("Function");
        sync_entity_name(state);
        if (ImGui::InputText("Name", state.entity_name, sizeof(state.entity_name))) {
            selected->name = state.entity_name;
        }
        thrystr::gui::pill_toggle("Visible", &selected->visible);
        wave_function_selector("Function", &selected->wave.function);
        thrystr::gui::pill_toggle("Secondary function", &selected->wave.use_secondary_function);
        if (selected->wave.use_secondary_function) {
            wave_function_selector("Secondary", &selected->wave.secondary_function);
            value_bar_double("secondary amplitude", &selected->wave.secondary_amplitude, 0.01,
                             "%.4f", state.fonts.mono);
        }
        value_bar_double("function spatial distance nm", &selected->wave.wavelength_nm, 0.10,
                         "%.4f", state.fonts.mono);
        value_bar_double("amplitude", &selected->wave.amplitude, 0.01, "%.4f", state.fonts.mono);
        value_bar_double("amplitude offset", &selected->wave.amplitude_offset, 0.01, "%.4f",
                         state.fonts.mono);
        value_bar_double("phase nm", &selected->wave.phase_nm, 0.10, "%.4f", state.fonts.mono);
        value_bar_double("z rotation deg", &selected->wave.rotation_degrees, 0.25, "%.3f",
                         state.fonts.mono);
        thrystr::gui::key_value_row(
            "modifiers", "%s", format_count(selected->wave.wavelength_modifiers.size()).c_str());
        if (thrystr::gui::ghost_button("Fit Selection", ImVec2(124.0f, 0.0f))) {
            const WaveFitResult fit = fit_wave_to_selection(state, selected->wave);
            selected->wave.wavelength_nm = fit.wavelength_nm;
            selected->wave.phase_nm = fit.phase_nm;
            rebuild_wavelength_modifiers(state, selected->wave);
            const auto [first, last] = analysis_selection_bounds(state);
            const WaveFitResult final_fit = score_wave_on_range(state, selected->wave, first, last);
            state.status = "Fitted " + selected->name + ": " + format_count(final_fit.hits) + "/" +
                           format_count(final_fit.tested) + " point hits, " +
                           format_count(selected->wave.wavelength_modifiers.size()) +
                           " wavelength keys";
        }
        ImGui::SameLine();
        if (thrystr::gui::ghost_button("Fit Points", ImVec2(104.0f, 0.0f))) {
            fit_selected_points_to_wave(state);
        }
        break;

    case PropertyTab::Fit:
        thrystr::gui::section_header("X-Line Section");
        if (state.analysis && source_sample_count(state) > 0u) {
            if (!state.segment.active) {
                if (thrystr::gui::accent_button("Create Section", ImVec2(132.0f, 0.0f))) {
                    create_section(state);
                }
            } else {
                clamp_segment(state);
                const double period = data_spatial_period_nm(state);
                double start_index = static_cast<double>(state.segment.selection_start);
                double end_index = static_cast<double>(state.segment.selection_end);
                bool changed = false;
                changed |=
                    value_bar_double("start index", &start_index, 0.25, "%.0f", state.fonts.mono);
                changed |=
                    value_bar_double("end index", &end_index, 0.25, "%.0f", state.fonts.mono);
                if (changed) {
                    const std::size_t count = source_sample_count(state);
                    state.segment.selection_start = clamp_index(start_index, count);
                    state.segment.selection_end = clamp_index(end_index, count);
                    clamp_segment(state);
                }
                double start_nm = static_cast<double>(state.segment.selection_start) * period;
                double end_nm = static_cast<double>(state.segment.selection_end) * period;
                bool nm_changed = false;
                nm_changed |= value_bar_double("start nm", &start_nm, period * 0.25, "%.3f",
                                               state.fonts.mono);
                nm_changed |=
                    value_bar_double("end nm", &end_nm, period * 0.25, "%.3f", state.fonts.mono);
                if (nm_changed) {
                    const std::size_t count = source_sample_count(state);
                    state.segment.selection_start = clamp_index(start_nm / period, count);
                    state.segment.selection_end = clamp_index(end_nm / period, count);
                    clamp_segment(state);
                }
                const auto [first, last] = normalized_selection(state);
                thrystr::gui::key_value_row("span", "%.3f nm",
                                            static_cast<double>(last - first) * period);
                if (thrystr::gui::ghost_button("Select All", ImVec2(104.0f, 0.0f))) {
                    create_section(state);
                }
                ImGui::SameLine();
                if (state.auto_fit_job.running.load()) {
                    if (thrystr::gui::ghost_button("Cancel Fit", ImVec2(108.0f, 0.0f))) {
                        cancel_auto_fit(state);
                    }
                    thrystr::gui::status_row("auto-fit", "running", thrystr::gui::StatusTone::Info);
                    std::size_t completed_segments = 0u;
                    std::size_t total_segments = 0u;
                    std::size_t processed_samples = 0u;
                    std::size_t total_samples = 0u;
                    std::size_t active_start = 0u;
                    std::size_t active_length = 0u;
                    std::string active_operation;
                    {
                        std::scoped_lock lock(state.auto_fit_job.result_mutex);
                        completed_segments = state.auto_fit_job.completed_segments;
                        total_segments = state.auto_fit_job.total_segments;
                        processed_samples = state.auto_fit_job.processed_samples;
                        total_samples = state.auto_fit_job.total_samples;
                        active_start = state.auto_fit_job.active_segment_start;
                        active_length = state.auto_fit_job.active_segment_length;
                        active_operation = state.auto_fit_job.active_operation;
                    }
                    if (!active_operation.empty()) {
                        thrystr::gui::key_value_row("operation", "%s", active_operation.c_str());
                    }
                    if (total_segments > 0u) {
                        const std::string segment_text =
                            format_count(completed_segments) + "/" + format_count(total_segments);
                        thrystr::gui::key_value_row("segments", "%s", segment_text.c_str());
                    }
                    if (total_samples > 0u) {
                        const std::string sample_text =
                            format_count(processed_samples) + "/" + format_count(total_samples);
                        thrystr::gui::key_value_row("samples", "%s", sample_text.c_str());
                    }
                    if (active_length > 0u) {
                        thrystr::gui::key_value_row(
                            "active range", "%s..%s", format_count(active_start).c_str(),
                            format_count(active_start + active_length - 1u).c_str());
                    }
                } else if (thrystr::gui::accent_button("Auto-Fit All", ImVec2(126.0f, 0.0f))) {
                    auto_fit_current_range(state);
                }
                ImGui::SameLine();
                if (!state.auto_fit_job.running.load() &&
                    thrystr::gui::ghost_button("Auto-Fit Tracks", ImVec2(138.0f, 0.0f))) {
                    auto_fit_current_range(state, true);
                }
                if (!state.fitted_sections.empty()) {
                    const EncodingEstimate encoding = estimate_encoded_output_size(state);
                    thrystr::gui::key_value_row("fit sections", "%s",
                                                format_count(state.fitted_sections.size()).c_str());
                    if (state.fitted_workspace_valid) {
                        thrystr::gui::key_value_row(
                            "fit tracks", "%s%s", format_count(encoding.data_tracks).c_str(),
                            state.fitted_workspace_used_parity ? " + parity" : "");
                    }
                    thrystr::gui::key_value_row(
                        "fit samples", "%s",
                        format_count(state.fit_validation.checked_samples).c_str());
                    thrystr::gui::key_value_row("fit residual", "%.6f",
                                                state.fit_validation.max_residual);
                    if (encoding.encoded_bytes > 0u) {
                        thrystr::gui::key_value_row("encoded size", "%s",
                                                    format_bytes(encoding.encoded_bytes).c_str());
                        thrystr::gui::key_value_row("source ratio", "%s %s",
                                                    format_percent(encoding.source_ratio).c_str(),
                                                    encoding_target_label(encoding.source_ratio));
                        thrystr::gui::key_value_row("curve points", "%s",
                                                    format_count(encoding.curve_points).c_str());
                        thrystr::gui::key_value_row("parity sections", "%s",
                                                    format_count(encoding.parity_sections).c_str());
                    }
                    if (thrystr::gui::ghost_button("Validate Fit", ImVec2(118.0f, 0.0f))) {
                        state.fit_validation = validate_fitted_sections(state);
                        state.show_fit_validation = true;
                    }
                    ImGui::SameLine();
                    thrystr::gui::pill_toggle("Reconstruction", &state.show_reconstruction_only);
                }
            }
        } else {
            thrystr::gui::status_row("section", "no data", thrystr::gui::StatusTone::Muted);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        thrystr::gui::section_header("Selected Point Fit");
        thrystr::gui::key_value_row("selected points", "%s",
                                    format_count(state.selected_data_points.size()).c_str());
        thrystr::gui::pill_toggle("Function combinations", &state.fit_function_combinations);
        if (thrystr::gui::accent_button("Fit Selected Points", ImVec2(158.0f, 0.0f))) {
            fit_selected_points_to_wave(state);
        }
        break;

    case PropertyTab::Points:
        thrystr::gui::section_header("Point Selection");
        thrystr::gui::pill_toggle("Point pick mode", &state.point_selection_mode);
        if (state.point_selection_mode) {
            state.segment_selection_mode = false;
        }
        thrystr::gui::key_value_row("selected", "%s",
                                    format_count(state.selected_data_points.size()).c_str());
        if (!state.selected_data_points.empty()) {
            normalize_selected_data_points(state);
            thrystr::gui::key_value_row("first", "%s",
                                        format_count(state.selected_data_points.front()).c_str());
            thrystr::gui::key_value_row("last", "%s",
                                        format_count(state.selected_data_points.back()).c_str());
        }
        if (thrystr::gui::ghost_button("Clear Points", ImVec2(118.0f, 0.0f))) {
            state.selected_data_points.clear();
        }
        ImGui::SameLine();
        if (thrystr::gui::accent_button("Fit Points", ImVec2(104.0f, 0.0f))) {
            fit_selected_points_to_wave(state);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        thrystr::gui::section_header("Random Sampling");
        value_bar_int("sample count", &state.random_sample_count, 1.0, state.fonts.mono);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputScalar("seed", ImGuiDataType_U64, &state.random_sample_seed);
        if (thrystr::gui::accent_button("Load Random Sampling", ImVec2(176.0f, 0.0f))) {
            select_random_sample_points(state);
        }
        break;

    case PropertyTab::Mappers:
        draw_value_mapper_stack(state);
        break;

    case PropertyTab::View:
        thrystr::gui::section_header("Render");
        thrystr::gui::pill_toggle("Data points", &state.show_points);
        ImGui::SameLine();
        thrystr::gui::pill_toggle("Data lines", &state.show_lines);
        ImGui::SameLine();
        thrystr::gui::pill_toggle("Reconstruction", &state.show_reconstruction_only);
        {
            double zoom_x = static_cast<double>(state.zoom_x);
            double zoom_y = static_cast<double>(state.zoom_y);
            if (value_bar_double("x zoom", &zoom_x, 0.01, "%.2f", state.fonts.mono)) {
                state.zoom_x = static_cast<float>(zoom_x);
            }
            if (value_bar_double("y zoom", &zoom_y, 0.01, "%.2f", state.fonts.mono)) {
                state.zoom_y = static_cast<float>(zoom_y);
            }
        }
        thrystr::gui::pill_toggle("Wheel scroll", &state.wheel_scroll_mode);
        break;
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

void draw_data_point_markers(ImDrawList* draw, AppState& state, std::size_t first, std::size_t last,
                             float plot_left, float plot_top, float plot_bottom, float x_step,
                             float y_zoom, const ImVec2& clip_min, const ImVec2& clip_max) {
    const std::size_t count = source_sample_count(state);
    if (!draw || count == 0u || last < first) {
        return;
    }

    if (x_step >= 3.0f) {
        for (std::size_t i = first; i <= last && i < count; ++i) {
            const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, i);
            if (!scalar) {
                continue;
            }
            const ImVec2 point(
                plot_left + static_cast<float>(i) * x_step,
                y_for_value(static_cast<float>(*scalar), plot_top, plot_bottom, y_zoom));
            draw->AddCircleFilled(point, 2.2f, kDataPointHi);
        }
        return;
    }

    const int column_count =
        std::max(1, static_cast<int>(std::ceil(std::max(1.0f, clip_max.x - clip_min.x))));
    state.plot_min_y.assign(static_cast<std::size_t>(column_count), FLT_MAX);
    state.plot_max_y.assign(static_cast<std::size_t>(column_count), -FLT_MAX);

    for (std::size_t i = first; i <= last && i < count; ++i) {
        const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, i);
        if (!scalar) {
            continue;
        }
        const float x = plot_left + static_cast<float>(i) * x_step;
        if (x < clip_min.x || x > clip_max.x) {
            continue;
        }
        const int column =
            std::clamp(static_cast<int>(std::floor(x - clip_min.x)), 0, column_count - 1);
        const float y = y_for_value(static_cast<float>(*scalar), plot_top, plot_bottom, y_zoom);
        state.plot_min_y[static_cast<std::size_t>(column)] =
            std::min(state.plot_min_y[static_cast<std::size_t>(column)], y);
        state.plot_max_y[static_cast<std::size_t>(column)] =
            std::max(state.plot_max_y[static_cast<std::size_t>(column)], y);
    }

    for (int column = 0; column < column_count; ++column) {
        const std::size_t index = static_cast<std::size_t>(column);
        if (state.plot_min_y[index] == FLT_MAX) {
            continue;
        }
        const float x = clip_min.x + static_cast<float>(column) + 0.5f;
        if (std::abs(state.plot_max_y[index] - state.plot_min_y[index]) < 1.0f) {
            draw->AddRectFilled(ImVec2(x - 0.5f, state.plot_min_y[index] - 0.5f),
                                ImVec2(x + 0.5f, state.plot_min_y[index] + 0.5f), kDataPointMed);
        } else {
            draw->AddLine(ImVec2(x, state.plot_min_y[index]), ImVec2(x, state.plot_max_y[index]),
                          kDataPointLo, 1.0f);
        }
    }
}

void draw_selected_data_point_markers(ImDrawList* draw, const AppState& state, std::size_t first,
                                      std::size_t last, float plot_left, float plot_top,
                                      float plot_bottom, float x_step, float y_zoom) {
    if (!draw || state.selected_data_points.empty()) {
        return;
    }
    const auto lower = std::lower_bound(state.selected_data_points.begin(),
                                        state.selected_data_points.end(), first);
    const auto upper = std::upper_bound(state.selected_data_points.begin(),
                                        state.selected_data_points.end(), last);
    for (auto it = lower; it != upper; ++it) {
        const std::size_t index = *it;
        if (const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, index)) {
            const float x = plot_left + static_cast<float>(index) * x_step;
            const float y = y_for_value(static_cast<float>(*scalar), plot_top, plot_bottom, y_zoom);
            draw->AddCircleFilled(ImVec2(x, y), 5.0f,
                                  palette::with_alpha(palette::accents::gold, 0.92f));
            draw->AddCircle(ImVec2(x, y), 7.0f, palette::ink::primary, 0, 1.4f);
        }
    }
}

void flush_plot_points(ImDrawList* draw, std::vector<ImVec2>& points, ImU32 color,
                       float thickness) {
    thrystr::gui::stroke_polyline(draw, points, color, thickness);
    points.clear();
}

void append_data_polyline(AppState& state, ImDrawList* draw, std::size_t first, std::size_t last,
                          float plot_left, float plot_top, float plot_bottom, float x_step,
                          float y_zoom) {
    state.plot_points.clear();
    state.plot_points.reserve(last >= first ? last - first + 1u : 0u);
    for (std::size_t i = first; i <= last && i < source_sample_count(state); ++i) {
        const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, i);
        if (!scalar) {
            flush_plot_points(draw, state.plot_points, kDataLineColor, 1.2f);
            continue;
        }
        state.plot_points.emplace_back(
            plot_left + static_cast<float>(i) * x_step,
            y_for_value(static_cast<float>(*scalar), plot_top, plot_bottom, y_zoom));
    }
    flush_plot_points(draw, state.plot_points, kDataLineColor, 1.2f);
}

void append_wave_polyline(AppState& state, const WaveEntity& wave, std::size_t first,
                          std::size_t last, double period_nm, float plot_left, float plot_top,
                          float plot_bottom, float x_step, float y_zoom) {
    state.plot_points.clear();
    const std::size_t samples = std::min<std::size_t>(4096, last - first + 1u);
    state.plot_points.reserve(samples);
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const double t = samples > 1u ? static_cast<double>(sample) / (samples - 1u) : 0.0;
        const double index = static_cast<double>(first) + t * static_cast<double>(last - first);
        const double value = wave_value_at_nm(wave, index * period_nm);
        state.plot_points.emplace_back(
            plot_left + static_cast<float>(index) * x_step,
            y_for_value(static_cast<float>(value), plot_top, plot_bottom, y_zoom));
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
        set_playhead_index(
            state, amount > state.playhead_index ? 0u : state.playhead_index - amount, true);
        return;
    }
    const std::size_t amount = static_cast<std::size_t>(delta);
    const std::size_t max_index = count - 1u;
    const std::size_t clamped_playhead = std::min(state.playhead_index, max_index);
    const std::size_t remaining = max_index - clamped_playhead;
    set_playhead_index(state, amount >= remaining ? max_index : clamped_playhead + amount, true);
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
    errno = 0;
    char* end = nullptr;
    const double value = std::strtod(state.custom_playback_speed_text, &end);
    while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) {
        ++end;
    }
    if (end != state.custom_playback_speed_text && end != nullptr && *end == '\0' &&
        errno != ERANGE && std::isfinite(value) && value > 0.0) {
        state.playback_points_per_second = value;
    }
}

bool playback_speed_button(const char* label, bool selected, float width) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, palette::to_vec4(palette::status::info));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette::to_vec4(palette::status::info));
        ImGui::PushStyleColor(ImGuiCol_Text, palette::to_vec4(palette::surface::window));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, palette::to_vec4(palette::surface::control));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              palette::to_vec4(palette::surface::control_hi));
        ImGui::PushStyleColor(ImGuiCol_Text, palette::to_vec4(palette::ink::primary));
    }
    ImGui::PushID(label);
    const bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    return clicked;
}

void draw_playback_speed_controls(AppState& state) {
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "speed");
    for (double preset : kPlaybackSpeedPresets) {
        ImGui::SameLine();
        char label[16] = {};
        std::snprintf(label, sizeof(label), "%.0f", preset);
        const bool selected = !state.custom_playback_speed &&
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
                          sizeof(state.custom_playback_speed_text), "%.3g",
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
        if (ImGui::InputText("##custom_playback_pps", state.custom_playback_speed_text,
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

std::optional<std::uint64_t> ticker_sample_at(const AppState& state, std::size_t index) {
    if (const std::optional<std::uint64_t> sample = raw_sample_at(state, index)) {
        return sample;
    }
    if (const std::optional<thrystr::app::Scalar> scalar = scalar_at(state, index)) {
        return sample_from_scalar(*scalar, data_sample_bits(state));
    }
    return std::nullopt;
}

const thrystr::app::Section* section_for_index(std::span<const thrystr::app::Section> sections,
                                               std::size_t index) {
    for (const thrystr::app::Section& section : sections) {
        if (thrystr::app::section_contains(section, index)) {
            return &section;
        }
    }
    return nullptr;
}

std::optional<double> parity_wave_value_at(const thrystr::app::WorkspaceModel& workspace,
                                           std::size_t index) {
    if (workspace.parity_track_id == thrystr::app::kNoParityTrack) {
        return std::nullopt;
    }
    const thrystr::app::Track* parity =
        thrystr::app::find_track(workspace, workspace.parity_track_id);
    if (!parity) {
        return std::nullopt;
    }
    const thrystr::app::Section* section = section_for_index(parity->sections, index);
    if (!section) {
        return std::nullopt;
    }
    return thrystr::app::wave_value_at_index(*section, index);
}

struct FitReadout {
    bool available = false;
    std::uint8_t track_id = 0u;
    std::optional<double> wave_scalar;
    std::optional<double> parity_scalar;
};

FitReadout fit_readout_at(const AppState& state, std::size_t index) {
    FitReadout readout;
    if (!state.fitted_workspace_valid || state.fitted_workspace.tracks.empty()) {
        return readout;
    }

    const thrystr::app::WorkspaceModel& workspace = state.fitted_workspace;
    const std::uint8_t owner = workspace.parity_track_id == thrystr::app::kNoParityTrack
                                   ? 0u
                                   : thrystr::app::parity_owner_at(workspace, index);
    const thrystr::app::Track* track = thrystr::app::find_track(workspace, owner);
    if (!track || track->kind != thrystr::app::TrackKind::Data) {
        return readout;
    }
    if (!track->owned_mask.empty() && !thrystr::app::get_owned_bit(track->owned_mask, index)) {
        return readout;
    }

    const thrystr::app::Section* section = section_for_index(track->sections, index);
    if (!section) {
        return readout;
    }
    readout.available = true;
    readout.track_id = owner;
    readout.wave_scalar = thrystr::app::wave_value_at_index(*section, index);
    readout.parity_scalar = parity_wave_value_at(workspace, index);
    return readout;
}

void draw_data_ticker(const AppState& state, ImDrawList* draw, std::size_t first, std::size_t last,
                      float plot_left, float plot_bottom, float x_step, const ImVec2& clip_min,
                      const ImVec2& clip_max) {
    if (!draw || source_sample_count(state) == 0u || last < first) {
        return;
    }

    const float ticker_top = plot_bottom + 58.0f;
    const float ticker_bottom = clip_max.y - 8.0f;
    if (ticker_bottom <= ticker_top) {
        return;
    }

    draw->AddRectFilled(ImVec2(clip_min.x, ticker_top - 4.0f), ImVec2(clip_max.x, ticker_bottom),
                        palette::surface::panel_alt);
    draw->AddLine(ImVec2(clip_min.x, ticker_top - 4.0f), ImVec2(clip_max.x, ticker_top - 4.0f),
                  palette::border::separator, 1.0f);

    const std::size_t stride = thrystr::gui::pixel_stride(x_step, kTickerTargetLabelPixels);
    const std::size_t start = first - (first % stride);
    for (std::size_t i = start; i <= last && i < source_sample_count(state); i += stride) {
        if (i == state.playhead_index) {
            continue;
        }
        const std::optional<std::uint64_t> byte = ticker_sample_at(state, i);
        if (!byte) {
            continue;
        }
        const std::string label = hex_sample_text(*byte, data_sample_bits(state));
        const float x = plot_left + static_cast<float>(i) * x_step;
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(x - text_size.x * 0.5f, ticker_top), palette::ink::muted,
                      label.c_str());
    }

    if (state.playhead_index < first || state.playhead_index > last) {
        return;
    }
    const std::optional<std::uint64_t> current_byte = ticker_sample_at(state, state.playhead_index);
    if (!current_byte) {
        return;
    }

    const std::string label = hex_sample_text(*current_byte, data_sample_bits(state));
    const float x = plot_left + static_cast<float>(state.playhead_index) * x_step;
    ImFont* current_font = state.fonts.mono ? state.fonts.mono : ImGui::GetFont();
    const float current_font_size = ImGui::GetFontSize() * kTickerCurrentScale;
    const ImVec2 text_size =
        current_font->CalcTextSizeA(current_font_size, FLT_MAX, 0.0f, label.c_str());
    const ImVec2 text_pos(x - text_size.x * 0.5f, ticker_top);
    draw->AddRectFilled(ImVec2(text_pos.x - 6.0f, text_pos.y - 3.0f),
                        ImVec2(text_pos.x + text_size.x + 6.0f, text_pos.y + text_size.y + 3.0f),
                        palette::surface::control_hi, palette::radii::ctrl);
    draw->AddText(current_font, current_font_size, text_pos, palette::ink::primary, label.c_str());
}

void draw_parity_strip(ImDrawList* draw, const AppState& state, std::size_t first, std::size_t last,
                       float plot_left, float plot_bottom, float x_step, const ImVec2& clip_min,
                       const ImVec2& clip_max) {
    if (!draw || !state.fitted_workspace_valid ||
        state.fitted_workspace.parity_track_id == thrystr::app::kNoParityTrack || last < first) {
        return;
    }

    const std::size_t track_span = data_track_span(state.fitted_workspace);
    if (track_span < 2u) {
        return;
    }

    const float strip_top = plot_bottom + 14.0f;
    const float strip_bottom = std::min(plot_bottom + 46.0f, clip_max.y - 40.0f);
    if (strip_bottom <= strip_top + 4.0f) {
        return;
    }

    draw->AddRectFilled(ImVec2(clip_min.x, strip_top), ImVec2(clip_max.x, strip_bottom),
                        palette::surface::panel_alt, palette::radii::ctrl);
    draw->AddText(ImVec2(clip_min.x + 8.0f, strip_top + 8.0f), palette::ink::muted, "parity");

    const float graph_left = clip_min.x + 58.0f;
    const float graph_right = clip_max.x - 12.0f;
    const double denom = static_cast<double>(std::max<std::size_t>(1u, track_span - 1u));
    const auto y_for_track = [&](double value) {
        const double normalized = std::clamp(value / denom, 0.0, 1.0);
        return strip_bottom - static_cast<float>(normalized) * (strip_bottom - strip_top);
    };

    for (std::size_t track = 0; track < track_span; ++track) {
        const float y = y_for_track(static_cast<double>(track));
        draw->AddLine(ImVec2(graph_left, y), ImVec2(graph_right, y), palette::border::separator,
                      1.0f);
    }

    const std::size_t samples = std::min<std::size_t>(2048u, last - first + 1u);
    ImVec2 previous{};
    bool has_previous = false;
    for (std::size_t sample = 0; sample < samples; ++sample) {
        const double t =
            samples > 1u ? static_cast<double>(sample) / static_cast<double>(samples - 1u) : 0.0;
        const std::size_t index = static_cast<std::size_t>(
            std::llround(static_cast<double>(first) + t * static_cast<double>(last - first)));
        const std::optional<double> parity = parity_wave_value_at(state.fitted_workspace, index);
        if (!parity) {
            has_previous = false;
            continue;
        }
        const ImVec2 point(plot_left + static_cast<float>(index) * x_step, y_for_track(*parity));
        if (has_previous) {
            draw->AddLine(previous, point, palette::accents::gold, 1.35f);
        }
        previous = point;
        has_previous = true;
    }

    for (std::size_t index = first; index <= last && index < source_sample_count(state);
         index += std::max<std::size_t>(1u, (last - first + 1u) / 256u)) {
        const std::uint8_t owner = thrystr::app::parity_owner_at(state.fitted_workspace, index);
        if (owner >= track_span) {
            continue;
        }
        const float x = plot_left + static_cast<float>(index) * x_step;
        draw->AddLine(ImVec2(x, y_for_track(static_cast<double>(owner)) - 3.0f),
                      ImVec2(x, y_for_track(static_cast<double>(owner)) + 3.0f), kSelectionHandle,
                      1.0f);
    }
}

void draw_auto_fit_progress_overlay(ImDrawList* draw, const AutoFitProgressSnapshot& progress,
                                    std::size_t first, std::size_t last, float plot_left,
                                    float plot_top, float plot_bottom, float x_step,
                                    const ImVec2& clip_min, const ImVec2& clip_max) {
    if (!draw || !progress.running) {
        return;
    }

    const float bar_left = clip_min.x + 58.0f;
    const float bar_right = clip_max.x - 24.0f;
    const float bar_top = plot_top - 20.0f;
    const float bar_bottom = plot_top - 10.0f;
    if (bar_right > bar_left && bar_bottom > clip_min.y) {
        const float ratio = progress.total_samples == 0u
                                ? 0.0f
                                : std::clamp(static_cast<float>(progress.processed_samples) /
                                                 static_cast<float>(progress.total_samples),
                                             0.0f, 1.0f);
        draw->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(bar_right, bar_bottom),
                            palette::surface::control_hi, palette::radii::pill);
        draw->AddRectFilled(ImVec2(bar_left, bar_top),
                            ImVec2(bar_left + (bar_right - bar_left) * ratio, bar_bottom),
                            palette::status::info, palette::radii::pill);
        const std::string label =
            (progress.progress_label.empty() ? std::string("auto-fit") : progress.progress_label) +
            "  " + format_count(progress.processed_samples) + "/" +
            format_count(progress.total_samples) + " samples";
        draw->AddText(ImVec2(bar_left, bar_top - 17.0f), palette::ink::muted, label.c_str());
    }

    if (progress.active_segment_length == 0u || last < first) {
        return;
    }
    const std::size_t active_first = progress.active_segment_start;
    const std::size_t active_last =
        progress.active_segment_start + progress.active_segment_length - 1u;
    if (active_last < first || active_first > last) {
        return;
    }

    const std::size_t visible_first = std::max(first, active_first);
    const std::size_t visible_last = std::min(last, active_last);
    const float x0 = plot_left + static_cast<float>(visible_first) * x_step;
    const float x1 = plot_left + static_cast<float>(visible_last) * x_step;
    const ImU32 fill = palette::with_alpha(palette::status::info, 42.0f / 255.0f);
    const ImU32 edge = palette::with_alpha(palette::status::info, 0.92f);
    draw->AddRectFilled(ImVec2(x0, plot_top), ImVec2(x1, plot_bottom), fill);
    draw->AddRect(ImVec2(x0, plot_top), ImVec2(x1, plot_bottom), edge, 0.0f, 0, 1.8f);

    const std::string operation =
        progress.active_operation.empty() ? std::string("working") : progress.active_operation;
    draw->AddText(ImVec2(std::max(x0 + 8.0f, clip_min.x + 64.0f), plot_top + 10.0f), edge,
                  operation.c_str());
}

void draw_fitted_sections(ImDrawList* draw, const AppState& state, std::size_t first,
                          std::size_t last, float plot_left, float plot_top, float plot_bottom,
                          float x_step, float y_zoom) {
    if (!draw || state.fitted_sections.empty() || last < first) {
        return;
    }

    if (state.fitted_workspace_valid && !state.fitted_workspace.tracks.empty()) {
        const std::size_t track_count =
            std::max<std::size_t>(1u, data_track_count(state.fitted_workspace));
        const float lane_gap = 3.0f;
        const float lane_top = plot_top + 10.0f;
        const float lane_height =
            std::max(5.0f, std::min(18.0f, ((plot_bottom - plot_top) * 0.24f) /
                                               static_cast<float>(track_count)));
        std::size_t data_track_order = 0u;
        for (const thrystr::app::Track& track : state.fitted_workspace.tracks) {
            if (track.kind != thrystr::app::TrackKind::Data || !track.visible) {
                continue;
            }
            const float lane_y0 =
                lane_top + static_cast<float>(data_track_order) * (lane_height + lane_gap);
            const float lane_y1 = lane_y0 + lane_height;
            const ImU32 base_color = kWaveColors[data_track_order % kWaveColors.size()];
            const ImU32 fill = palette::with_alpha(base_color, 30.0f / 255.0f);
            const ImU32 edge = palette::with_alpha(base_color, 0.86f);
            char label[24] = {};
            std::snprintf(label, sizeof(label), "track %u", static_cast<unsigned>(track.id));
            draw->AddText(ImVec2(plot_left + 8.0f, lane_y0 - 1.0f), edge, label);

            for (const thrystr::app::Section& section : track.sections) {
                if (section.length == 0u) {
                    continue;
                }
                const std::size_t section_first = section.start_index;
                const std::size_t section_last =
                    static_cast<std::size_t>(thrystr::app::section_end(section) - 1u);
                if (section_last < first || section_first > last) {
                    continue;
                }

                const std::size_t visible_first = std::max(first, section_first);
                const std::size_t visible_last = std::min(last, section_last);
                const float x0 = plot_left + static_cast<float>(section_first) * x_step;
                const float x1 = plot_left + static_cast<float>(section_last) * x_step;
                draw->AddRectFilled(ImVec2(x0, lane_y0), ImVec2(x1, lane_y1), fill);
                draw->AddRect(ImVec2(x0, lane_y0), ImVec2(x1, lane_y1), edge, 0.0f, 0, 1.0f);

                const std::size_t samples =
                    std::min<std::size_t>(1024u, visible_last - visible_first + 1u);
                ImVec2 previous{};
                bool has_previous = false;
                for (std::size_t sample = 0; sample < samples; ++sample) {
                    const double t = samples > 1u ? static_cast<double>(sample) /
                                                        static_cast<double>(samples - 1u)
                                                  : 0.0;
                    const double index = static_cast<double>(visible_first) +
                                         t * static_cast<double>(visible_last - visible_first);
                    const double value = thrystr::app::wave_value_at_index(
                        section, static_cast<std::size_t>(std::llround(index)));
                    const ImVec2 point(
                        plot_left + static_cast<float>(index) * x_step,
                        y_for_value(static_cast<float>(value), plot_top, plot_bottom, y_zoom));
                    if (has_previous) {
                        draw->AddLine(previous, point, edge, 1.4f);
                    }
                    previous = point;
                    has_previous = true;
                }
            }
            ++data_track_order;
        }
        return;
    }

    for (std::size_t section_index = 0; section_index < state.fitted_sections.size();
         ++section_index) {
        const thrystr::app::Section& section = state.fitted_sections[section_index];
        if (section.length == 0u) {
            continue;
        }
        const std::size_t section_first = section.start_index;
        const std::size_t section_last =
            static_cast<std::size_t>(thrystr::app::section_end(section) - 1u);
        if (section_last < first || section_first > last) {
            continue;
        }

        const std::size_t visible_first = std::max(first, section_first);
        const std::size_t visible_last = std::min(last, section_last);
        const ImU32 base_color = kWaveColors[section_index % kWaveColors.size()];
        const ImU32 fill = palette::with_alpha(base_color, 34.0f / 255.0f);
        const ImU32 edge = section.max_residual > section.fit_tolerance
                               ? palette::status::destructive
                               : palette::with_alpha(base_color, 0.88f);
        const float x0 = plot_left + static_cast<float>(section_first) * x_step;
        const float x1 = plot_left + static_cast<float>(section_last) * x_step;
        draw->AddRectFilled(ImVec2(x0, plot_top), ImVec2(x1, plot_bottom), fill);
        draw->AddRect(ImVec2(x0, plot_top), ImVec2(x1, plot_bottom), edge, 0.0f, 0, 1.2f);

        const std::size_t samples = std::min<std::size_t>(1024u, visible_last - visible_first + 1u);
        ImVec2 previous{};
        bool has_previous = false;
        for (std::size_t sample = 0; sample < samples; ++sample) {
            const double t = samples > 1u
                                 ? static_cast<double>(sample) / static_cast<double>(samples - 1u)
                                 : 0.0;
            const double index = static_cast<double>(visible_first) +
                                 t * static_cast<double>(visible_last - visible_first);
            const double value = thrystr::app::wave_value_at_index(
                section, static_cast<std::size_t>(std::llround(index)));
            const ImVec2 point(
                plot_left + static_cast<float>(index) * x_step,
                y_for_value(static_cast<float>(value), plot_top, plot_bottom, y_zoom));
            if (has_previous) {
                draw->AddLine(previous, point, edge, 1.4f);
            }
            previous = point;
            has_previous = true;
        }
    }
}

float playback_readout_width(const AppState& state) {
    const std::size_t max_index =
        source_sample_count(state) == 0u ? 0u : source_sample_count(state) - 1u;
    const std::string longest_index = "x " + format_count(max_index);
    const ImVec2 index_size = ImGui::CalcTextSize(longest_index.c_str());
    const ImVec2 hex_size = ImGui::CalcTextSize("hex FF");
    const ImVec2 scalar_size = ImGui::CalcTextSize("func -1.000000");
    return std::max(228.0f, std::max({index_size.x, hex_size.x, scalar_size.x}) +
                                ImGui::GetStyle().FramePadding.x * 2.0f);
}

void draw_plot(AppState& state) {
    ++state.plot_frame;
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    const float toolbox_width =
        state.workspace_open ? std::min(kToolboxWidth, viewport.x * 0.45f) : 0.0f;
    ImGui::SetNextWindowPos(ImVec2(0.0f, kTopChromeHeight));
    ImGui::SetNextWindowSize(ImVec2(std::max(120.0f, viewport.x - toolbox_width),
                                    std::max(120.0f, viewport.y - kTopChromeHeight)));
    ImGui::Begin("Waveform", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (!state.analysis || source_sample_count(state) == 0u) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##empty_plot", available);
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        draw->AddRectFilled(min, max, palette::surface::window);
        draw->AddRect(min, max, palette::border::separator);
        draw->AddText(ImVec2(min.x + 24.0f, min.y + 24.0f), palette::ink::muted,
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
    if (thrystr::gui::icon_button(state.xline_playing ? thrystr::gui::icons::kPause
                                                      : thrystr::gui::icons::kPlay,
                                  state.xline_playing ? "Pause x-line" : "Play x-line")) {
        toggle_xline_playback(state);
    }
    ImGui::SameLine();
    const std::optional<std::uint64_t> playhead_byte =
        ticker_sample_at(state, state.playhead_index);
    const std::string playhead_hex =
        playhead_byte ? hex_sample_text(*playhead_byte, data_sample_bits(state)) : "--";
    const std::optional<thrystr::app::Scalar> playhead_scalar =
        scalar_at(state, state.playhead_index);
    const FitReadout fit_readout = fit_readout_at(state, state.playhead_index);
    const std::string track_text = fit_readout.available
                                       ? std::to_string(static_cast<unsigned>(fit_readout.track_id))
                                       : std::string("--");
    const std::string data_scalar_text =
        playhead_scalar ? format_scalar(*playhead_scalar) : std::string("--");
    const std::string wave_scalar_text =
        fit_readout.wave_scalar ? format_scalar(*fit_readout.wave_scalar) : std::string("--");
    const float readout_width = playback_readout_width(state);
    const float readout_x = ImGui::GetCursorPosX();
    ImGui::BeginGroup();
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "x %s",
                       format_count(state.playhead_index).c_str());
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "hex %s", playhead_hex.c_str());
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "track %s", track_text.c_str());
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "data %s", data_scalar_text.c_str());
    ImGui::TextColored(palette::to_vec4(palette::ink::muted), "func %s", wave_scalar_text.c_str());
    ImGui::EndGroup();
    ImGui::SameLine(readout_x + readout_width);
    if (playback_speed_button(state.wheel_scroll_mode ? "wheel scroll" : "wheel scale",
                              state.wheel_scroll_mode, 102.0f)) {
        state.wheel_scroll_mode = !state.wheel_scroll_mode;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.wheel_scroll_mode ? "Mouse wheel scrolls the x timeline"
                                                  : "Mouse wheel scales the x timeline");
    }
    ImGui::SameLine();
    if (playback_speed_button("segment", state.segment_selection_mode, 78.0f)) {
        state.segment_selection_mode = !state.segment_selection_mode;
        if (state.segment_selection_mode) {
            state.point_selection_mode = false;
        }
        state.selection_drag_handle = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.segment_selection_mode
                              ? "Timeline clicks edit the segment selection"
                              : "Timeline clicks move the playhead");
    }
    ImGui::SameLine();
    if (playback_speed_button("points", state.point_selection_mode, 70.0f)) {
        state.point_selection_mode = !state.point_selection_mode;
        if (state.point_selection_mode) {
            state.segment_selection_mode = false;
            state.selection_drag_handle = 0;
            state.property_tab = PropertyTab::Points;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(state.point_selection_mode
                              ? "Timeline clicks toggle individual fit points"
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
    const float margin_bottom = 126.0f;
    const float logical_width =
        std::max(available.x, margin_left + margin_right + static_cast<float>(count - 1) * x_step);

    ImGui::BeginChild("##plot_scroll", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 child_pos = ImGui::GetWindowPos();
    const ImVec2 child_size = ImGui::GetWindowSize();
    const bool child_mouse_inside =
        io.MousePos.x >= child_pos.x && io.MousePos.x <= child_pos.x + child_size.x &&
        io.MousePos.y >= child_pos.y && io.MousePos.y <= child_pos.y + child_size.y;
    const float child_width = ImGui::GetWindowWidth();
    const float max_scroll_x = std::max(0.0f, logical_width - child_width);
    const float native_scroll_x = ImGui::GetScrollX();
    const float scrollbar_height = ImGui::GetStyle().ScrollbarSize;
    const bool scrollbar_drag_in_progress =
        child_mouse_inside && io.MouseDown[ImGuiMouseButton_Left] &&
        io.MousePos.y >= child_pos.y + child_size.y - scrollbar_height;
    state.timeline_scroll_x = std::clamp(state.timeline_scroll_x, 0.0f, max_scroll_x);
    if (scrollbar_drag_in_progress && std::abs(native_scroll_x - state.timeline_scroll_x) > 0.5f) {
        state.timeline_scroll_x = std::clamp(native_scroll_x, 0.0f, max_scroll_x);
        state.last_manual_scroll_frame = state.plot_frame;
    }
    if (state.pending_scroll_index) {
        const float target_scroll = static_cast<float>(*state.pending_scroll_index) * x_step +
                                    margin_left - child_width * 0.5f;
        state.timeline_scroll_x = std::clamp(target_scroll, 0.0f, max_scroll_x);
        state.pending_scroll_index.reset();
    }
    const bool manual_scroll_recent =
        state.plot_frame >= state.last_manual_scroll_frame &&
        state.plot_frame - state.last_manual_scroll_frame <= kManualScrollAutoscrollInhibitFrames;
    if (state.xline_playing && !manual_scroll_recent) {
        const float playhead_local =
            margin_left + static_cast<float>(state.playhead_index) * x_step;
        const float visible_midpoint = state.timeline_scroll_x + child_width * 0.5f;
        if (playhead_local >= visible_midpoint) {
            state.timeline_scroll_x =
                std::clamp(playhead_local - child_width * 0.5f, 0.0f, max_scroll_x);
        }
    }
    if (child_mouse_inside && state.wheel_scroll_mode && io.MouseWheel != 0.0f && !io.KeyCtrl &&
        !io.KeySuper) {
        const float scroll_step = std::clamp(child_width * 0.18f, 64.0f, 420.0f);
        state.timeline_scroll_x =
            std::clamp(state.timeline_scroll_x - io.MouseWheel * scroll_step, 0.0f, max_scroll_x);
        state.last_manual_scroll_frame = state.plot_frame;
    }
    ImGui::InvisibleButton("##plot_canvas", ImVec2(logical_width, plot_height));
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
            const float scroll_step = std::clamp(ImGui::GetWindowWidth() * 0.18f, 64.0f, 420.0f);
            state.timeline_scroll_x = std::clamp(
                state.timeline_scroll_x - io.MouseWheel * scroll_step, 0.0f, max_scroll_x);
            state.last_manual_scroll_frame = state.plot_frame;
        } else if (!state.wheel_scroll_mode) {
            const float factor = std::pow(1.12f, io.MouseWheel);
            const float old_step = x_step;
            const float mouse_index = std::clamp((io.MousePos.x - plot_left) / old_step, 0.0f,
                                                 static_cast<float>(count - 1u));
            state.zoom_x = std::max(0.01f, state.zoom_x * factor);
            const float new_step = plot_x_step(state);
            const float mouse_local_x = io.MousePos.x - child_pos.x;
            const float new_logical_width = std::max(
                available.x, margin_left + margin_right + static_cast<float>(count - 1) * new_step);
            const float new_max_scroll_x = std::max(0.0f, new_logical_width - child_width);
            state.timeline_scroll_x = std::clamp(
                mouse_index * new_step + margin_left - mouse_local_x, 0.0f, new_max_scroll_x);
            state.last_manual_scroll_frame = state.plot_frame;
        }
    }
    ImGui::SetScrollX(state.timeline_scroll_x);
    const auto visible_range = thrystr::gui::visible_sample_range(
        {count, state.timeline_scroll_x, child_width, margin_left, x_step});
    if (!visible_range.valid) {
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    const std::size_t first = visible_range.first;
    const std::size_t last = visible_range.last;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const auto index_from_x = [&](float x) {
        const double index = std::round((x - plot_left) / x_step);
        return static_cast<std::size_t>(std::clamp(index, 0.0, static_cast<double>(count - 1u)));
    };
    const auto index_from_mouse = [&]() { return index_from_x(mouse.x); };
    const float playhead_hit_x = plot_left + static_cast<float>(state.playhead_index) * x_step;
    const bool playhead_hit = std::abs(mouse.x - playhead_hit_x) <= kPlayheadScrubHitPixels &&
                              mouse.y >= plot_top && mouse.y <= clip_max.y;
    bool playhead_scrub_claimed = state.playhead_dragging;
    if (plot_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && playhead_hit) {
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
    ensure_lazy_blocks(state, std::min(first, state.playhead_index),
                       std::max(last, state.playhead_index));

    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(clip_min, clip_max, true);
    thrystr::gui::reserve_timeline_geometry(
        draw, visible_range.count() * (state.entities.size() + 2u),
        visible_range.count() / thrystr::gui::pixel_stride(x_step, kTickerTargetLabelPixels) + 64u);
    draw->AddRectFilled(item_min, item_max, palette::surface::window);
    const AutoFitProgressSnapshot fit_progress = auto_fit_progress_snapshot(state);

    const float label_x = child_pos.x + 8.0f;
    const float grid_x0 = child_pos.x + margin_left;
    const float grid_x1 = child_pos.x + child_size.x - 18.0f;
    for (float y_value : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        const float y = y_for_value(y_value, plot_top, plot_bottom, y_zoom);
        draw->AddLine(ImVec2(grid_x0, y), ImVec2(grid_x1, y),
                      y_value == 0.0f ? palette::border::strong : palette::border::separator,
                      y_value == 0.0f ? 1.4f : 1.0f);
        char label[24] = {};
        std::snprintf(label, sizeof(label), "%.1f", static_cast<double>(y_value));
        draw->AddText(ImVec2(label_x, y - 7.0f), palette::ink::muted, label);
    }

    const std::size_t tick = nice_tick(160.0 / std::max(0.1f, x_step));
    const std::size_t tick_start = first - (first % tick);
    for (std::size_t i = tick_start; i <= last && i < count; i += tick) {
        const float x = plot_left + static_cast<float>(i) * x_step;
        draw->AddLine(ImVec2(x, plot_top), ImVec2(x, plot_bottom), palette::border::separator,
                      1.0f);
        char label[32] = {};
        std::snprintf(label, sizeof(label), "%.0f nm", static_cast<double>(i) * period_nm);
        draw->AddText(ImVec2(x + 4.0f, plot_bottom + 8.0f), palette::ink::muted, label);
    }

    const std::size_t delta_index = std::min(analysis.window.delta_index, count - 1);
    if (delta_index >= first && delta_index <= last) {
        const float x = plot_left + static_cast<float>(delta_index) * x_step;
        draw->AddRectFilled(ImVec2(x - 2.0f, plot_top),
                            ImVec2(x + std::max(2.0f, x_step + 2.0f), plot_bottom), kMaxDeltaFill);
        draw->AddText(ImVec2(x + 6.0f, plot_top + 6.0f), palette::status::destructive, "max delta");
    }

    if (state.segment.active) {
        clamp_segment(state);
        const auto [selection_first, selection_last] = normalized_selection(state);
        const float start_x = plot_left + static_cast<float>(selection_first) * x_step;
        const float end_x = plot_left + static_cast<float>(selection_last) * x_step;
        draw->AddRectFilled(ImVec2(start_x, plot_top), ImVec2(end_x, plot_bottom), kSelectionFill);
        draw->AddLine(ImVec2(start_x, plot_top), ImVec2(start_x, plot_bottom), kSelectionEdge,
                      2.0f);
        draw->AddLine(ImVec2(end_x, plot_top), ImVec2(end_x, plot_bottom), kSelectionEdge, 2.0f);
        draw->AddCircleFilled(ImVec2(start_x, plot_top + 8.0f), 4.0f, kSelectionHandle);
        draw->AddCircleFilled(ImVec2(end_x, plot_top + 8.0f), 4.0f, kSelectionHandle);
    }

    draw_auto_fit_progress_overlay(draw, fit_progress, first, last, plot_left, plot_top,
                                   plot_bottom, x_step, clip_min, clip_max);

    if (plot_hovered && !playhead_scrub_claimed && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const std::size_t index = index_from_mouse();
        if (state.point_selection_mode) {
            toggle_selected_data_point(state, index);
            set_playhead_index(state, index, false);
        } else if (!state.segment_selection_mode) {
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
    if (draw_data && state.show_lines && !state.show_reconstruction_only) {
        append_data_polyline(state, draw, first, last, plot_left, plot_top, plot_bottom, x_step,
                             y_zoom);
    }
    std::size_t wave_index = 0;
    for (const Entity& entity : state.entities) {
        if (entity.type != EntityType::Wave || !entity.visible || last <= first) {
            continue;
        }
        const ImU32 color = kWaveColors[wave_index % kWaveColors.size()];
        ++wave_index;
        append_wave_polyline(state, entity.wave, first, last, period_nm, plot_left, plot_top,
                             plot_bottom, x_step, y_zoom);
        thrystr::gui::stroke_polyline(draw, state.plot_points, color, 1.5f);
    }

    draw_fitted_sections(draw, state, first, last, plot_left, plot_top, plot_bottom, x_step,
                         y_zoom);

    if (draw_data && state.show_points && !state.show_reconstruction_only) {
        draw_data_point_markers(draw, state, first, last, plot_left, plot_top, plot_bottom, x_step,
                                y_zoom, clip_min, clip_max);
    }
    draw_selected_data_point_markers(draw, state, first, last, plot_left, plot_top, plot_bottom,
                                     x_step, y_zoom);

    draw->AddRect(ImVec2(plot_left, plot_top), ImVec2(plot_right, plot_bottom),
                  palette::border::default_, 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(child_pos.x + margin_left, child_pos.y + 8.0f), palette::ink::primary,
                  "x-line section");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 178.0f, child_pos.y + 8.0f), kDataLineColor,
                  "data");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 128.0f, child_pos.y + 8.0f), kWaveColors[0],
                  "functions");
    if (state.show_reconstruction_only) {
        draw->AddRectFilled(ImVec2(child_pos.x + margin_left, child_pos.y + 28.0f),
                            ImVec2(child_pos.x + margin_left + 118.0f, child_pos.y + 52.0f),
                            palette::with_alpha(palette::status::warning, 48.0f / 255.0f),
                            palette::radii::pill);
        draw->AddText(ImVec2(child_pos.x + margin_left + 12.0f, child_pos.y + 33.0f),
                      palette::status::warning, "reconstruction");
    }

    if (state.playhead_index >= first && state.playhead_index <= last) {
        const float playhead_x = plot_left + static_cast<float>(state.playhead_index) * x_step;
        draw->AddLine(ImVec2(playhead_x, plot_top), ImVec2(playhead_x, clip_max.y - 8.0f),
                      kSelectionHandle, 2.0f);
        if (const std::optional<thrystr::app::Scalar> scalar =
                scalar_at(state, state.playhead_index)) {
            draw->AddCircleFilled(ImVec2(playhead_x, y_for_value(static_cast<float>(*scalar),
                                                                 plot_top, plot_bottom, y_zoom)),
                                  4.0f, kSelectionHandle);
        }
    }
    draw_parity_strip(draw, state, first, last, plot_left, plot_bottom, x_step, clip_min, clip_max);
    draw_data_ticker(state, draw, first, last, plot_left, plot_bottom, x_step, clip_min, clip_max);

    draw->PopClipRect();
    ImGui::EndChild();
    ImGui::End();
}

void navigate_file_dialog_selection(AppState& state) {
    const int row = state.file_dialog.row_sel;
    if (row == state.file_dialog_last_row || row < 0 ||
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
        return "Save Function Data###thrystr_file_dialog";
    case DialogPurpose::None:
        break;
    }
    return kFileDialogStableId;
}

thrystr::gui::FileDialogResult
begin_chromed_file_dialog(DialogPurpose purpose, thrystr::gui::FileDialogMode mode,
                          thrystr::gui::FileDialogState& dialog,
                          std::span<const thrystr::gui::FileDialogEntry> entries) {
    return thrystr::gui::begin_file_dialog(file_dialog_popup_label(purpose), mode, dialog, entries);
}

void draw_file_dialog(AppState& state) {
    if (state.pending_dialog != DialogPurpose::None) {
        state.active_dialog = state.pending_dialog;
        state.pending_dialog = DialogPurpose::None;
        thrystr::gui::open_file_dialog(file_dialog_popup_label(state.active_dialog));
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
    const thrystr::gui::FileDialogMode mode = state.active_dialog == DialogPurpose::SaveWave
                                                  ? thrystr::gui::FileDialogMode::Save
                                                  : thrystr::gui::FileDialogMode::Open;
    const thrystr::gui::FileDialogResult result = begin_chromed_file_dialog(
        state.active_dialog, mode, state.file_dialog,
        std::span<const thrystr::gui::FileDialogEntry>(state.file_dialog_entries.data(),
                                                       state.file_dialog_entries.size()));

    if (cwd_before != state.file_dialog.cwd || filter_before != state.file_dialog.filter) {
        state.file_dialog_dirty = true;
    }
    navigate_file_dialog_selection(state);

    if (result == thrystr::gui::FileDialogResult::Cancelled) {
        state.active_dialog = DialogPurpose::None;
        return;
    }
    if (result != thrystr::gui::FileDialogResult::Confirmed) {
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
            state.fitted_sections.clear();
            state.fitted_workspace = {};
            state.fitted_workspace_valid = false;
            state.fitted_workspace_used_parity = false;
            state.fit_validation = {};
            state.show_reconstruction_only = false;
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

std::string lowercase_ascii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

bool text_matches_query(std::string_view title, std::string_view body, std::string_view query) {
    const std::string needle = lowercase_ascii(query);
    if (needle.empty()) {
        return true;
    }
    const std::string haystack = lowercase_ascii(std::string(title) + "\n" + std::string(body));
    return haystack.find(needle) != std::string::npos;
}

void draw_markdown_body(std::string_view markdown, const thrystr::gui::FontSet& fonts) {
    std::string line;
    const auto flush_line = [&](std::string_view text) {
        if (text.rfind("### ", 0) == 0 || text.rfind("## ", 0) == 0 || text.rfind("# ", 0) == 0) {
            const std::size_t offset = text.rfind("### ", 0) == 0  ? 4u
                                       : text.rfind("## ", 0) == 0 ? 3u
                                                                   : 2u;
            if (fonts.sans_md) {
                ImGui::PushFont(fonts.sans_md);
            }
            ImGui::TextWrapped("%.*s", static_cast<int>(text.size() - offset),
                               text.data() + offset);
            if (fonts.sans_md) {
                ImGui::PopFont();
            }
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            return;
        }
        if (text.rfind("- ", 0) == 0) {
            ImGui::BulletText("%.*s", static_cast<int>(text.size() - 2u), text.data() + 2u);
            return;
        }
        if (!text.empty()) {
            ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
        } else {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
        }
    };

    for (char ch : markdown) {
        if (ch == '\n') {
            flush_line(line);
            line.clear();
        } else if (ch != '\r') {
            line.push_back(ch);
        }
    }
    if (!line.empty()) {
        flush_line(line);
    }
}

void draw_docs_panel(AppState& state) {
    if (!state.show_docs) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(820.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Docs", &state.show_docs, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

#if defined(THRYSTR_HAS_DOCS)
    const auto* pages = thrystr::docs::doc_pages();
    const std::size_t page_count = thrystr::docs::doc_page_count();
    if (page_count == 0u || pages == nullptr) {
        thrystr::gui::status_row("manual", "not generated", thrystr::gui::StatusTone::Warning);
        ImGui::End();
        return;
    }

    state.docs_page = std::clamp(state.docs_page, 0, static_cast<int>(page_count - 1u));
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##docs_search", state.docs_search, sizeof(state.docs_search));

    const float nav_width = 220.0f;
    ImGui::BeginChild("##docs_nav", ImVec2(nav_width, 0.0f), true);
    for (std::size_t i = 0; i < page_count; ++i) {
        if (!text_matches_query(pages[i].title, pages[i].markdown, state.docs_search)) {
            continue;
        }
        if (ImGui::Selectable(pages[i].title.data(), state.docs_page == static_cast<int>(i))) {
            state.docs_page = static_cast<int>(i);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##docs_body", ImVec2(0.0f, 0.0f), true);
    draw_markdown_body(pages[static_cast<std::size_t>(state.docs_page)].markdown, state.fonts);
    ImGui::EndChild();
#else
    thrystr::gui::status_row("manual", "not built", thrystr::gui::StatusTone::Warning);
#endif

    ImGui::End();
}

void handle_shortcuts(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        state.show_docs = !state.show_docs;
    }
    if (io.WantTextInput || state.active_dialog != DialogPurpose::None ||
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
    if (shortcut &&
        (ImGui::IsKeyPressed(ImGuiKey_W, false) || ImGui::IsKeyPressed(ImGuiKey_A, false))) {
        create_wave_entity(state);
    }
    if (shortcut && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        open_empty_workspace(state);
    }
}

void draw_app(AppState& state, thrystr::gui::WindowHandle window,
              const thrystr::gui::ResizeCursorSet& cursors) {
    finish_auto_fit_if_ready(state);
    handle_shortcuts(state);
    draw_titlebar(state, window);
    draw_plot(state);
    draw_entity_toolbox(state);
    draw_inspector_overlay(state);
    draw_settings_dialog(state);
    draw_fit_validation_overlay(state);
    draw_auto_fit_activity_dialog(state);
    draw_docs_panel(state);
    draw_file_dialog(state);
    handle_custom_chrome(state, window, cursors);
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    thrystr::gui::NativeApplication native_application;
    if (!native_application.ready()) {
        return 1;
    }

    thrystr::gui::ResizeCursorSet chrome_cursors;
    AppState state;
    initialize_file_dialog(state);

    thrystr::gui::InterfaceSession interface_session;

    StartupAction startup_action = StartupAction::None;
    bool screenshot_saved = false;
    if (args.file.empty()) {
        thrystr::gui::WindowHandle splash_window =
            thrystr::gui::create_window({"thrystr start",
                                         {kSplashWindowWidth, kSplashWindowHeight},
                                         {kSplashMinWindowWidth, kSplashMinWindowHeight},
                                         false,
                                         false});
        if (!splash_window) {
            return 1;
        }

        interface_session.start(splash_window, THRYSTR_FONT_DIR, state.fonts);
        const thrystr::gui::Texture hero_texture = thrystr::gui::load_texture(kSplashHeroPath);
        state.splash_hero_texture = hero_texture.id;
        state.splash_hero_size =
            ImVec2(static_cast<float>(hero_texture.width), static_cast<float>(hero_texture.height));
        thrystr::gui::show_window(splash_window);

        int rendered_frames = 0;
        while (!thrystr::gui::should_close(splash_window)) {
            thrystr::gui::poll_events();
            interface_session.begin_frame();

            StartupAction action = draw_splash_window(state, splash_window, chrome_cursors);
            ImGuiIO& io = ImGui::GetIO();
            const bool shortcut = io.KeyCtrl || io.KeySuper;
            if (shortcut && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
                action = StartupAction::NewWorkspace;
            }
            if (state.request_close) {
                thrystr::gui::request_close(splash_window);
            }

            const thrystr::gui::FrameSize frame = interface_session.render_frame(splash_window);
            ++rendered_frames;
            if (!args.screenshot.empty() && rendered_frames >= args.frames) {
                if (!thrystr::gui::save_framebuffer_png(args.screenshot, frame.width,
                                                        frame.height)) {
                    std::fprintf(stderr, "could not write screenshot: %s\n",
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

        thrystr::gui::destroy_texture(state.splash_hero_texture);
        state.splash_hero_size = {};
        interface_session.shutdown(state.fonts);
        thrystr::gui::destroy_window(splash_window);
        state.request_close = false;
        state.chrome_action = ChromeAction::None;
        state.splash_open = false;

        if (screenshot_saved) {
            return 0;
        }
        if (startup_action == StartupAction::None) {
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

    std::optional<thrystr::gui::WindowBounds> workspace_geometry;
    int workspace_width = args.width;
    int workspace_height = args.height;
    if (!args.size_overridden) {
        workspace_geometry =
            thrystr::gui::preferred_workspace_bounds({kMinWindowWidth, kMinWindowHeight});
        if (workspace_geometry) {
            workspace_width = workspace_geometry->size.width;
            workspace_height = workspace_geometry->size.height;
        }
    }

    thrystr::gui::WindowHandle window =
        thrystr::gui::create_window({"thrystr",
                                     {workspace_width, workspace_height},
                                     {kMinWindowWidth, kMinWindowHeight},
                                     true,
                                     false});
    if (!window) {
        return 1;
    }
    if (workspace_geometry) {
        thrystr::gui::move_resize_window(window, *workspace_geometry);
    }

    interface_session.start(window, THRYSTR_FONT_DIR, state.fonts);
    if (!args.file.empty()) {
        std::snprintf(state.path, sizeof(state.path), "%s", args.file.c_str());
        load_path(state);
    }
    thrystr::gui::show_window(window);

    int rendered_frames = 0;
    while (!thrystr::gui::should_close(window)) {
        thrystr::gui::poll_events();
        interface_session.begin_frame();

        draw_app(state, window, chrome_cursors);
        if (state.request_close) {
            thrystr::gui::request_close(window);
        }

        const thrystr::gui::FrameSize frame = interface_session.render_frame(window);

        ++rendered_frames;
        if (!args.screenshot.empty() && rendered_frames >= args.frames) {
            if (!thrystr::gui::save_framebuffer_png(args.screenshot, frame.width, frame.height)) {
                std::fprintf(stderr, "could not write screenshot: %s\n", args.screenshot.c_str());
            }
            break;
        }
    }

    cancel_auto_fit(state);
    if (state.auto_fit_job.worker.joinable()) {
        state.auto_fit_job.worker.join();
    }
    interface_session.shutdown(state.fonts);
    thrystr::gui::destroy_window(window);
    return 0;
}
