// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/gui/component.hpp>
#include <thrystr/gui/window_types.hpp>

#include <cassert>

namespace {

class CountingComponent final : public thrystr::gui::Component {
  public:
    CountingComponent() : Component("counter", "Counter") {}

    void render() override { ++render_count; }

    int render_count = 0;
};

void test_component_contract() {
    CountingComponent component;
    assert(component.name() == "counter");
    assert(component.label() == "Counter");
    assert(component.visible());
    component.set_visible(false);
    assert(!component.visible());
    component.render();
    assert(component.render_count == 1);
}

void test_window_request_value_types() {
    thrystr::gui::WindowRequest request{"workspace", {1200, 800}, {760, 460}, true, false};
    assert(request.title == "workspace");
    assert(request.size.width == 1200);
    assert(request.minimum_size.height == 460);
    assert(request.resizable);
}

} // namespace

int main() {
    test_component_contract();
    test_window_request_value_types();
    return 0;
}
