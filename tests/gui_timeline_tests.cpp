// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/runtime_config.hpp>
#include <thrystr/gui/timeline_draw.hpp>

#include <imgui.h>

#include <cassert>

namespace {

void test_visible_range() {
    const thrystr::gui::TimelineViewport view{1000u, 250.0f, 500.0f, 50.0f, 2.0f};
    const thrystr::gui::SampleRange range = thrystr::gui::visible_sample_range(view);
    assert(range.valid);
    assert(range.first == 100u);
    assert(range.last == 352u);
    assert(range.count() == 253u);
}

void test_stride() {
    assert(thrystr::gui::pixel_stride(0.25f, 58u) == 58u);
    assert(thrystr::gui::pixel_stride(4.0f, 58u) == 15u);
    assert(thrystr::gui::pixel_stride(200.0f, 58u) == 1u);
}

void test_runtime_config() {
    ImGui::CreateContext();
    thrystr::gui::apply_runtime_config(ImGui::GetIO());
    assert(ImGui::GetIO().IniFilename == nullptr);
    assert(ImGui::GetIO().LogFilename == nullptr);
    assert(!ImGui::GetIO().ConfigInputTextCursorBlink);
    ImGui::DestroyContext();
}

} // namespace

int main() {
    test_visible_range();
    test_stride();
    test_runtime_config();
    return 0;
}
