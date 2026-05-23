#include <thrystr/scalar_analysis.hpp>

#include <GLFW/glfw3.h>
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
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct Args {
    std::string file;
    std::string screenshot;
    int width = 1600;
    int height = 900;
    int frames = 0;
};

struct AppState {
    char path[4096] = {};
    std::optional<thrystr::Analysis> analysis;
    std::string status = "No file loaded";
    bool show_points = true;
    bool show_lines = true;
    bool show_sine = true;
    bool show_cosine = true;
    float zoom_x = 1.0f;
    float zoom_y = 1.0f;
    float max_slope = thrystr::kDefaultMaxSlope;
    float wave_tolerance = static_cast<float>(thrystr::kDefaultWaveTolerance);
    int phase_steps = 720;
    int phase_test_points = 65536;
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

void load_path(AppState& state) {
    try {
        state.status = "Analyzing";
        state.analysis = thrystr::analyze_file(
            std::filesystem::path(state.path),
            thrystr::kDefaultWindowBytes,
            state.max_slope,
            static_cast<double>(state.wave_tolerance),
            state.phase_steps,
            static_cast<std::size_t>(state.phase_test_points));

        const auto& analysis = *state.analysis;
        state.status = "Loaded " + analysis.source_path.filename().string() +
                       " / sample " + format_bytes(analysis.window.length);
    } catch (const std::exception& error) {
        state.analysis.reset();
        state.status = error.what();
    }
}

void draw_titlebar(AppState& state) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport.x, 52.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));
    ImGui::Begin("##titlebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar();

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f), ImVec2(viewport.x, 52.0f),
        skald::tokens::surface::deep,
        skald::tokens::surface::deep,
        skald::tokens::surface::window,
        skald::tokens::surface::window);

    if (state.fonts.sans_md) {
        ImGui::PushFont(state.fonts.sans_md);
    }
    ImGui::TextUnformatted("thrystr");
    if (state.fonts.sans_md) {
        ImGui::PopFont();
    }

    ImGui::SameLine(104.0f);
    ImGui::SetNextItemWidth(std::max(260.0f, viewport.x - 430.0f));
    ImGui::InputTextWithHint("##path", "/path/to/binary", state.path, sizeof(state.path));

    ImGui::SameLine();
    if (skald::AccentButton("Load", ImVec2(76.0f, 0.0f))) {
        load_path(state);
    }

    ImGui::SameLine();
    ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted), "%s",
                       state.status.c_str());

    ImGui::End();
}

void slider_float_row(const char* label,
                      const char* id,
                      float* value,
                      float min_value,
                      float max_value,
                      const char* format) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat(id, value, min_value, max_value, format);
}

void slider_int_row(const char* label,
                    const char* id,
                    int* value,
                    int min_value,
                    int max_value) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt(id, value, min_value, max_value);
}

void draw_inspector(AppState& state) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 52.0f));
    ImGui::SetNextWindowSize(ImVec2(336.0f, viewport.y - 52.0f));
    ImGui::Begin("Inspector", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

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
    } else {
        skald::KvRowStatus("state", "empty", skald::BadgeTone::Muted);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    skald::SectionHeader("Render");
    skald::PillToggle("points", &state.show_points);
    skald::PillToggle("lines", &state.show_lines);
    skald::PillToggle("sine", &state.show_sine);
    skald::PillToggle("cosine", &state.show_cosine);
    slider_float_row("x zoom", "##xzoom", &state.zoom_x, 0.10f, 8.0f, "%.2f");
    slider_float_row("y zoom", "##yzoom", &state.zoom_y, 0.25f, 4.0f, "%.2f");

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    skald::SectionHeader("Load Params");
    slider_float_row("max slope", "##maxslope", &state.max_slope, 0.05f, 1.0f, "%.2f");
    slider_float_row("wave tolerance", "##wavetolerance", &state.wave_tolerance,
                     0.001f, 0.10f, "%.3f");
    slider_int_row("phase steps", "##phasesteps", &state.phase_steps, 90, 2880);
    slider_int_row("phase samples", "##phasesamples", &state.phase_test_points,
                   1024, 262144);

    if (state.analysis) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        skald::SectionHeader("Wave Fit");
        const thrystr::Analysis& a = *state.analysis;
        skald::KvRow("sine hits", "%s", format_count(a.sine.hits).c_str());
        skald::KvRow("sine phase", "%.4f", a.sine.phase_radians);
        skald::KvRow("cos hits", "%s", format_count(a.cosine.hits).c_str());
        skald::KvRow("cos phase", "%.4f", a.cosine.phase_radians);
        skald::KvRow("tested", "%s", format_count(a.sine.tested_points).c_str());
    }

    ImGui::End();
}

float y_for_value(float value, float top, float bottom, float zoom_y) {
    const float scaled = value * zoom_y;
    const float normalized = (scaled + 1.0f) * 0.5f;
    return bottom - normalized * (bottom - top);
}

