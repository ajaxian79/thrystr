// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/ui/window.hpp>

#include <algorithm>
#include <stdexcept>

namespace thrystr::ui {

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

void Component::sync(const ApplicationOverlay&) {}

Window::Window(std::string name, std::string label, Rect bounds) {
    if (name.empty()) {
        throw std::invalid_argument("window name must not be empty");
    }
    state_.name = std::move(name);
    state_.label = label.empty() ? state_.name : std::move(label);
    state_.bounds = bounds;
}

WindowState& Window::state() noexcept { return state_; }

const WindowState& Window::state() const noexcept { return state_; }

Component& Window::add_component(std::unique_ptr<Component> component) {
    if (!component) {
        throw std::invalid_argument("component must not be null");
    }
    if (find_component(component->name())) {
        throw std::invalid_argument("duplicate component: " + component->name());
    }

    Component& component_ref = *component;
    owned_components_.push_back(std::move(component));
    component_order_.push_back(&component_ref);
    return component_ref;
}

Component* Window::find_component(std::string_view name) noexcept {
    const auto found = std::find_if(
        component_order_.begin(), component_order_.end(),
        [name](const Component* component) { return component && component->name() == name; });
    return found == component_order_.end() ? nullptr : *found;
}

const Component* Window::find_component(std::string_view name) const noexcept {
    const auto found = std::find_if(
        component_order_.begin(), component_order_.end(),
        [name](const Component* component) { return component && component->name() == name; });
    return found == component_order_.end() ? nullptr : *found;
}

void Window::sync(const ApplicationOverlay& overlay) {
    for (Component* component : component_order_) {
        if (component && component->visible()) {
            component->sync(overlay);
        }
    }
}

std::span<Component* const> Window::components() const noexcept { return component_order_; }

} // namespace thrystr::ui
