// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace thrystr::model {

class Dataset {
  public:
    virtual ~Dataset() = default;

    virtual std::size_t count() const noexcept = 0;
    virtual double spatial_period_nm() const noexcept = 0;
    virtual std::optional<double> scalar_at(std::size_t index) const = 0;
};

class VectorDataset final : public Dataset {
  public:
    explicit VectorDataset(std::vector<double> scalars, double spatial_period_nm = 1.0);

    std::size_t count() const noexcept override;
    double spatial_period_nm() const noexcept override;
    std::optional<double> scalar_at(std::size_t index) const override;

    std::span<const double> values() const noexcept;

  private:
    std::vector<double> scalars_;
    double spatial_period_nm_ = 1.0;
};

struct SegmentRange {
    std::size_t start = 0;
    std::size_t length = 0;

    std::size_t end_exclusive() const noexcept;
    bool contains(std::size_t index) const noexcept;
};

} // namespace thrystr::model