void draw_wave_overlay(const thrystr::Analysis& analysis,
                       float phase,
                       bool cosine,
                       float x_step,
                       float plot_left,
                       float plot_top,
                       float plot_bottom,
                       float zoom_y,
                       std::size_t first,
                       std::size_t last,
                       ImU32 color,
                       ImDrawList* draw) {
    if (analysis.scalars.size() < 2 || last <= first) {
        return;
    }

    const double denom = static_cast<double>(analysis.scalars.size() - 1);
    const std::size_t samples = std::min<std::size_t>(2048, last - first + 1);
    ImVec2 previous{};
    bool has_previous = false;

    for (std::size_t s = 0; s < samples; ++s) {
        const double t = samples > 1
            ? static_cast<double>(s) / static_cast<double>(samples - 1)
            : 0.0;
        const double index = static_cast<double>(first) +
                             t * static_cast<double>(last - first);
        const double theta = 2.0 * kPi * index / denom + phase;
        const float value = static_cast<float>(cosine ? std::cos(theta) : std::sin(theta));
        const ImVec2 point(plot_left + static_cast<float>(index) * x_step,
                           y_for_value(value, plot_top, plot_bottom, zoom_y));
        if (has_previous) {
            draw->AddLine(previous, point, color, 1.4f);
        }
        previous = point;
        has_previous = true;
    }
}

void draw_plot(AppState& state) {
    const ImVec2 viewport = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(336.0f, 52.0f));
    ImGui::SetNextWindowSize(ImVec2(viewport.x - 336.0f, viewport.y - 52.0f));
    ImGui::Begin("Waveform", nullptr,
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);

    if (!state.analysis || state.analysis->scalars.empty()) {
        ImGui::TextColored(skald::tokens::to_vec4(skald::tokens::ink::muted),
                           "No sample");
        ImGui::End();
        return;
    }

    const thrystr::Analysis& analysis = *state.analysis;
    const std::size_t count = analysis.scalars.size();
    const float x_step = std::max(0.05f, analysis.x_scale * state.zoom_x);
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
        const float y = y_for_value(y_value, plot_top, plot_bottom, state.zoom_y);
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
        std::snprintf(label, sizeof(label), "%zu", i);
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

    if (state.show_sine) {
        draw_wave_overlay(analysis, static_cast<float>(analysis.sine.phase_radians),
                          false, x_step, plot_left, plot_top, plot_bottom,
                          state.zoom_y, first, last, IM_COL32(214, 160, 63, 210), draw);
    }
    if (state.show_cosine) {
        draw_wave_overlay(analysis, static_cast<float>(analysis.cosine.phase_radians),
                          true, x_step, plot_left, plot_top, plot_bottom,
                          state.zoom_y, first, last, IM_COL32(74, 160, 104, 210), draw);
    }

    if (state.show_lines) {
        for (std::size_t i = first; i < last && i + 1 < count; ++i) {
            const ImVec2 a(plot_left + static_cast<float>(i) * x_step,
                           y_for_value(analysis.scalars[i], plot_top, plot_bottom, state.zoom_y));
            const ImVec2 b(plot_left + static_cast<float>(i + 1) * x_step,
                           y_for_value(analysis.scalars[i + 1], plot_top, plot_bottom, state.zoom_y));
            draw->AddLine(a, b, IM_COL32(90, 142, 210, 235), 1.2f);
        }
    }
    if (state.show_points && x_step >= 3.0f) {
        for (std::size_t i = first; i <= last && i < count; ++i) {
            const ImVec2 point(plot_left + static_cast<float>(i) * x_step,
                               y_for_value(analysis.scalars[i], plot_top, plot_bottom, state.zoom_y));
            draw->AddCircleFilled(point, 2.0f, IM_COL32(230, 232, 235, 220));
        }
    }

    draw->AddRect(ImVec2(plot_left, plot_top), ImVec2(plot_right, plot_bottom),
                  skald::tokens::border::default_, 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(child_pos.x + margin_left, child_pos.y + 8.0f),
                  skald::tokens::ink::primary, "scalar sample");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 220.0f, child_pos.y + 8.0f),
                  IM_COL32(90, 142, 210, 235), "data");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 166.0f, child_pos.y + 8.0f),
                  IM_COL32(214, 160, 63, 210), "sine");
    draw->AddText(ImVec2(child_pos.x + child_size.x - 104.0f, child_pos.y + 8.0f),
                  IM_COL32(74, 160, 104, 210), "cosine");

    draw->PopClipRect();
    ImGui::EndChild();
    ImGui::End();
}

void draw_app(AppState& state) {
    draw_titlebar(state);
    draw_inspector(state);
    draw_plot(state);
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parse_args(argc, argv);

    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(args.width, args.height, "thrystr", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    skald::ApplyDefaults(skald::tokens::accents::cyan);

    AppState state;
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

        draw_app(state);

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
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
