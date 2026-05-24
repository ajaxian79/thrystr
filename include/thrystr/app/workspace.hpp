// SPDX-License-Identifier: LicenseRef-thrystr-dual
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace thrystr::app {

constexpr std::uint8_t kNoParityTrack = 0xffu;
constexpr std::size_t kMaxTracks = 17u;
constexpr std::size_t kMaxDataTracks = 16u;
constexpr std::size_t kMinSectionSize = 2u;
constexpr double kDefaultSectionSpacingNm = 1.0;
constexpr double kDefaultSectionTolerance = 1.0 / 128.0;

/// Role played by a track in the encoded workspace.
enum class TrackKind : std::uint8_t {
    Data = 0,
    Parity = 1,
};

/// One contiguous source-index span and the sinusoid fitted to it.
struct Section {
    std::uint32_t start_index = 0;
    std::uint32_t length = 0;
    double section_spacing_nm = kDefaultSectionSpacingNm;
    double wave_wavelength_nm = 2.0;
    double wave_amplitude = 0.0;
    double wave_amplitude_offset = 0.0;
    double wave_phase_nm = 0.0;
    double fit_tolerance = kDefaultSectionTolerance;
    double max_residual = 0.0;
    double mean_residual = 0.0;
};

/// A sparse data track or the parity track that routes samples to tracks.
struct Track {
    std::uint8_t id = 0;
    TrackKind kind = TrackKind::Data;
    std::string name;
    bool visible = true;
    std::vector<Section> sections;
    std::vector<std::uint8_t> owned_mask;
};

/// Mutable app-level representation of section and track coverage.
struct WorkspaceModel {
    std::vector<Track> tracks;
    std::uint8_t active_track_id = 0;
    std::uint8_t parity_track_id = kNoParityTrack;
};

/// Return the inclusive-exclusive end index of a section.
std::uint64_t section_end(const Section& section);

/// Return true when `index` is inside the section span.
bool section_contains(const Section& section, std::size_t index);

/// Evaluate a section wave at a source index.
double wave_value_at_index(const Section& section, std::size_t index);

/// Return the number of bytes required to store one ownership bit per sample.
std::size_t owned_mask_size(std::size_t sample_count);

/// Resize `mask` for `sample_count` bits and clear all ownership.
void reset_owned_mask(std::vector<std::uint8_t>& mask, std::size_t sample_count);

/// Read one ownership bit. Out-of-range bits are false.
bool get_owned_bit(std::span<const std::uint8_t> mask, std::size_t index);

/// Set or clear one ownership bit, growing is not allowed.
void set_owned_bit(std::span<std::uint8_t> mask, std::size_t index, bool value);

/// Count the set ownership bits in a packed mask.
std::size_t popcount_owned(std::span<const std::uint8_t> mask);

/// Return the first set bit, or `sample_count` when the mask is empty.
std::size_t first_owned_index(std::span<const std::uint8_t> mask,
                              std::size_t sample_count);

/// Return one-past the last set bit, or zero when the mask is empty.
std::size_t last_owned_index_exclusive(std::span<const std::uint8_t> mask,
                                       std::size_t sample_count);

/// Create a single data track that owns every sample.
Track make_full_data_track(std::uint8_t id, std::size_t sample_count);

/// Append a section to a track.
Section& append_section(Track& track, const Section& section);

/// Find a track by stable id.
Track* find_track(WorkspaceModel& workspace, std::uint8_t id);

/// Find a track by stable id.
const Track* find_track(const WorkspaceModel& workspace, std::uint8_t id);

}  // namespace thrystr::app
