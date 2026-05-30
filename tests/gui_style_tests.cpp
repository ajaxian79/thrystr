// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/file_dialog.hpp>
#include <thrystr/gui/icons.hpp>
#include <thrystr/gui/palette.hpp>

#include <cassert>
#include <cstring>

namespace {

void test_palette_alpha() {
    const ImU32 color =
        thrystr::gui::palette::with_alpha(thrystr::gui::palette::status::info, 0.5f);
    const auto alpha = (color >> IM_COL32_A_SHIFT) & 0xff;
    assert(alpha == 127u);
}

void test_palette_vector() {
    const ImVec4 color = thrystr::gui::palette::to_vec4(thrystr::gui::palette::ink::primary);
    assert(color.w == 1.0f);
    assert(color.x > color.z * 0.9f);
}

void test_icon_range() {
    assert(thrystr::gui::icons::kRangeLo <= thrystr::gui::icons::kRangeHi);
    assert(std::strlen(thrystr::gui::icons::kFile) > 0u);
}

void test_file_dialog_defaults() {
    thrystr::gui::FileDialogState state;
    assert(state.row_sel == -1);
    assert(std::strcmp(state.cwd, "/home/user") == 0);
}

} // namespace

int main() {
    test_palette_alpha();
    test_palette_vector();
    test_icon_range();
    test_file_dialog_defaults();
    return 0;
}
