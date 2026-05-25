// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <imgui.h>

#include <span>
#include <string_view>

namespace thrystr::gui {

struct SplashRecent {
    std::string_view title;
    std::string_view path;
    std::string_view modified;
};

struct SplashAction {
    std::string_view label;
    std::string_view shortcut;
};

struct SplashChoice {
    enum class Kind {
        None,
        Recent,
        Action,
    };
    Kind kind = Kind::None;
    int index = -1;
};

SplashChoice splash(std::string_view wordmark, std::string_view tagline,
                    std::span<const SplashRecent> recents, std::span<const SplashAction> actions,
                    ImTextureID hero_texture = 0, ImVec2 hero_size = ImVec2(0.0f, 0.0f),
                    ImFont* hero_font = nullptr);

} // namespace thrystr::gui
