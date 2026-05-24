// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/model/dataset.hpp>
#include <thrystr/model/workspace.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace thrystr::ui {

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct ApplicationOverlay {
    thrystr::model::Workspace* workspace = nullptr;
    const thrystr::model::Dataset* dataset = nullptr;
};

class Component {
  public:
    Component(std::string name, std::string label);
    virtual ~Component() = default;

    const std::string& name() const noexcept;
    const std::string& label() const noexcept;
    bool visible() const noexcept;
    void set_visible(bool visible) noexcept;

    virtual void sync(const ApplicationOverlay& overlay);

  private:
    std::string name_;
    std::string label_;
    bool visible_ = true;
};

struct WindowState {
    std::string name;
    std::string label;
    Rect bounds;
    bool visible = true;
    bool focused = false;
};

class Window {
  public:
    Window(std::string name, std::string label, Rect bounds = {});

    WindowState& state() noexcept;
    const WindowState& state() const noexcept;

    template <typename ComponentT, typename... Args> ComponentT& add_component(Args&&... args) {
        auto component = std::make_unique<ComponentT>(std::forward<Args>(args)...);
        ComponentT& component_ref = *component;
        add_component(std::move(component));
        return component_ref;
    }

    Component& add_component(std::unique_ptr<Component> component);
    Component* find_component(std::string_view name) noexcept;
    const Component* find_component(std::string_view name) const noexcept;
    void sync(const ApplicationOverlay& overlay);

    std::span<Component* const> components() const noexcept;

  private:
    WindowState state_;
    std::vector<std::unique_ptr<Component>> owned_components_;
    std::vector<Component*> component_order_;
};

} // namespace thrystr::ui
