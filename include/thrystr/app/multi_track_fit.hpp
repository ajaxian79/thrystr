// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <thrystr/app/convergent_fit.hpp>
#include <thrystr/app/workspace.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace thrystr::app {

/// Controls for deterministic multi-track fitting.
struct MultiTrackOptions {
    double tolerance = kDefaultSectionTolerance;
    double default_spacing_nm = kDefaultSectionSpacingNm;
    double parity_margin = 0.4;
    std::size_t max_section_length = 4096u;
    std::uint8_t min_data_tracks = 2u;
    std::uint8_t max_data_tracks = 8u;
    bool allow_single_track_fallback = true;
    const std::atomic<bool>* cancel_requested = nullptr;
};

/// Summary of a fitted single-track or multi-track workspace.
struct MultiTrackFitResult {
    WorkspaceModel workspace;
    bool used_parity = false;
    bool cancelled = false;
    std::size_t single_track_section_count = 0;
    std::size_t total_section_count = 0;
};

/// Decode the data-track id selected by the parity track at `index`.
std::uint8_t parity_owner_at(const WorkspaceModel& workspace, std::size_t index);

/// Reconstruct one sample from the fitted workspace.
double reconstructed_value_at(const WorkspaceModel& workspace, std::size_t index);

/// Fit a workspace and choose multi-track only when it beats single-track cost.
MultiTrackFitResult fit_multi_track_sections(
    std::span<const float> scalars,
    const MultiTrackOptions& options = {});

}  // namespace thrystr::app
