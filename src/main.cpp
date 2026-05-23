#include <thrystr/scalar_analysis.hpp>

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
#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr float kToolboxWidth = 380.0f;
constexpr float kTopChromeHeight = 44.0f;
constexpr double kChromeResizeMargin = 8.0;
constexpr int kMinWindowWidth = 760;
constexpr int kMinWindowHeight = 460;
constexpr std::uintmax_t kLargeSourcePhaseFitDeferBytes = 128ull * 1024ull * 1024ull;
constexpr const char* kOpenFileDialogId = "##open_file_dialog";
constexpr const char* kWaveSettingsExtension = ".thryw";
constexpr std::array<char, 8> kWaveSettingsMagic = {'T', 'H', 'R', 'Y', 'W', 'A', 'V', 'E'};
constexpr std::uint32_t kWaveSettingsVersion = 2;

enum class DialogPurpose {
    None,
    OpenSource,
    LoadWave,
    SaveWave,
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

struct Args {
    std::string file;
    std::string screenshot;
    int width = 1600;
    int height = 900;
    int frames = 0;
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
    bool show_sine = true;
    bool show_cosine = true;
    float zoom_x = 1.0f;
    float zoom_y = 1.0f;
    float max_slope = thrystr::kDefaultMaxSlope;
    float wave_scale = static_cast<float>(thrystr::kDefaultWaveScale);
    float wave_tolerance = static_cast<float>(thrystr::kDefaultWaveTolerance);
    int phase_steps = 720;
    int phase_test_points = 65536;
    skald::FileDialogState file_dialog{};
    std::vector<FileBrowserEntry> file_dialog_rows;
    std::vector<skald::FileDialogEntry> file_dialog_entries;
    int file_dialog_last_row = -1;
    DialogPurpose pending_dialog = DialogPurpose::None;
    DialogPurpose active_dialog = DialogPurpose::None;
    skald::Fonts fonts{};
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
            }
        };

        next_string(args.file, "--file");
        next_string(args.screenshot, "--screenshot");
        next_int(args.width, "--width");
        next_int(args.height, "--height");
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
        x = state.chrome_start_window_x + dx;
        w = state.chrome_start_window_w - dx;
        if (w < kMinWindowWidth) {
            x = state.chrome_start_window_x + state.chrome_start_window_w - kMinWindowWidth;
            w = kMinWindowWidth;
        }
    } else if (chrome_action_has_right(state.chrome_action)) {
        w = std::max(kMinWindowWidth, state.chrome_start_window_w + dx);
    }

    if (chrome_action_has_top(state.chrome_action)) {
        y = state.chrome_start_window_y + dy;
        h = state.chrome_start_window_h - dy;
        if (h < kMinWindowHeight) {
            y = state.chrome_start_window_y + state.chrome_start_window_h - kMinWindowHeight;
            h = kMinWindowHeight;
        }
    } else if (chrome_action_has_bottom(state.chrome_action)) {
        h = std::max(kMinWindowHeight, state.chrome_start_window_h + dy);
    }

    glfwSetWindowPos(window, x, y);
    glfwSetWindowSize(window, w, h);
}

