// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/controls.hpp>

#include "control_bridge.hpp"

#include <cstdarg>
#include <cstdio>

namespace thrystr::gui {

void section_header(std::string_view label) { skald::SectionHeader(label); }

void muted_separator(float top_pad, float bottom_pad) {
    skald::MutedSeparator(top_pad, bottom_pad);
}

void key_value_row(std::string_view label, const char* value_format, ...) {
    char value[512];
    va_list args;
    va_start(args, value_format);
    std::vsnprintf(value, sizeof(value), value_format, args);
    va_end(args);
    skald::KvRow(label, "%s", value);
}

void status_row(std::string_view label, std::string_view value, StatusTone tone) {
    skald::KvRowStatus(label, value, vendor_tone(tone));
}

} // namespace thrystr::gui
