// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/model/fitter.hpp>

#include <stdexcept>

namespace thrystr::model {

void FitterRegistry::register_fitter(std::unique_ptr<Fitter> fitter) {
    if (!fitter) {
        throw std::invalid_argument("fitter must not be null");
    }
    const std::string name(fitter->name());
    if (name.empty()) {
        throw std::invalid_argument("fitter name must not be empty");
    }
    if (fitters_.contains(name)) {
        throw std::invalid_argument("duplicate fitter: " + name);
    }
    fitters_.emplace(name, std::move(fitter));
}

Fitter* FitterRegistry::find(std::string_view name) noexcept {
    const auto found = fitters_.find(std::string(name));
    return found == fitters_.end() ? nullptr : found->second.get();
}

const Fitter* FitterRegistry::find(std::string_view name) const noexcept {
    const auto found = fitters_.find(std::string(name));
    return found == fitters_.end() ? nullptr : found->second.get();
}

std::vector<std::string_view> FitterRegistry::names() const {
    std::vector<std::string_view> result;
    result.reserve(fitters_.size());
    for (const auto& [name, fitter] : fitters_) {
        (void)fitter;
        result.push_back(name);
    }
    return result;
}

} // namespace thrystr::model
