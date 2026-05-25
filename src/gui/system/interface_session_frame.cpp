// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/interface_session.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <skald/skald.h>

namespace thrystr::gui {
namespace {

void clear_background() {
    constexpr unsigned int color = skald::tokens::surface::deep;
    glClearColor(static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xff) / 255.0f,
                 static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xff) / 255.0f,
                 static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xff) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

} // namespace

void InterfaceSession::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

FrameSize InterfaceSession::render_frame(WindowHandle window) {
    FrameSize size;
    ImGui::Render();
    auto* native_window = static_cast<GLFWwindow*>(window);
    glfwGetFramebufferSize(native_window, &size.width, &size.height);
    glViewport(0, 0, size.width, size.height);
    clear_background();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(native_window);
    return size;
}

} // namespace thrystr::gui
