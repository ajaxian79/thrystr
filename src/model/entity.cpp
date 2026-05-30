// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/entity.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace thrystr::model {
namespace {

constexpr double kDefaultWavelengthNm = 64.0;
constexpr double kDefaultAmplitude = 2.0;
constexpr double kDefaultAmplitudeOffset = -1.0;

double property_or(const PropertyBag& properties, std::string_view name, double fallback) {
    return properties.double_value(name).value_or(fallback);
}

std::int64_t property_or(const PropertyBag& properties, std::string_view name,
                         std::int64_t fallback) {
    return properties.int_value(name).value_or(fallback);
}

bool property_or(const PropertyBag& properties, std::string_view name, bool fallback) {
    return properties.bool_value(name).value_or(fallback);
}

double function_basis(const PropertyBag& properties, StandardFunction function, double x) {
    const double wavelength_nm =
        std::max(0.000001, property_or(properties, "wavelength_nm", kDefaultWavelengthNm));
    const double phase_nm = property_or(properties, "phase_nm", 0.0);
    const double rotation_degrees = property_or(properties, "rotation_degrees", 0.0);
    const double unit_x = (x - phase_nm) / wavelength_nm;
    const double raw_y = rotated_standard_function_value(function, unit_x, rotation_degrees);
    return (raw_y + 1.0) * 0.5;
}

} // namespace

Entity::Entity(std::string name, std::string label)
    : name_(std::move(name)), label_(std::move(label)) {}

EntityId Entity::id() const noexcept { return id_; }

void Entity::set_id(EntityId id) noexcept { id_ = id; }

const std::string& Entity::name() const noexcept { return name_; }

void Entity::set_name(std::string name) { name_ = std::move(name); }

const std::string& Entity::label() const noexcept { return label_; }

void Entity::set_label(std::string label) { label_ = std::move(label); }

bool Entity::visible() const noexcept { return visible_; }

void Entity::set_visible(bool visible) noexcept { visible_ = visible; }

PropertyBag& Entity::properties() noexcept { return properties_; }

const PropertyBag& Entity::properties() const noexcept { return properties_; }

std::optional<double> Entity::plot(double) const { return std::nullopt; }

std::vector<double> Entity::plot_row(double) const { return {}; }

FunctionEntity::FunctionEntity(std::string name, std::string label)
    : Entity(std::move(name), std::move(label)) {
    properties_.register_property("function", "Function",
                                  static_cast<std::int64_t>(StandardFunction::Sine));
    properties_.register_property("secondary_enabled", "Secondary Function", false);
    properties_.register_property("secondary_function", "Secondary",
                                  static_cast<std::int64_t>(StandardFunction::Cosine));
    properties_.register_property("wavelength_nm", "Wavelength nm", kDefaultWavelengthNm);
    properties_.register_property("amplitude", "Amplitude", kDefaultAmplitude);
    properties_.register_property("secondary_amplitude", "Secondary Amplitude", 0.0);
    properties_.register_property("amplitude_offset", "Amplitude Offset", kDefaultAmplitudeOffset);
    properties_.register_property("phase_nm", "Phase nm", 0.0);
    properties_.register_property("rotation_degrees", "Z Rotation", 0.0);
}

EntityKind FunctionEntity::kind() const noexcept { return EntityKind::Function; }

std::optional<double> FunctionEntity::plot(double x) const {
    const auto function = normalize_standard_function(static_cast<std::uint32_t>(
        property_or(properties_, "function", static_cast<std::int64_t>(StandardFunction::Sine))));
    double value = property_or(properties_, "amplitude_offset", kDefaultAmplitudeOffset) +
                   property_or(properties_, "amplitude", kDefaultAmplitude) *
                       function_basis(properties_, function, x);

    if (property_or(properties_, "secondary_enabled", false)) {
        const auto secondary = normalize_standard_function(static_cast<std::uint32_t>(
            property_or(properties_, "secondary_function",
                        static_cast<std::int64_t>(StandardFunction::Cosine))));
        value += property_or(properties_, "secondary_amplitude", 0.0) *
                 function_basis(properties_, secondary, x);
    }

    if (!std::isfinite(value)) {
        return value < 0.0 ? -1.0e6 : 1.0e6;
    }
    return std::clamp(value, -1.0e6, 1.0e6);
}

RawDataEntity::RawDataEntity(std::shared_ptr<const Dataset> dataset, std::string name,
                             std::string label)
    : Entity(std::move(name), std::move(label)), dataset_(std::move(dataset)) {
    properties_.register_property("spatial_period_nm", "Spatial Period nm",
                                  dataset_ ? dataset_->spatial_period_nm() : 1.0);
}

EntityKind RawDataEntity::kind() const noexcept { return EntityKind::RawData; }

const Dataset* RawDataEntity::dataset() const noexcept { return dataset_.get(); }

MatrixEntity::MatrixEntity(std::string name, std::string label)
    : Entity(std::move(name), std::move(label)) {
    properties_.register_property("columns", "Columns", static_cast<std::int64_t>(1));
}

EntityKind MatrixEntity::kind() const noexcept { return EntityKind::Matrix; }

std::vector<double> MatrixEntity::plot_row(double x) const {
    if (plotter_) {
        return plotter_(x, properties_);
    }

    const std::size_t columns = static_cast<std::size_t>(
        std::max<std::int64_t>(1, property_or(properties_, "columns", std::int64_t{1})));
    return std::vector<double>(columns, x);
}

void MatrixEntity::set_plotter(MatrixPlotter plotter) { plotter_ = std::move(plotter); }

RefinedDataEntity::RefinedDataEntity(EntityId raw_data_entity_id, EntityId matrix_entity_id,
                                     std::string name, std::string label)
    : Entity(std::move(name), std::move(label)), raw_data_entity_id_(raw_data_entity_id),
      matrix_entity_id_(matrix_entity_id) {
    properties_.register_property("raw_data_entity_id", "Raw Data Entity",
                                  static_cast<std::uint64_t>(raw_data_entity_id_));
    properties_.register_property("matrix_entity_id", "Matrix Entity",
                                  static_cast<std::uint64_t>(matrix_entity_id_));
}

EntityKind RefinedDataEntity::kind() const noexcept { return EntityKind::RefinedData; }

EntityId RefinedDataEntity::raw_data_entity_id() const noexcept { return raw_data_entity_id_; }

EntityId RefinedDataEntity::matrix_entity_id() const noexcept { return matrix_entity_id_; }

} // namespace thrystr::model
