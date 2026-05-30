// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <string>
#include <string_view>

namespace thrystr::gui {

class Component {
  public:
    Component(std::string name, std::string label);
    virtual ~Component() = default;

    const std::string& name() const noexcept;
    const std::string& label() const noexcept;

    bool visible() const noexcept;
    void set_visible(bool visible) noexcept;

    virtual void render() = 0;

  private:
    std::string name_;
    std::string label_;
    bool visible_ = true;
};

} // namespace thrystr::gui