void handle_custom_chrome(AppState& state,
                          GLFWwindow* window,
                          const ChromeCursors& cursors) {
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

    const ChromeAction resize_action =
        resize_action_at(cursor_x, cursor_y, window_w, window_h);
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

bool save_screenshot(const std::string& path, int width, int height) {
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
    return stbi_write_png(path.c_str(), width, height, 4, flipped.data(), width * 4) != 0;
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
    return state.analysis ? state.analysis->scalars.size() : 0u;
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
    if (!state.analysis || state.analysis->scalars.empty()) {
        state.status = "Load source data before creating a section";
        return;
    }
    state.segment.active = true;
    state.segment.selection_start = 0;
    state.segment.selection_end = state.analysis->scalars.size() - 1u;
    state.selection_drag_handle = 0;
    state.status = "Created x-line section";
}

double wave_value_at_nm(const WaveEntity& wave, double x_nm) {
    double wavelength = std::max(0.000001, wave.wavelength_nm);
    for (const auto& modifier : wave.wavelength_modifiers) {
        if (x_nm >= modifier.first) {
            wavelength = std::max(0.000001, wavelength + modifier.second);
        }
    }
    const double theta = 2.0 * kPi * (x_nm - wave.phase_nm) / wavelength;
    return wave.amplitude_offset + wave.amplitude * ((std::sin(theta) + 1.0) * 0.5);
}

std::pair<double, double> fit_wave_to_selection(const AppState& state) {
    if (!state.analysis || state.analysis->scalars.size() < 2) {
        return {64.0, 0.0};
    }

    const std::size_t first = state.segment.active
        ? std::min(state.segment.selection_start, state.segment.selection_end)
        : 0u;
    const std::size_t last = state.segment.active
        ? std::max(state.segment.selection_start, state.segment.selection_end)
        : state.analysis->scalars.size() - 1u;
    const double period = data_spatial_period_nm(state);
    const double span_nm = std::max(period, static_cast<double>(last - first + 1u) * period);
    const double min_wavelength = std::max(period * 2.0, span_nm / 64.0);
    const double max_wavelength = std::max(min_wavelength, span_nm * 2.0);
    const int wavelength_steps = 96;
    const int phase_steps = 96;

    double best_score = -1.0;
    double best_wavelength = min_wavelength;
    double best_phase = 0.0;

    for (int wi = 0; wi < wavelength_steps; ++wi) {
        const double t = wavelength_steps > 1
            ? static_cast<double>(wi) / static_cast<double>(wavelength_steps - 1)
            : 0.0;
        const double wavelength = min_wavelength + (max_wavelength - min_wavelength) * t;
        for (int pi = 0; pi < phase_steps; ++pi) {
            const double phase_nm = wavelength * static_cast<double>(pi) /
                                    static_cast<double>(phase_steps);
            double error = 0.0;
            std::size_t samples = 0;
            const std::size_t stride = std::max<std::size_t>(1, (last - first + 1u) / 2048u);
            WaveEntity candidate;
            candidate.wavelength_nm = wavelength;
            candidate.phase_nm = phase_nm;
            for (std::size_t index = first; index <= last && index < state.analysis->scalars.size();
                 index += stride) {
                const double x_nm = static_cast<double>(index) * period;
                const double wave_value = wave_value_at_nm(candidate, x_nm);
                error += std::abs(wave_value - state.analysis->scalars[index]);
                ++samples;
            }
            const double score = samples == 0 ? 0.0 : 1.0 / (1.0 + error / samples);
            if (score > best_score) {
                best_score = score;
                best_wavelength = wavelength;
                best_phase = phase_nm;
            }
        }
    }

    return {best_wavelength, best_phase};
}

void rebuild_wavelength_modifiers(const AppState& state, WaveEntity& wave) {
    wave.wavelength_modifiers.clear();
    if (!state.analysis || state.analysis->scalars.size() < 2) {
        return;
    }

    const std::size_t first = state.segment.active
        ? std::min(state.segment.selection_start, state.segment.selection_end)
        : 0u;
    const std::size_t last = state.segment.active
        ? std::max(state.segment.selection_start, state.segment.selection_end)
        : state.analysis->scalars.size() - 1u;
    const double period = data_spatial_period_nm(state);
    const double tolerance = std::max(0.01, static_cast<double>(state.wave_tolerance));
    const std::size_t stride = std::max<std::size_t>(1, (last - first + 1u) / 192u);
    const double max_delta = std::max(period, std::abs(wave.wavelength_nm) * 0.25);

    for (std::size_t index = first; index <= last && index < state.analysis->scalars.size();
         index += stride) {
        const double x_nm = static_cast<double>(index) * period;
        const double residual = state.analysis->scalars[index] - wave_value_at_nm(wave, x_nm);
        if (std::abs(residual) <= tolerance) {
            continue;
        }
        const double delta_nm = std::clamp(residual * period, -max_delta, max_delta);
        wave.wavelength_modifiers.push_back({x_nm, delta_nm});
        if (wave.wavelength_modifiers.size() >= 256u) {
            break;
        }
    }
}

void create_interpolated_wave(AppState& state) {
    Entity& wave = create_wave_entity(state, "interpolated " + std::to_string(state.wave_serial++));
    const auto [wavelength_nm, phase_nm] = fit_wave_to_selection(state);
    wave.wave.wavelength_nm = wavelength_nm;
    wave.wave.phase_nm = phase_nm;
    rebuild_wavelength_modifiers(state, wave.wave);
    state.status = "Created interpolated wave";
}

void load_path(AppState& state);

bool should_defer_phase_fit_on_load(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    return !error && file_size >= kLargeSourcePhaseFitDeferBytes;
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
    const std::uint8_t show_sine = state.show_sine ? 1 : 0;
    const std::uint8_t show_cosine = state.show_cosine ? 1 : 0;
    write_binary(output, show_points);
    write_binary(output, show_lines);
    write_binary(output, show_sine);
    write_binary(output, show_cosine);

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
    state.show_sine = read_binary<std::uint8_t>(input) != 0;
    state.show_cosine = read_binary<std::uint8_t>(input) != 0;
    const double sine_phase = read_binary<double>(input);
    const double cosine_phase = read_binary<double>(input);

    const std::uint32_t mapper_count = read_binary<std::uint32_t>(input);
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
        state.status = "Analyzing";
        const std::filesystem::path source_path(state.path);
        const bool phase_fit_deferred = should_defer_phase_fit_on_load(source_path);
        state.analysis = thrystr::analyze_file(
            source_path,
            thrystr::kDefaultWindowBytes,
            state.max_slope,
            static_cast<double>(state.wave_scale),
            static_cast<double>(state.wave_tolerance),
            state.phase_steps,
            phase_fit_deferred ? 0u : static_cast<std::size_t>(state.phase_test_points),
            state.mappers);

        const auto& analysis = *state.analysis;
        Entity& data = ensure_data_entity(state);
        data.name = analysis.source_path.filename().string();
        if (state.selected_entity_id == 0 || !find_entity(state, state.selected_entity_id)) {
            select_entity(state, data.id);
        }
        if (!state.segment.active && !analysis.scalars.empty()) {
            state.segment.selection_start = 0;
            state.segment.selection_end = analysis.scalars.size() - 1u;
        }
        state.status = "Loaded " + analysis.source_path.filename().string() +
                       " / sample " + format_bytes(analysis.window.length);
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
        copy_to_buffer(state.file_dialog.cwd, parent.string());
        copy_to_buffer(state.file_dialog.filename, selected.filename().string());
    } else {
        copy_to_buffer(state.file_dialog.cwd, home_or_current_path().string());
        if (purpose == DialogPurpose::SaveWave) {
            copy_to_buffer(state.file_dialog.filename, std::string("wave") + kWaveSettingsExtension);
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
    refresh_file_dialog_entries(state);
}

void request_file_dialog(AppState& state, DialogPurpose purpose) {
    configure_file_dialog(state, purpose);
    state.pending_dialog = purpose;
}

void draw_titlebar(AppState& state, GLFWwindow* window) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport.x, kTopChromeHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##titlebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(2);

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(ImVec2(0.0f, 0.0f),
                        ImVec2(viewport.x, kTopChromeHeight),
                        skald::tokens::surface::deep);
    draw->AddLine(ImVec2(0.0f, kTopChromeHeight - 1.0f),
                  ImVec2(viewport.x, kTopChromeHeight - 1.0f),
                  skald::tokens::border::separator,
                  1.0f);

    ImGui::SetCursorPos(ImVec2(14.0f, 10.0f));
    if (state.fonts.sans_md) {
        ImGui::PushFont(state.fonts.sans_md);
    }
    ImGui::TextUnformatted("thrystr");
    if (state.fonts.sans_md) {
        ImGui::PopFont();
    }

    ImGui::SameLine(96.0f);
    if (skald::GhostButton("File", ImVec2(52.0f, 0.0f))) {
        ImGui::OpenPopup("##file_menu");
    }
    if (ImGui::BeginPopup("##file_menu")) {
        if (ImGui::MenuItem("New Workspace")) {
            open_empty_workspace(state);
        }
        if (ImGui::MenuItem("Load Source")) {
            request_file_dialog(state, DialogPurpose::OpenSource);
        }
        if (ImGui::MenuItem("Load Wave Data")) {
            request_file_dialog(state, DialogPurpose::LoadWave);
        }
        if (ImGui::MenuItem("Save Wave Data")) {
            request_file_dialog(state, DialogPurpose::SaveWave);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            state.request_close = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (skald::GhostButton("Create", ImVec2(70.0f, 0.0f))) {
        ImGui::OpenPopup("##create_menu");
    }
    if (ImGui::BeginPopup("##create_menu")) {
        if (ImGui::MenuItem("Wave", "Ctrl/Cmd+A")) {
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
    if (skald::AccentButton("Load Source", ImVec2(116.0f, 0.0f))) {
        request_file_dialog(state, DialogPurpose::OpenSource);
    }
    ImGui::SameLine();
    if (skald::GhostButton("Load Wave", ImVec2(104.0f, 0.0f))) {
        request_file_dialog(state, DialogPurpose::LoadWave);
    }
    ImGui::SameLine();
    if (skald::GhostButton("Save", ImVec2(62.0f, 0.0f))) {
        request_file_dialog(state, DialogPurpose::SaveWave);
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

    const float controls_width = 96.0f;
    ImGui::SetCursorPos(ImVec2(viewport.x - controls_width, 9.0f));
    if (skald::IconButton("-", "Minimize", 26.0f)) {
        glfwIconifyWindow(window);
    }
    ImGui::SameLine();
    if (skald::IconButton("[]", "Maximize", 26.0f)) {
        if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
            glfwRestoreWindow(window);
        } else {
            glfwMaximizeWindow(window);
        }
    }
    ImGui::SameLine();
    if (skald::IconButton("x", "Close", 26.0f)) {
        state.request_close = true;
    }

    ImGui::End();
}

bool value_bar_double(const char* label,
                      double* value,
                      double step_per_pixel,
                      const char* format = "%.4f") {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    const float width = std::max(80.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, 28.0f);
    const bool pressed = ImGui::InvisibleButton("##value_bar", size);
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
    const ImVec2 text_size = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(pos.x + size.x - text_size.x - 10.0f,
                         pos.y + (size.y - text_size.y) * 0.5f),
                  skald::tokens::ink::primary, text);
    if (hovered) {
        skald::Tooltip(pressed ? "Drag right or left" : "Drag right or left");
    }
    ImGui::PopID();
    return changed;
}

bool value_bar_int(const char* label, int* value, double step_per_pixel) {
    double editable = static_cast<double>(*value);
    const bool changed = value_bar_double(label, &editable, step_per_pixel, "%.0f");
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
            if (ImGui::SmallButton("^") && i > 0) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i - 1);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("v") && i + 1 < state.mappers.size()) {
                move_from = static_cast<int>(i);
                move_to = static_cast<int>(i + 1);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) {
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
    if (!ImGui::Begin("Inspector Overlay", &state.show_inspector_overlay,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    skald::SectionHeader("Analysis");
    if (state.analysis) {
        const thrystr::Analysis& a = *state.analysis;
        skald::KvRow("source", "%s", format_bytes(a.source_size).c_str());
        skald::KvRow("windows", "%s", format_count(a.window_count).c_str());
        skald::KvRow("offset", "%s", format_count(a.window.offset).c_str());
        skald::KvRow("length", "%s", format_bytes(a.window.length).c_str());
        skald::KvRow("max delta", "%u", static_cast<unsigned>(a.window.max_delta));
        skald::KvRow("x scale", "%.3f", static_cast<double>(a.x_scale));
        skald::KvRow("sample points", "%s", format_count(a.scalars.size()).c_str());
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

    ImGui::End();
}

void draw_settings_dialog(AppState& state) {
    if (state.show_settings) {
        ImGui::OpenPopup("Settings");
    }

    bool open = state.show_settings;
    if (ImGui::BeginPopupModal("Settings", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
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
        if (value_bar_double("x zoom", &zoom_x, 0.01, "%.2f")) {
            state.zoom_x = static_cast<float>(zoom_x);
        }
        if (value_bar_double("y zoom", &zoom_y, 0.01, "%.2f")) {
            state.zoom_y = static_cast<float>(zoom_y);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        if (skald::AccentButton("Done", ImVec2(96.0f, 0.0f))) {
            state.show_settings = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (!open) {
        state.show_settings = false;
    }
}

void draw_splash(AppState& state) {
    if (!state.splash_open) {
        return;
    }

    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, kTopChromeHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport.x, std::max(120.0f, viewport.y - kTopChromeHeight)),
                             ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("##splash_host", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
    draw->AddRectFilled(min, max, skald::tokens::surface::window);

    const float content_width = ImGui::GetContentRegionAvail().x;
    const float actions_width = std::min(360.0f, std::max(280.0f, content_width * 0.34f));
    const float right_width = std::max(240.0f, content_width - actions_width - 28.0f);

    ImGui::BeginGroup();
    if (state.fonts.sans_md) {
        ImGui::PushFont(state.fonts.sans_md);
    }
    ImGui::TextUnformatted("thrystr");
    if (state.fonts.sans_md) {
        ImGui::PopFont();
    }
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted),
                       "Scalar wave workspace");
    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    skald::SectionHeader("Workspace");
    if (skald::AccentButton("New workspace", ImVec2(actions_width, 0.0f))) {
        open_empty_workspace(state);
        ImGui::EndGroup();
        ImGui::End();
        return;
    }
    if (skald::GhostButton("Open workspace", ImVec2(actions_width, 0.0f))) {
        request_file_dialog(state, DialogPurpose::LoadWave);
        ImGui::EndGroup();
        ImGui::End();
        return;
    }
    if (skald::GhostButton("Load source", ImVec2(actions_width, 0.0f))) {
        request_file_dialog(state, DialogPurpose::OpenSource);
        ImGui::EndGroup();
        ImGui::End();
        return;
    }
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 28.0f);
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, skald::tokens::to_vec4(skald::tokens::surface::panel));
    ImGui::BeginChild("##splash_recent", ImVec2(right_width, 270.0f),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    skald::SectionHeader("Recent");
    skald::KvRowStatus("workspaces", "none", skald::BadgeTone::Muted);
    skald::KvRowStatus("source", "none loaded", skald::BadgeTone::Muted);
    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    skald::SectionHeader("Entities");
    skald::KvRowStatus("data", "empty", skald::BadgeTone::Muted);
    skald::KvRowStatus("waves", "0", skald::BadgeTone::Muted);
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::End();
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
    ImGui::Begin("Toolbox", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

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
            skald::KvRow("points", "%s", format_count(a.scalars.size()).c_str());
            skald::KvRow("sample", "%s", format_bytes(a.window.length).c_str());
        } else {
            skald::KvRowStatus("source", "none", skald::BadgeTone::Muted);
        }
        if (data &&
            value_bar_double("point spacing nm", &data->data.spatial_period_nm,
                             0.01, "%.6f")) {
            data->data.spatial_period_nm =
                std::max(0.000001, data->data.spatial_period_nm);
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("X-Line Section");
        if (state.analysis && !state.analysis->scalars.empty()) {
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
                changed |= value_bar_double("start index", &start_index, 0.25, "%.0f");
                changed |= value_bar_double("end index", &end_index, 0.25, "%.0f");
                if (changed) {
                    const std::size_t count = state.analysis->scalars.size();
                    state.segment.selection_start = clamp_index(start_index, count);
                    state.segment.selection_end = clamp_index(end_index, count);
                    clamp_segment(state);
                }
                double start_nm =
                    static_cast<double>(state.segment.selection_start) * period;
                double end_nm =
                    static_cast<double>(state.segment.selection_end) * period;
                bool nm_changed = false;
                nm_changed |= value_bar_double("start nm", &start_nm, period * 0.25, "%.3f");
                nm_changed |= value_bar_double("end nm", &end_nm, period * 0.25, "%.3f");
                if (nm_changed) {
                    const std::size_t count = state.analysis->scalars.size();
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
        double max_slope = static_cast<double>(state.max_slope);
        double wave_tolerance = static_cast<double>(state.wave_tolerance);
        params_changed |= value_bar_double("max slope", &max_slope, 0.001, "%.4f");
        params_changed |= value_bar_double("wave tolerance", &wave_tolerance, 0.0001, "%.5f");
        params_changed |= value_bar_int("phase steps", &state.phase_steps, 4.0);
        params_changed |= value_bar_int("phase samples", &state.phase_test_points, 512.0);
        state.max_slope = static_cast<float>(max_slope);
        state.wave_tolerance = static_cast<float>(wave_tolerance);
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
        value_bar_double("wave spatial distance nm", &selected->wave.wavelength_nm, 0.10, "%.4f");
        value_bar_double("amplitude", &selected->wave.amplitude, 0.01, "%.4f");
        value_bar_double("amplitude offset", &selected->wave.amplitude_offset, 0.01, "%.4f");
        value_bar_double("phase nm", &selected->wave.phase_nm, 0.10, "%.4f");
        skald::KvRow("modifiers", "%s",
                     format_count(selected->wave.wavelength_modifiers.size()).c_str());
        if (skald::GhostButton("Fit Selection", ImVec2(124.0f, 0.0f))) {
            const auto [wavelength_nm, phase_nm] = fit_wave_to_selection(state);
            selected->wave.wavelength_nm = wavelength_nm;
            selected->wave.phase_nm = phase_nm;
            rebuild_wavelength_modifiers(state, selected->wave);
            state.status = "Fitted " + selected->name + " to selection";
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
    const float slope_step = std::max(0.05f, state.analysis->x_scale);
    const float point_spacing =
        static_cast<float>(std::clamp(data_spatial_period_nm(state), 0.000001, 1.0e6));
    const float zoom = std::max(0.01f, std::abs(state.zoom_x));
    return std::max(0.05f, std::max(slope_step, point_spacing) * zoom);
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

    if (!state.analysis || state.analysis->scalars.empty()) {
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

    const thrystr::Analysis& analysis = *state.analysis;
    const std::size_t count = analysis.scalars.size();
    state.segment.selection_start = std::min(state.segment.selection_start, count - 1u);
    state.segment.selection_end = std::min(state.segment.selection_end, count - 1u);
    const float x_step = plot_x_step(state);
    const float y_zoom = std::max(0.01f, std::abs(state.zoom_y));
    const double period_nm = data_spatial_period_nm(state);
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float plot_height = std::max(360.0f, available.y - 8.0f);
    const float margin_left = 58.0f;
    const float margin_right = 30.0f;
    const float margin_top = 28.0f;
    const float margin_bottom = 44.0f;
    const float logical_width = std::max(available.x,
        margin_left + margin_right + static_cast<float>(count - 1) * x_step);

    ImGui::BeginChild("##plot_scroll", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::InvisibleButton("##plot_canvas", ImVec2(logical_width, plot_height));
    const bool plot_hovered = ImGui::IsItemHovered();

    const ImVec2 item_min = ImGui::GetItemRectMin();
    const ImVec2 item_max = ImGui::GetItemRectMax();
    const ImVec2 child_pos = ImGui::GetWindowPos();
    const ImVec2 child_size = ImGui::GetWindowSize();
    const ImVec2 clip_min(child_pos.x, child_pos.y);
    const ImVec2 clip_max(child_pos.x + child_size.x, child_pos.y + child_size.y);

    const float plot_left = item_min.x + margin_left;
    const float plot_right = plot_left + static_cast<float>(count - 1) * x_step;
    const float plot_top = item_min.y + margin_top;
    const float plot_bottom = item_max.y - margin_bottom;
    if (plot_hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        ImGuiIO& io = ImGui::GetIO();
        const float factor = std::pow(1.12f, io.MouseWheel);
        if (io.KeyCtrl || io.KeySuper) {
            state.zoom_y = std::max(0.01f, state.zoom_y * factor);
        } else {
            const float old_step = x_step;
            const float mouse_index = std::clamp(
                (io.MousePos.x - plot_left) / old_step,
                0.0f,
                static_cast<float>(count - 1u));
            state.zoom_x = std::max(0.01f, state.zoom_x * factor);
            const float new_step = plot_x_step(state);
            const float mouse_local_x = io.MousePos.x - child_pos.x;
            ImGui::SetScrollX(std::max(0.0f, mouse_index * new_step + margin_left - mouse_local_x));
        }
    }
    const float visible_left_local = ImGui::GetScrollX();
    const float visible_width = ImGui::GetWindowWidth();
    const float index_left = std::max(0.0f, (visible_left_local - margin_left) / x_step);
    const float index_right = std::min(static_cast<float>(count - 1),
        (visible_left_local + visible_width - margin_left) / x_step);
    const std::size_t first = std::min(count - 1,
        static_cast<std::size_t>(std::floor(std::max(0.0f, index_left))));
    const std::size_t last = std::min(count - 1,
        static_cast<std::size_t>(std::ceil(std::max(index_left, index_right))) + 2u);

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

    const std::size_t delta_index = std::min(analysis.max_delta_sample_index, count - 1);
    if (delta_index >= first && delta_index <= last) {
        const float x = plot_left + static_cast<float>(delta_index) * x_step;
        draw->AddRectFilled(ImVec2(x - 2.0f, plot_top),
                            ImVec2(x + std::max(2.0f, x_step + 2.0f), plot_bottom),
                            IM_COL32(208, 127, 127, 40));
        draw->AddText(ImVec2(x + 6.0f, plot_top + 6.0f),
                      skald::tokens::status::destructive, "max delta");
    }

    if (state.segment.active) {
        clamp_segment(state);
        const auto [selection_first, selection_last] = normalized_selection(state);
        const float start_x = plot_left + static_cast<float>(selection_first) * x_step;
        const float end_x = plot_left + static_cast<float>(selection_last) * x_step;
        draw->AddRectFilled(ImVec2(start_x, plot_top), ImVec2(end_x, plot_bottom),
                            IM_COL32(88, 170, 210, 32));
        draw->AddLine(ImVec2(start_x, plot_top), ImVec2(start_x, plot_bottom),
                      IM_COL32(88, 170, 210, 210), 2.0f);
        draw->AddLine(ImVec2(end_x, plot_top), ImVec2(end_x, plot_bottom),
                      IM_COL32(88, 170, 210, 210), 2.0f);
        draw->AddCircleFilled(ImVec2(start_x, plot_top + 8.0f), 4.0f,
                              IM_COL32(88, 170, 210, 235));
        draw->AddCircleFilled(ImVec2(end_x, plot_top + 8.0f), 4.0f,
                              IM_COL32(88, 170, 210, 235));
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const auto index_from_mouse = [&]() {
        const double index = std::round((mouse.x - plot_left) / x_step);
        return static_cast<std::size_t>(
            std::clamp(index, 0.0, static_cast<double>(count - 1u)));
    };
    if (plot_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const std::size_t index = index_from_mouse();
        if (!state.segment.active) {
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
    if (state.selection_drag_handle != 0) {
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
    }

    const Entity* data = data_entity(state);
    const bool draw_data = data == nullptr || data->visible;
    if (draw_data && state.show_lines) {
        for (std::size_t i = first; i < last && i + 1 < count; ++i) {
            const ImVec2 a(plot_left + static_cast<float>(i) * x_step,
                           y_for_value(analysis.scalars[i], plot_top, plot_bottom, y_zoom));
            const ImVec2 b(plot_left + static_cast<float>(i + 1) * x_step,
                           y_for_value(analysis.scalars[i + 1], plot_top, plot_bottom, y_zoom));
            draw->AddLine(a, b, IM_COL32(90, 142, 210, 235), 1.2f);
        }
    }
    if (draw_data && state.show_points && x_step >= 3.0f) {
        for (std::size_t i = first; i <= last && i < count; ++i) {
            const ImVec2 point(plot_left + static_cast<float>(i) * x_step,
                               y_for_value(analysis.scalars[i], plot_top, plot_bottom, y_zoom));
            draw->AddCircleFilled(point, 2.0f, IM_COL32(230, 232, 235, 220));
        }
    }

    static constexpr std::array<ImU32, 6> kWaveColors = {
        IM_COL32(214, 160, 63, 220),
        IM_COL32(74, 160, 104, 220),
        IM_COL32(201, 94, 139, 220),
        IM_COL32(139, 122, 224, 220),
        IM_COL32(215, 107, 73, 220),
        IM_COL32(94, 183, 188, 220),
    };
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

    draw->AddRect(ImVec2(plot_left, plot_top), ImVec2(plot_right, plot_bottom),
                  skald::tokens::border::default_, 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(child_pos.x + margin_left, child_pos.y + 8.0f),
                  skald::tokens::ink::primary, "x-line section");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 178.0f, child_pos.y + 8.0f),
                  IM_COL32(90, 142, 210, 235), "data");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 108.0f, child_pos.y + 8.0f),
                  IM_COL32(214, 160, 63, 210), "waves");

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
}

void draw_file_dialog(AppState& state) {
    if (state.pending_dialog != DialogPurpose::None) {
        state.active_dialog = state.pending_dialog;
        state.pending_dialog = DialogPurpose::None;
        skald::OpenFileDialog(kOpenFileDialogId);
    }

    if (state.active_dialog == DialogPurpose::None) {
        return;
    }

    refresh_file_dialog_entries(state);
    const skald::FileDialogMode mode = state.active_dialog == DialogPurpose::SaveWave
        ? skald::FileDialogMode::Save
        : skald::FileDialogMode::Open;
    const skald::FileDialogResult result = skald::BeginFileDialog(
        kOpenFileDialogId,
        mode,
        state.file_dialog,
        std::span<const skald::FileDialogEntry>(state.file_dialog_entries.data(),
                                                state.file_dialog_entries.size()));

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
    if (io.WantTextInput) {
        return;
    }
    const bool shortcut = io.KeyCtrl || io.KeySuper;
    if (shortcut && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
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
    draw_splash(state);
    ImGui::SetNextWindowPos(ImVec2(-10000.0f, -10000.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f), ImGuiCond_Always);
    ImGui::Begin("##dialog_host", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoInputs |
                 ImGuiWindowFlags_NoBackground);
    draw_file_dialog(state);
    ImGui::End();
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(args.width, args.height, "thrystr", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    force_undecorated_window(window);
    glfwSetWindowSizeLimits(window,
                            kMinWindowWidth,
                            kMinWindowHeight,
                            GLFW_DONT_CARE,
                            GLFW_DONT_CARE);
    glfwShowWindow(window);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    ChromeCursors chrome_cursors = create_chrome_cursors();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    skald::ApplyDefaults(skald::tokens::accents::cyan);

    AppState state;
    initialize_file_dialog(state);
    state.fonts = skald::LoadFonts(io, THRYSTR_SKALD_FONT_DIR);
    if (!args.file.empty()) {
        std::snprintf(state.path, sizeof(state.path), "%s", args.file.c_str());
        load_path(state);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    int rendered_frames = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        draw_app(state, window, chrome_cursors);
        if (state.request_close) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui::Render();
        int fb_width = 0;
        int fb_height = 0;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        glViewport(0, 0, fb_width, fb_height);
        glClearColor(0.035f, 0.040f, 0.055f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        ++rendered_frames;
        if (!args.screenshot.empty() && rendered_frames >= args.frames) {
            if (!save_screenshot(args.screenshot, fb_width, fb_height)) {
                std::fprintf(stderr, "could not write screenshot: %s\n", args.screenshot.c_str());
            }
            break;
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    destroy_chrome_cursors(chrome_cursors);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
