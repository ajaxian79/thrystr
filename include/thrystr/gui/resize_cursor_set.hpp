// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

namespace thrystr::gui {

enum class ResizeCursor {
    Horizontal,
    Vertical,
    DiagonalDown,
    DiagonalUp,
};

class ResizeCursorSet {
  public:
    ResizeCursorSet();
    ~ResizeCursorSet();

    ResizeCursorSet(const ResizeCursorSet&) = delete;
    ResizeCursorSet& operator=(const ResizeCursorSet&) = delete;

    void* cursor(ResizeCursor cursor) const noexcept;

  private:
    void* horizontal_ = nullptr;
    void* vertical_ = nullptr;
    void* diagonal_down_ = nullptr;
    void* diagonal_up_ = nullptr;
};

} // namespace thrystr::gui
