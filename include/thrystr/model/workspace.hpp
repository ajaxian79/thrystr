// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/model/entity.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thrystr::model {

using LayerId = std::uint64_t;
using CollectionId = std::uint64_t;

struct Layer {
    LayerId id = 0;
    std::string name;
    std::string label;
    bool visible = true;
    std::vector<EntityId> entities;
};

struct Collection {
    CollectionId id = 0;
    std::string name;
    std::string label;
    bool visible = true;
    std::vector<EntityId> entities;
    std::vector<LayerId> layers;
    std::vector<CollectionId> collections;
};

class Workspace {
  public:
    template <typename EntityT, typename... Args> EntityT& create_entity(Args&&... args) {
        auto entity = std::make_unique<EntityT>(std::forward<Args>(args)...);
        EntityT& entity_ref = *entity;
        add_entity(std::move(entity));
        return entity_ref;
    }

    Entity& add_entity(std::unique_ptr<Entity> entity);
    Entity* find_entity(EntityId id) noexcept;
    const Entity* find_entity(EntityId id) const noexcept;

    Layer& create_layer(std::string name, std::string label = {});
    Layer* find_layer(LayerId id) noexcept;
    const Layer* find_layer(LayerId id) const noexcept;

    Collection& create_collection(std::string name, std::string label = {});
    Collection* find_collection(CollectionId id) noexcept;
    const Collection* find_collection(CollectionId id) const noexcept;

    void attach_entity_to_layer(EntityId entity_id, LayerId layer_id);
    void attach_entity_to_collection(EntityId entity_id, CollectionId collection_id);
    void attach_layer_to_collection(LayerId layer_id, CollectionId collection_id);
    void attach_collection_to_collection(CollectionId child_id, CollectionId parent_id);

    std::span<EntityId const> entity_order() const noexcept;
    std::span<Layer const> layers() const noexcept;
    std::span<Collection const> collections() const noexcept;

  private:
    EntityId next_entity_id_ = 1;
    LayerId next_layer_id_ = 1;
    CollectionId next_collection_id_ = 1;
    std::unordered_map<EntityId, std::unique_ptr<Entity>> entities_;
    std::vector<EntityId> entity_order_;
    std::vector<Layer> layers_;
    std::vector<Collection> collections_;
};

} // namespace thrystr::model
