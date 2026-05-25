// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/image.hpp>
#include <thrystr/gui/interface_types.hpp>

#include <cassert>
#include <cmath>
namespace {

void test_font_set_helpers() {
    thrystr::gui::FontSet fonts;
    fonts.loaded = true;
    assert(fonts.body() == nullptr);
    assert(fonts.heading() == nullptr);
    assert(fonts.code() == nullptr);

    fonts.clear();
    assert(!fonts.loaded);
}

void test_frame_size_math() {
    thrystr::gui::FrameSize empty;
    assert(empty.empty());
    assert(empty.aspect_ratio() == 0.0f);

    thrystr::gui::FrameSize wide{1920, 1080};
    assert(!wide.empty());
    assert(std::abs(wide.aspect_ratio() - 1.7777778f) < 0.0001f);
}

void test_texture_validity() {
    thrystr::gui::Texture empty;
    assert(!empty.valid());
    assert(empty.empty());
    thrystr::gui::Texture texture{7, 32, 16};
    assert(texture.valid());
    assert(texture.area() == 512);
}
} // namespace

int main() {
    test_font_set_helpers();
    test_frame_size_math();
    test_texture_validity();
    return 0;
}
