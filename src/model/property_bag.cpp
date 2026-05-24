// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/property_bag.hpp>

#include <stdexcept>

namespace thrystr::model {
namespace {

std::string key_for(std::string_view name) { return std::string(name); }

} // namespace

PropertyDefinition& PropertyBag::register_property(std::string name, std::string label,
                                                   PropertyValue default_value) {
    if (name.empty()) {
        throw std::invalid_argument("property name must not be empty");
    }
    if (by_name_.contains(name)) {
        throw std::invalid_argument("duplicate property: " + name);
    }

    const std::size_t index = ordered_.size();
    ordered_.push_back({std::move(name), std::move(label), std::move(default_value)});
    by_name_.emplace(ordered_.back().name, index);
    return ordered_.back();
}

bool PropertyBag::contains(std::string_view name) const {
    return by_name_.find(key_for(name)) != by_name_.end();
}

std::size_t PropertyBag::size() const noexcept { return ordered_.size(); }

const PropertyDefinition* PropertyBag::definition(std::string_view name) const {
    const auto found = by_name_.find(key_for(name));
    return found == by_name_.end() ? nullptr : &ordered_[found->second];
}

PropertyDefinition* PropertyBag::definition(std::string_view name) {
    const auto found = by_name_.find(key_for(name));
    return found == by_name_.end() ? nullptr : &ordered_[found->second];
}

const PropertyValue* PropertyBag::value(std::string_view name) const {
    const PropertyDefinition* property = definition(name);
    return property ? &property->value : nullptr;
}

PropertyValue* PropertyBag::value(std::string_view name) {
    PropertyDefinition* property = definition(name);
    return property ? &property->value : nullptr;
}

void PropertyBag::set(std::string_view name, PropertyValue value) {
    PropertyDefinition* property = definition(name);
    if (!property) {
        throw std::out_of_range("unknown property: " + std::string(name));
    }
    property->value = std::move(value);
}

std::optional<double> PropertyBag::double_value(std::string_view name) const {
    const PropertyValue* property_value = value(name);
    if (!property_value) {
        return std::nullopt;
    }
    if (const auto* value_double = std::get_if<double>(property_value)) {
        return *value_double;
    }
    if (const auto* value_int = std::get_if<std::int64_t>(property_value)) {
        return static_cast<double>(*value_int);
    }
    if (const auto* value_uint = std::get_if<std::uint64_t>(property_value)) {
        return static_cast<double>(*value_uint);
    }
    return std::nullopt;
}

std::optional<std::int64_t> PropertyBag::int_value(std::string_view name) const {
    const PropertyValue* property_value = value(name);
    if (!property_value) {
        return std::nullopt;
    }
    if (const auto* value_int = std::get_if<std::int64_t>(property_value)) {
        return *value_int;
    }
    if (const auto* value_uint = std::get_if<std::uint64_t>(property_value)) {
        return static_cast<std::int64_t>(*value_uint);
    }
    return std::nullopt;
}

std::optional<bool> PropertyBag::bool_value(std::string_view name) const {
    const PropertyValue* property_value = value(name);
    if (!property_value) {
        return std::nullopt;
    }
    if (const auto* value_bool = std::get_if<bool>(property_value)) {
        return *value_bool;
    }
    return std::nullopt;
}

std::optional<std::string_view> PropertyBag::string_value(std::string_view name) const {
    const PropertyValue* property_value = value(name);
    if (!property_value) {
        return std::nullopt;
    }
    if (const auto* value_string = std::get_if<std::string>(property_value)) {
        return std::string_view(*value_string);
    }
    return std::nullopt;
}

std::span<const PropertyDefinition> PropertyBag::ordered() const noexcept { return ordered_; }

std::span<PropertyDefinition> PropertyBag::ordered() noexcept { return ordered_; }

} // namespace thrystr::model
