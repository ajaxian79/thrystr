// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/workspace.hpp>

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

template <typename IdT> void append_unique(std::vector<IdT>& values, IdT id) {
    if (std::find(values.begin(), values.end(), id) == values.end()) {
        values.push_back(id);
    }
}

} // namespace

Entity& Workspace::add_entity(std::unique_ptr<Entity> entity) {
    if (!entity) {
        throw std::invalid_argument("entity must not be null");
    }
    const EntityId id = next_entity_id_++;
    entity->set_id(id);
    Entity& entity_ref = *entity;
    entities_.emplace(id, std::move(entity));
    entity_order_.push_back(id);
    return entity_ref;
}

Entity* Workspace::find_entity(EntityId id) noexcept {
    const auto found = entities_.find(id);
    return found == entities_.end() ? nullptr : found->second.get();
}

const Entity* Workspace::find_entity(EntityId id) const noexcept {
    const auto found = entities_.find(id);
    return found == entities_.end() ? nullptr : found->second.get();
}

Layer& Workspace::create_layer(std::string name, std::string label) {
    Layer layer;
    layer.id = next_layer_id_++;
    layer.name = std::move(name);
    layer.label = label.empty() ? layer.name : std::move(label);
    layers_.push_back(std::move(layer));
    return layers_.back();
}

Layer* Workspace::find_layer(LayerId id) noexcept { return find_by_id(layers_, id); }

const Layer* Workspace::find_layer(LayerId id) const noexcept { return find_by_id(layers_, id); }

Collection& Workspace::create_collection(std::string name, std::string label) {
    Collection collection;
    collection.id = next_collection_id_++;
    collection.name = std::move(name);
    collection.label = label.empty() ? collection.name : std::move(label);
    collections_.push_back(std::move(collection));
    return collections_.back();
}

Collection* Workspace::find_collection(CollectionId id) noexcept {
    return find_by_id(collections_, id);
}

const Collection* Workspace::find_collection(CollectionId id) const noexcept {
    return find_by_id(collections_, id);
}

void Workspace::attach_entity_to_layer(EntityId entity_id, LayerId layer_id) {
    if (!find_entity(entity_id)) {
        throw std::out_of_range("unknown entity id");
    }
    Layer* layer = find_layer(layer_id);
    if (!layer) {
        throw std::out_of_range("unknown layer id");
    }
    append_unique(layer->entities, entity_id);
}

void Workspace::attach_entity_to_collection(EntityId entity_id, CollectionId collection_id) {
    if (!find_entity(entity_id)) {
        throw std::out_of_range("unknown entity id");
    }
    Collection* collection = find_collection(collection_id);
    if (!collection) {
        throw std::out_of_range("unknown collection id");
    }
    append_unique(collection->entities, entity_id);
}

void Workspace::attach_layer_to_collection(LayerId layer_id, CollectionId collection_id) {
    if (!find_layer(layer_id)) {
        throw std::out_of_range("unknown layer id");
    }
    Collection* collection = find_collection(collection_id);
    if (!collection) {
        throw std::out_of_range("unknown collection id");
    }
    append_unique(collection->layers, layer_id);
}

void Workspace::attach_collection_to_collection(CollectionId child_id, CollectionId parent_id) {
    if (child_id == parent_id) {
        throw std::invalid_argument("collection cannot contain itself");
    }
    if (!find_collection(child_id)) {
        throw std::out_of_range("unknown child collection id");
    }
    Collection* parent = find_collection(parent_id);
    if (!parent) {
        throw std::out_of_range("unknown parent collection id");
    }
    append_unique(parent->collections, child_id);
}

std::span<EntityId const> Workspace::entity_order() const noexcept { return entity_order_; }

std::span<Layer const> Workspace::layers() const noexcept { return layers_; }

std::span<Collection const> Workspace::collections() const noexcept { return collections_; }

} // namespace thrystr::model
