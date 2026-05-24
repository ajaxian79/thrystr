// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/app/workspace.hpp>

#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace thrystr::app {

/// Deterministic controls for section fitting.
struct ConvergentFitOptions {
    double tolerance = kDefaultSectionTolerance;
    double default_spacing_nm = kDefaultSectionSpacingNm;
    std::size_t max_section_length = 4096u;
    int wavelength_steps = 48;
    int phase_steps = 48;
    const std::atomic<bool>* cancel_requested = nullptr;
};

/// Aggregate result of a deterministic section fit.
struct ConvergentFitResult {
    std::vector<Section> sections;
    bool cancelled = false;
    double max_residual = 0.0;
    double mean_residual = 0.0;
};

/// Fit one section over `[start, start + length)`.
Section fit_section(std::span<const float> scalars,
                    std::size_t start,
                    std::size_t length,
                    const ConvergentFitOptions& options = {});

/// Greedily tile the scalar series with fitted sections.
ConvergentFitResult fit_convergent_sections(
    std::span<const float> scalars,
    const ConvergentFitOptions& options = {});

}  // namespace thrystr::app
