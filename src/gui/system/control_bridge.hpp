// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/controls.hpp>

#include <skald/widgets.h>

namespace thrystr::gui {

inline skald::BadgeTone vendor_tone(StatusTone tone) {
    switch (tone) {
    case StatusTone::Accent:
        return skald::BadgeTone::Accent;
    case StatusTone::Success:
        return skald::BadgeTone::Success;
    case StatusTone::Warning:
        return skald::BadgeTone::Warning;
    case StatusTone::Destructive:
        return skald::BadgeTone::Destructive;
    case StatusTone::Info:
        return skald::BadgeTone::Info;
    case StatusTone::Muted:
        return skald::BadgeTone::Muted;
    }
    return skald::BadgeTone::Muted;
}

} // namespace thrystr::gui
