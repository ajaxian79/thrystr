// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/node_graph.hpp>

#include <algorithm>
#include <stdexcept>

namespace thrystr::model {
namespace {

template <typename T, typename IdT> T* find_by_id(std::vector<T>& values, IdT id) {
    const auto found =
        std::find_if(values.begin(), values.end(), [id](const T& value) { return value.id == id; });
    return found == values.end() ? nullptr : &*found;
}

template <typename T, typename IdT> const T* find_by_id(const std::vector<T>& values, IdT id) {
    const auto found =
        std::find_if(values.begin(), values.end(), [id](const T& value) { return value.id == id; });
    return found == values.end() ? nullptr : &*found;
}

} // namespace

Node& NodeGraph::create_node(std::string name, std::string label,
                             std::optional<EntityId> entity_id) {
    if (name.empty()) {
        throw std::invalid_argument("node name must not be empty");
    }
    Node node;
    node.id = next_node_id_++;
    node.name = std::move(name);
    node.label = label.empty() ? node.name : std::move(label);
    node.entity_id = entity_id;
    nodes_.push_back(std::move(node));
    return nodes_.back();
}

NodePin& NodeGraph::add_pin(NodeId node_id, std::string name, std::string label,
                            PinDirection direction) {
    Node* node = find_node(node_id);
    if (!node) {
        throw std::out_of_range("unknown node id");
    }
    if (name.empty()) {
        throw std::invalid_argument("pin name must not be empty");
    }

    NodePin pin;
    pin.id = next_pin_id_++;
    pin.name = std::move(name);
    pin.label = label.empty() ? pin.name : std::move(label);
    pin.direction = direction;
    node->pins.push_back(std::move(pin));
    return node->pins.back();
}

void NodeGraph::connect(NodeId from_node, PinId from_pin, NodeId to_node, PinId to_pin) {
    const NodePin* source = find_pin(from_node, from_pin);
    const NodePin* target = find_pin(to_node, to_pin);
    if (!source || !target) {
        throw std::out_of_range("unknown node pin");
    }
    if (source->direction != PinDirection::Output || target->direction != PinDirection::Input) {
        throw std::invalid_argument("node edges must connect output pins to input pins");
    }
    edges_.push_back({from_node, from_pin, to_node, to_pin});
}

Node* NodeGraph::find_node(NodeId id) noexcept { return find_by_id(nodes_, id); }

const Node* NodeGraph::find_node(NodeId id) const noexcept { return find_by_id(nodes_, id); }

NodePin* NodeGraph::find_pin(NodeId node_id, PinId pin_id) noexcept {
    Node* node = find_node(node_id);
    return node ? find_by_id(node->pins, pin_id) : nullptr;
}

const NodePin* NodeGraph::find_pin(NodeId node_id, PinId pin_id) const noexcept {
    const Node* node = find_node(node_id);
    return node ? find_by_id(node->pins, pin_id) : nullptr;
}

void NodeGraph::set_output_node(NodeId id) {
    if (!find_node(id)) {
        throw std::out_of_range("unknown node id");
    }
    output_node_id_ = id;
}

NodeId NodeGraph::output_node() const noexcept { return output_node_id_; }

std::span<const Node> NodeGraph::nodes() const noexcept { return nodes_; }

std::span<const NodeEdge> NodeGraph::edges() const noexcept { return edges_; }

} // namespace thrystr::model
