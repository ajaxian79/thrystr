// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/dataset.hpp>

#include <algorithm>

namespace thrystr::model {

VectorDataset::VectorDataset(std::vector<double> scalars, double spatial_period_nm)
    : scalars_(std::move(scalars)), spatial_period_nm_(std::max(0.000001, spatial_period_nm)) {}

std::size_t VectorDataset::count() const noexcept { return scalars_.size(); }

double VectorDataset::spatial_period_nm() const noexcept { return spatial_period_nm_; }

std::optional<double> VectorDataset::scalar_at(std::size_t index) const {
    if (index >= scalars_.size()) {
        return std::nullopt;
    }
    return scalars_[index];
}

std::span<const double> VectorDataset::values() const noexcept { return scalars_; }

std::size_t SegmentRange::end_exclusive() const noexcept { return start + length; }

bool SegmentRange::contains(std::size_t index) const noexcept {
    return index >= start && index < end_exclusive();
}

} // namespace thrystr::model
