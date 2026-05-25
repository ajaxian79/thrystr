// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/gui/interface_types.hpp>

#include <string_view>

namespace thrystr::gui {

class InterfaceSession {
  public:
    InterfaceSession() = default;
    ~InterfaceSession();

    InterfaceSession(const InterfaceSession&) = delete;
    InterfaceSession& operator=(const InterfaceSession&) = delete;

    bool active() const noexcept;
    void start(WindowHandle window, std::string_view font_directory, FontSet& fonts);
    void shutdown(FontSet& fonts) noexcept;
    void begin_frame();
    FrameSize render_frame(WindowHandle window);

  private:
    bool active_ = false;
};

} // namespace thrystr::gui
