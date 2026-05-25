// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/splash.hpp>

#include <skald/widgets.h>

#include <vector>

namespace thrystr::gui {
namespace {

SplashChoice choice_from(skald::SplashChoice choice) {
    SplashChoice result;
    result.index = choice.index;
    result.kind = choice.kind == skald::SplashChoice::Kind::Action   ? SplashChoice::Kind::Action
                  : choice.kind == skald::SplashChoice::Kind::Recent ? SplashChoice::Kind::Recent
                                                                     : SplashChoice::Kind::None;
    return result;
}

} // namespace

SplashChoice splash(std::string_view wordmark, std::string_view tagline,
                    std::span<const SplashRecent> recents, std::span<const SplashAction> actions,
                    ImTextureID hero_texture, ImVec2 hero_size, ImFont* hero_font) {
    std::vector<skald::SplashRecent> vendor_recents;
    std::vector<skald::SplashAction> vendor_actions;
    for (const SplashRecent& recent : recents) {
        vendor_recents.push_back({recent.title, recent.path, recent.modified});
    }
    for (const SplashAction& action : actions) {
        vendor_actions.push_back({action.label, action.shortcut});
    }
    return choice_from(skald::Splash(wordmark, tagline, vendor_recents, vendor_actions,
                                     hero_texture, hero_size, hero_font));
}

} // namespace thrystr::gui
