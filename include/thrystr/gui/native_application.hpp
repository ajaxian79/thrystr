// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

namespace thrystr::gui {

// Owns process-wide GUI startup and shutdown.
// Keep this boundary small so app code never
// depends on the window backend directly.
class NativeApplication {
  public:
    NativeApplication();
    ~NativeApplication();

    NativeApplication(const NativeApplication&) = delete;
    NativeApplication& operator=(const NativeApplication&) = delete;

    bool ready() const noexcept;
    bool initialized() const noexcept;
    explicit operator bool() const noexcept;

  private:
    bool ready_ = false;
};

} // namespace thrystr::gui
