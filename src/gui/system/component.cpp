// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/component.hpp>

#include <stdexcept>

namespace thrystr::gui {

Component::Component(std::string name, std::string label)
    : name_(std::move(name)), label_(std::move(label)) {
    if (name_.empty()) {
        throw std::invalid_argument("component name must not be empty");
    }
    if (label_.empty()) {
        label_ = name_;
    }
}

const std::string& Component::name() const noexcept { return name_; }

const std::string& Component::label() const noexcept { return label_; }

bool Component::visible() const noexcept { return visible_; }

void Component::set_visible(bool visible) noexcept { visible_ = visible; }

} // namespace thrystr::gui
