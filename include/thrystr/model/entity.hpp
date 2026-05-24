// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/model/dataset.hpp>
#include <thrystr/model/property_bag.hpp>
#include <thrystr/model/standard_math.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace thrystr::model {

using EntityId = std::uint64_t;

enum class EntityKind {
    Function,
    RawData,
    Matrix,
    RefinedData,
};

class Entity {
  public:
    Entity(std::string name, std::string label);
    virtual ~Entity() = default;

    EntityId id() const noexcept;
    void set_id(EntityId id) noexcept;

    const std::string& name() const noexcept;
    void set_name(std::string name);

    const std::string& label() const noexcept;
    void set_label(std::string label);

    bool visible() const noexcept;
    void set_visible(bool visible) noexcept;

    PropertyBag& properties() noexcept;
    const PropertyBag& properties() const noexcept;

    virtual EntityKind kind() const noexcept = 0;
    virtual std::optional<double> plot(double x) const;
    virtual std::vector<double> plot_row(double x) const;

  private:
    EntityId id_ = 0;
    std::string name_;
    std::string label_;
    bool visible_ = true;

  protected:
    PropertyBag properties_;
};

class FunctionEntity final : public Entity {
  public:
    explicit FunctionEntity(std::string name = "function", std::string label = "Function");

    EntityKind kind() const noexcept override;
    std::optional<double> plot(double x) const override;
};

class RawDataEntity final : public Entity {
  public:
    RawDataEntity(std::shared_ptr<const Dataset> dataset, std::string name = "raw data",
                  std::string label = "Raw Data");

    EntityKind kind() const noexcept override;
    const Dataset* dataset() const noexcept;

  private:
    std::shared_ptr<const Dataset> dataset_;
};

using MatrixPlotter = std::function<std::vector<double>(double x, const PropertyBag& properties)>;

class MatrixEntity final : public Entity {
  public:
    explicit MatrixEntity(std::string name = "matrix", std::string label = "Matrix");

    EntityKind kind() const noexcept override;
    std::vector<double> plot_row(double x) const override;
    void set_plotter(MatrixPlotter plotter);

  private:
    MatrixPlotter plotter_;
};

class RefinedDataEntity final : public Entity {
  public:
    RefinedDataEntity(EntityId raw_data_entity_id, EntityId matrix_entity_id,
                      std::string name = "refined data", std::string label = "Refined Data");

    EntityKind kind() const noexcept override;
    EntityId raw_data_entity_id() const noexcept;
    EntityId matrix_entity_id() const noexcept;

  private:
    EntityId raw_data_entity_id_ = 0;
    EntityId matrix_entity_id_ = 0;
};

} // namespace thrystr::model
