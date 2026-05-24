// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/model/dataset.hpp>
#include <thrystr/model/workspace.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace thrystr::model {

struct FitRequest {
    const Dataset* dataset = nullptr;
    Workspace* workspace = nullptr;
    std::optional<SegmentRange> segment;
    LayerId target_layer_id = 0;
};

struct FitResult {
    bool success = false;
    std::string message;
    std::vector<EntityId> created_entities;
    std::vector<LayerId> created_layers;
};

class Fitter {
  public:
    virtual ~Fitter() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual std::string_view label() const noexcept = 0;
    virtual FitResult fit(const FitRequest& request) = 0;
};

class FitterRegistry {
  public:
    void register_fitter(std::unique_ptr<Fitter> fitter);
    Fitter* find(std::string_view name) noexcept;
    const Fitter* find(std::string_view name) const noexcept;
    std::vector<std::string_view> names() const;

  private:
    std::unordered_map<std::string, std::unique_ptr<Fitter>> fitters_;
};

} // namespace thrystr::model
