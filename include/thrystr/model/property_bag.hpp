// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace thrystr::model {

using PropertyValue =
    std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;

struct PropertyDefinition {
    std::string name;
    std::string label;
    PropertyValue value;
};

class PropertyBag {
  public:
    PropertyDefinition& register_property(std::string name, std::string label,
                                          PropertyValue default_value);

    bool contains(std::string_view name) const;
    std::size_t size() const noexcept;

    const PropertyDefinition* definition(std::string_view name) const;
    PropertyDefinition* definition(std::string_view name);

    const PropertyValue* value(std::string_view name) const;
    PropertyValue* value(std::string_view name);
    void set(std::string_view name, PropertyValue value);

    std::optional<double> double_value(std::string_view name) const;
    std::optional<std::int64_t> int_value(std::string_view name) const;
    std::optional<bool> bool_value(std::string_view name) const;
    std::optional<std::string_view> string_value(std::string_view name) const;

    std::span<const PropertyDefinition> ordered() const noexcept;
    std::span<PropertyDefinition> ordered() noexcept;

  private:
    std::vector<PropertyDefinition> ordered_;
    std::unordered_map<std::string, std::size_t> by_name_;
};

} // namespace thrystr::model
