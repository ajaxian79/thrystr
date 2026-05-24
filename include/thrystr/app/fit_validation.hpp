// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/app/workspace.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace thrystr::app {

/// One validation issue tied to a section or track.
struct ValidationIssue {
    std::string message;
    std::size_t track_index = 0;
    std::size_t section_index = 0;
    std::size_t sample_index = 0;
};

/// Aggregate coverage and residual information for a fit.
struct ValidationReport {
    bool pass = true;
    double max_residual = 0.0;
    double mean_residual = 0.0;
    std::size_t checked_samples = 0;
    std::vector<ValidationIssue> issues;
};

/// Validate a flat section table against a scalar data series.
ValidationReport validate_sections(std::span<const Scalar> scalars,
                                   std::span<const Section> sections);

/// Validate single-track or multi-track workspace coverage.
ValidationReport validate_tracks(std::span<const Scalar> scalars, const WorkspaceModel& workspace,
                                 double parity_margin = 0.4);

} // namespace thrystr::app
