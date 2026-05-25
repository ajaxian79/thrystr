// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/font_loader.hpp>
#include <thrystr/gui/interface_session.hpp>
#include <thrystr/gui/palette.hpp>
#include <thrystr/gui/style.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace thrystr::gui {

InterfaceSession::~InterfaceSession() = default;

bool InterfaceSession::active() const noexcept { return active_; }

void InterfaceSession::start(WindowHandle window, std::string_view font_directory, FontSet& fonts) {
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window));
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    apply_defaults(palette::accent::cyan);
    fonts = load_fonts(ImGui::GetIO(), font_directory.data());
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window), true);
    ImGui_ImplOpenGL3_Init("#version 130");
    active_ = true;
}

void InterfaceSession::shutdown(FontSet& fonts) noexcept {
    if (!active_) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    fonts.clear();
    active_ = false;
}

} // namespace thrystr::gui
