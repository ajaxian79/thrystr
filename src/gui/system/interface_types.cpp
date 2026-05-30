// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/interface_types.hpp>

namespace thrystr::gui {

void FontSet::clear() noexcept {
    sans = nullptr;
    sans_md = nullptr;
    mono = nullptr;
    hero = nullptr;
    loaded = false;
}

ImFont* FontSet::body() const noexcept { return sans; }

ImFont* FontSet::heading() const noexcept { return sans_md ? sans_md : sans; }

ImFont* FontSet::code() const noexcept { return mono; }

bool FrameSize::empty() const noexcept { return width <= 0 || height <= 0; }

float FrameSize::aspect_ratio() const noexcept {
    if (empty()) {
        return 0.0f;
    }
    return static_cast<float>(width) / static_cast<float>(height);
}

} // namespace thrystr::gui
