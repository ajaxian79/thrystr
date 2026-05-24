// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/entity.hpp>
#include <thrystr/model/fitter.hpp>
#include <thrystr/model/node_graph.hpp>
#include <thrystr/model/workspace.hpp>
#include <thrystr/ui/window.hpp>

#include <cassert>
#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

namespace {

class CountingFitter final : public thrystr::model::Fitter {
  public:
    std::string_view name() const noexcept override { return "counting"; }
    std::string_view label() const noexcept override { return "Counting Fitter"; }

    thrystr::model::FitResult fit(const thrystr::model::FitRequest& request) override {
        assert(request.dataset != nullptr);
        assert(request.workspace != nullptr);

        const thrystr::model::SegmentRange range =
            request.segment.value_or(thrystr::model::SegmentRange{0, request.dataset->count()});
        auto& function = request.workspace->create_entity<thrystr::model::FunctionEntity>(
            "fit function", "Fit Function");
        auto& layer = request.target_layer_id == 0
                          ? request.workspace->create_layer("fit layer", "Fit Layer")
                          : *request.workspace->find_layer(request.target_layer_id);
        request.workspace->attach_entity_to_layer(function.id(), layer.id);

        return {
            true,
            "fit " + std::to_string(range.length) + " samples",
            {function.id()},
            {layer.id},
        };
    }
};

class SyncCountingComponent final : public thrystr::ui::Component {
  public:
    SyncCountingComponent() : Component("counter", "Counter") {}

    void sync(const thrystr::ui::ApplicationOverlay& overlay) override {
        assert(overlay.workspace != nullptr);
        ++sync_count;
    }

    int sync_count = 0;
};

void test_property_bag_lookup_and_order() {
    thrystr::model::PropertyBag bag;
    bag.register_property("amplitude", "Amplitude", 2.0);
    bag.register_property("name", "Name", std::string("wave"));

    assert(bag.contains("amplitude"));
    assert(bag.double_value("amplitude").value() == 2.0);
    assert(bag.string_value("name").value() == "wave");
    assert(bag.ordered().size() == 2);

    bag.set("amplitude", 3.5);
    assert(bag.double_value("amplitude").value() == 3.5);
}

void test_function_entity_plots_standard_math() {
    thrystr::model::FunctionEntity function;
    function.properties().set("wavelength_nm", 4.0);
    function.properties().set("amplitude", 2.0);
    function.properties().set("amplitude_offset", -1.0);
    function.properties().set("phase_nm", 0.0);

    assert(std::abs(function.plot(0.0).value()) < 1.0e-9);
    assert(std::abs(function.plot(1.0).value() - 1.0) < 1.0e-9);

    function.properties().set("function",
                              static_cast<std::int64_t>(thrystr::model::StandardFunction::Cosine));
    assert(std::abs(function.plot(0.0).value() - 1.0) < 1.0e-9);

    function.properties().set("rotation_degrees", 45.0);
    assert(function.plot(1.0).has_value());
}

void test_workspace_layers_collections_and_entity_subclasses() {
    thrystr::model::Workspace workspace;
    auto dataset =
        std::make_shared<thrystr::model::VectorDataset>(std::vector<double>{-1.0, 0.0, 1.0}, 2.0);
    auto& raw = workspace.create_entity<thrystr::model::RawDataEntity>(dataset, "raw", "Raw");
    auto& matrix = workspace.create_entity<thrystr::model::MatrixEntity>("matrix", "Matrix");
    matrix.properties().set("columns", static_cast<std::int64_t>(2));
    matrix.set_plotter(
        [](double x, const thrystr::model::PropertyBag&) { return std::vector<double>{x, x * x}; });
    auto& refined = workspace.create_entity<thrystr::model::RefinedDataEntity>(
        raw.id(), matrix.id(), "refined", "Refined");

    auto& layer = workspace.create_layer("data layer", "Data Layer");
    workspace.attach_entity_to_layer(raw.id(), layer.id);
    workspace.attach_entity_to_layer(matrix.id(), layer.id);

    auto& collection = workspace.create_collection("experiment", "Experiment");
    workspace.attach_layer_to_collection(layer.id, collection.id);
    workspace.attach_entity_to_collection(refined.id(), collection.id);

    assert(workspace.find_entity(raw.id())->kind() == thrystr::model::EntityKind::RawData);
    assert(matrix.plot_row(3.0).at(1) == 9.0);
    assert(layer.entities.size() == 2);
    assert(collection.layers.size() == 1);
    assert(collection.entities.size() == 1);
}

void test_fitter_registry_accepts_dataset_sections_and_creates_layers() {
    thrystr::model::Workspace workspace;
    thrystr::model::VectorDataset dataset({0.0, 0.5, -0.25, 1.0}, 1.0);
    thrystr::model::FitterRegistry registry;
    registry.register_fitter(std::make_unique<CountingFitter>());

    thrystr::model::Fitter* fitter = registry.find("counting");
    assert(fitter != nullptr);
    const thrystr::model::FitResult result =
        fitter->fit({&dataset, &workspace, thrystr::model::SegmentRange{1, 2}, 0});

    assert(result.success);
    assert(result.created_entities.size() == 1);
    assert(result.created_layers.size() == 1);
    assert(workspace.layers().size() == 1);
    assert(workspace.layers().front().entities.size() == 1);
}

void test_node_graph_connects_raw_data_matrix_and_output() {
    thrystr::model::NodeGraph graph;
    const thrystr::model::NodeId raw =
        graph.create_node("raw", "Raw Data", thrystr::model::EntityId{1}).id;
    const thrystr::model::NodeId matrix =
        graph.create_node("matrix", "Matrix", thrystr::model::EntityId{2}).id;
    const thrystr::model::NodeId output = graph.create_node("output", "Output").id;

    const thrystr::model::PinId raw_out =
        graph.add_pin(raw, "data", "Data", thrystr::model::PinDirection::Output).id;
    const thrystr::model::PinId matrix_in =
        graph.add_pin(matrix, "input", "Input", thrystr::model::PinDirection::Input).id;
    const thrystr::model::PinId matrix_out =
        graph.add_pin(matrix, "output", "Output", thrystr::model::PinDirection::Output).id;
    const thrystr::model::PinId output_in =
        graph.add_pin(output, "display", "Display", thrystr::model::PinDirection::Input).id;

    graph.connect(raw, raw_out, matrix, matrix_in);
    graph.connect(matrix, matrix_out, output, output_in);
    graph.set_output_node(output);

    assert(graph.nodes().size() == 3);
    assert(graph.edges().size() == 2);
    assert(graph.output_node() == output);
}

void test_window_components_sync_against_application_overlay() {
    thrystr::model::Workspace workspace;
    thrystr::ui::Window window("workspace", "Workspace", {10.0f, 20.0f, 640.0f, 480.0f});
    SyncCountingComponent& counter = window.add_component<SyncCountingComponent>();

    assert(window.state().label == "Workspace");
    assert(window.find_component("counter") == &counter);
    window.sync({&workspace, nullptr});
    assert(counter.sync_count == 1);

    counter.set_visible(false);
    window.sync({&workspace, nullptr});
    assert(counter.sync_count == 1);
}

} // namespace

int main() {
    test_property_bag_lookup_and_order();
    test_function_entity_plots_standard_math();
    test_workspace_layers_collections_and_entity_subclasses();
    test_fitter_registry_accepts_dataset_sections_and_creates_layers();
    test_node_graph_connects_raw_data_matrix_and_output();
    test_window_components_sync_against_application_overlay();
    return 0;
}
