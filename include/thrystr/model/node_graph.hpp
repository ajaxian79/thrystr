// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/model/entity.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace thrystr::model {

using NodeId = std::uint64_t;
using PinId = std::uint64_t;

enum class PinDirection {
    Input,
    Output,
};

struct NodePin {
    PinId id = 0;
    std::string name;
    std::string label;
    PinDirection direction = PinDirection::Input;
};

struct Node {
    NodeId id = 0;
    std::string name;
    std::string label;
    std::optional<EntityId> entity_id;
    std::vector<NodePin> pins;
};

struct NodeEdge {
    NodeId from_node = 0;
    PinId from_pin = 0;
    NodeId to_node = 0;
    PinId to_pin = 0;
};

class NodeGraph {
  public:
    Node& create_node(std::string name, std::string label = {},
                      std::optional<EntityId> entity_id = std::nullopt);
    NodePin& add_pin(NodeId node_id, std::string name, std::string label, PinDirection direction);
    void connect(NodeId from_node, PinId from_pin, NodeId to_node, PinId to_pin);

    Node* find_node(NodeId id) noexcept;
    const Node* find_node(NodeId id) const noexcept;
    NodePin* find_pin(NodeId node_id, PinId pin_id) noexcept;
    const NodePin* find_pin(NodeId node_id, PinId pin_id) const noexcept;

    void set_output_node(NodeId id);
    NodeId output_node() const noexcept;

    std::span<const Node> nodes() const noexcept;
    std::span<const NodeEdge> edges() const noexcept;

  private:
    NodeId next_node_id_ = 1;
    PinId next_pin_id_ = 1;
    NodeId output_node_id_ = 0;
    std::vector<Node> nodes_;
    std::vector<NodeEdge> edges_;
};

} // namespace thrystr::model
