// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/multi_track_fit.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace thrystr::app {
namespace {

constexpr std::size_t kTrackMetadataSectionCost = 1u;

bool cancelled(const MultiTrackOptions& options) {
    return options.cancel_requested && options.cancel_requested->load();
}

std::uint8_t scalar_to_byte(float value) {
    const double scaled = (static_cast<double>(value) + 1.0) * 128.0;
    return static_cast<std::uint8_t>(
        std::clamp(std::llround(scaled), 0ll, 255ll));
}

std::size_t count_track_sections(const WorkspaceModel& workspace) {
    std::size_t count = 0;
    for (const Track& track : workspace.tracks) {
        count += track.sections.size();
    }
    return count;
}

Section constant_section(std::size_t start,
                         std::size_t length,
                         double value,
                         const MultiTrackOptions& options) {
    Section section;
    section.start_index = static_cast<std::uint32_t>(start);
    section.length = static_cast<std::uint32_t>(length);
    section.section_spacing_nm = std::max(1.0e-12, options.default_spacing_nm);
    section.wave_wavelength_nm = section.section_spacing_nm * 2.0;
    section.wave_amplitude = 0.0;
    section.wave_amplitude_offset = value;
    section.fit_tolerance = options.tolerance;
    return section;
}

Section two_owned_section(std::span<const float> scalars,
                          std::size_t first,
                          std::size_t second,
                          const MultiTrackOptions& options) {
    const double first_value = static_cast<double>(scalars[first]);
    const double second_value = static_cast<double>(scalars[second]);
    const double low = std::min(first_value, second_value);
    const double high = std::max(first_value, second_value);
    const double spacing = std::max(1.0e-12, options.default_spacing_nm);
    const double distance = std::max(1.0, static_cast<double>(second - first)) * spacing;

    Section section;
    section.start_index = static_cast<std::uint32_t>(first);
    section.length = static_cast<std::uint32_t>(second - first + 1u);
    section.section_spacing_nm = spacing;
    section.wave_wavelength_nm = distance * 2.0;
    section.wave_amplitude = high - low;
    section.wave_amplitude_offset = low;
    section.wave_phase_nm = first_value <= second_value
        ? section.wave_wavelength_nm * 0.25
        : -section.wave_wavelength_nm * 0.25;
    section.fit_tolerance = options.tolerance;
    section.max_residual = 0.0;
    section.mean_residual = 0.0;
    return section;
}

std::vector<std::uint8_t> most_frequent_bytes(std::span<const float> scalars,
                                              std::size_t limit) {
    std::array<std::size_t, 256> counts{};
    for (float value : scalars) {
        ++counts[scalar_to_byte(value)];
    }

    std::vector<std::uint8_t> bytes(256);
    std::iota(bytes.begin(), bytes.end(), static_cast<std::uint8_t>(0));
    std::stable_sort(bytes.begin(), bytes.end(), [&](std::uint8_t left, std::uint8_t right) {
        if (counts[left] != counts[right]) {
            return counts[left] > counts[right];
        }
        return left < right;
    });
    bytes.resize(std::min(limit, bytes.size()));
    return bytes;
}

std::vector<std::uint8_t> frequent_byte_partition(std::span<const float> scalars,
                                                  std::uint8_t track_count) {
    std::vector<std::uint8_t> partition(scalars.size(), track_count - 1u);
    const std::vector<std::uint8_t> frequent =
        most_frequent_bytes(scalars, track_count > 0u ? track_count - 1u : 0u);
    std::array<std::uint8_t, 256> byte_to_track{};
    byte_to_track.fill(track_count - 1u);
    for (std::size_t i = 0; i < frequent.size(); ++i) {
        byte_to_track[frequent[i]] = static_cast<std::uint8_t>(i);
    }
    for (std::size_t i = 0; i < scalars.size(); ++i) {
        partition[i] = byte_to_track[scalar_to_byte(scalars[i])];
    }
    return partition;
}

std::vector<std::uint8_t> bucket_partition(std::span<const float> scalars,
                                           std::uint8_t track_count) {
    std::vector<std::uint8_t> partition(scalars.size(), 0u);
    for (std::size_t i = 0; i < scalars.size(); ++i) {
        const unsigned byte = scalar_to_byte(scalars[i]);
        partition[i] = static_cast<std::uint8_t>(
            std::min<unsigned>(track_count - 1u, (byte * track_count) / 256u));
    }
    return partition;
}

std::vector<std::uint8_t> index_mod_partition(std::size_t sample_count,
                                              std::uint8_t track_count) {
    std::vector<std::uint8_t> partition(sample_count, 0u);
    for (std::size_t i = 0; i < sample_count; ++i) {
        partition[i] = static_cast<std::uint8_t>(i % track_count);
    }
    return partition;
}

Section sparse_section_for_owned_range(std::span<const float> scalars,
                                       std::span<const std::uint8_t> mask,
                                       std::size_t first,
                                       std::size_t last,
                                       const MultiTrackOptions& options) {
    std::vector<std::size_t> owned;
    owned.reserve(last - first + 1u);
    double low = std::numeric_limits<double>::infinity();
    double high = -std::numeric_limits<double>::infinity();
    double sum = 0.0;
    for (std::size_t index = first; index <= last; ++index) {
        if (!get_owned_bit(mask, index)) {
            continue;
        }
        owned.push_back(index);
        const double value = static_cast<double>(scalars[index]);
        low = std::min(low, value);
        high = std::max(high, value);
        sum += value;
    }

    if (owned.empty()) {
        return {};
    }
    if (owned.size() == 1u || high - low <= options.tolerance) {
        const double mean = sum / static_cast<double>(owned.size());
        return constant_section(first, last - first + 1u, mean, options);
    }
    if (owned.size() == 2u) {
        return two_owned_section(scalars, owned[0], owned[1], options);
    }

    return constant_section(owned[0], 1u, scalars[owned[0]], options);
}

std::vector<Section> fit_sparse_track_sections(std::span<const float> scalars,
                                               std::span<const std::uint8_t> mask,
                                               const MultiTrackOptions& options) {
    std::vector<Section> sections;
    const std::size_t sample_count = scalars.size();
    std::size_t index = first_owned_index(mask, sample_count);
    while (index < sample_count) {
        if (cancelled(options)) {
            break;
        }
        if (!get_owned_bit(mask, index)) {
            ++index;
            continue;
        }

        const std::size_t max_length = std::max<std::size_t>(1u, options.max_section_length);
        const std::size_t max_last =
            std::min(sample_count - 1u, index + max_length - 1u);
        Section section = sparse_section_for_owned_range(
            scalars, mask, index, max_last, options);
        if (section.length == 0u) {
            section = constant_section(index, 1u, scalars[index], options);
        }
        sections.push_back(section);
        index = static_cast<std::size_t>(section_end(section));
    }
    return sections;
}

std::vector<Section> fit_parity_sections(std::span<const std::uint8_t> partition,
                                         const MultiTrackOptions& options) {
    std::vector<Section> sections;
    std::size_t start = 0;
    while (start < partition.size()) {
        const std::uint8_t value = partition[start];
        std::size_t end = start + 1u;
        while (end < partition.size() && partition[end] == value) {
            ++end;
        }
        Section section = constant_section(start,
                                           end - start,
                                           static_cast<double>(value),
                                           options);
        section.fit_tolerance = std::max(1.0e-12, 0.5 - options.parity_margin);
        sections.push_back(section);
        start = end;
    }
    return sections;
}

WorkspaceModel make_single_track_workspace(std::span<const float> scalars,
                                           const MultiTrackOptions& options,
                                           std::size_t* section_count) {
    ConvergentFitOptions single_options;
    single_options.tolerance = options.tolerance;
    single_options.default_spacing_nm = options.default_spacing_nm;
    single_options.max_section_length = options.max_section_length;
    single_options.cancel_requested = options.cancel_requested;
    const ConvergentFitResult fit = fit_convergent_sections(scalars, single_options);

    WorkspaceModel workspace;
    Track track = make_full_data_track(0u, scalars.size());
    track.name = "single track";
    track.sections = fit.sections;
    workspace.tracks.push_back(std::move(track));
    workspace.active_track_id = 0u;
    workspace.parity_track_id = kNoParityTrack;
    if (section_count) {
        *section_count = fit.sections.size();
    }
    return workspace;
}

WorkspaceModel workspace_from_partition(std::span<const float> scalars,
                                        std::span<const std::uint8_t> partition,
                                        std::uint8_t data_track_count,
                                        const MultiTrackOptions& options) {
    WorkspaceModel workspace;
    workspace.active_track_id = 0u;
    workspace.parity_track_id = data_track_count;
    workspace.tracks.reserve(static_cast<std::size_t>(data_track_count) + 1u);

    for (std::uint8_t id = 0; id < data_track_count; ++id) {
        Track track;
        track.id = id;
        track.kind = TrackKind::Data;
        track.name = "data track " + std::to_string(static_cast<unsigned>(id));
        reset_owned_mask(track.owned_mask, scalars.size());
        for (std::size_t index = 0; index < partition.size(); ++index) {
            if (partition[index] == id) {
                set_owned_bit(track.owned_mask, index, true);
            }
        }
        track.sections = fit_sparse_track_sections(scalars, track.owned_mask, options);
        workspace.tracks.push_back(std::move(track));
    }

    Track parity;
    parity.id = data_track_count;
    parity.kind = TrackKind::Parity;
    parity.name = "parity";
    parity.sections = fit_parity_sections(partition, options);
    workspace.tracks.push_back(std::move(parity));
    return workspace;
}

bool candidate_is_better(std::size_t candidate_cost,
                         std::size_t candidate_sections,
                         std::size_t best_cost,
                         std::size_t best_sections) {
    if (candidate_cost != best_cost) {
        return candidate_cost < best_cost;
    }
    return candidate_sections < best_sections;
}

}  // namespace

std::uint8_t parity_owner_at(const WorkspaceModel& workspace, std::size_t index) {
    if (workspace.parity_track_id == kNoParityTrack) {
        return 0u;
    }
    const Track* parity = find_track(workspace, workspace.parity_track_id);
    if (!parity) {
        return kNoParityTrack;
    }
    for (const Section& section : parity->sections) {
        if (!section_contains(section, index)) {
            continue;
        }
        const double value = wave_value_at_index(section, index);
        return static_cast<std::uint8_t>(
            std::clamp(std::llround(value), 0ll,
                       static_cast<long long>(kMaxDataTracks - 1u)));
    }
    return kNoParityTrack;
}

double reconstructed_value_at(const WorkspaceModel& workspace, std::size_t index) {
    const std::uint8_t owner = parity_owner_at(workspace, index);
    const Track* track = find_track(workspace, owner);
    if (!track || track->kind != TrackKind::Data ||
        !get_owned_bit(track->owned_mask, index)) {
        return 0.0;
    }
    for (const Section& section : track->sections) {
        if (section_contains(section, index)) {
            return wave_value_at_index(section, index);
        }
    }
    return 0.0;
}

MultiTrackFitResult fit_multi_track_sections(
    std::span<const float> scalars,
    const MultiTrackOptions& options) {
    MultiTrackFitResult result;
    if (scalars.empty()) {
        return result;
    }

    result.workspace = make_single_track_workspace(
        scalars, options, &result.single_track_section_count);
    result.total_section_count = result.single_track_section_count;
    if (cancelled(options)) {
        result.cancelled = true;
        return result;
    }

    std::size_t best_cost = options.allow_single_track_fallback
        ? result.single_track_section_count
        : std::numeric_limits<std::size_t>::max();
    std::size_t best_sections = options.allow_single_track_fallback
        ? result.single_track_section_count
        : std::numeric_limits<std::size_t>::max();
    const std::uint8_t min_tracks =
        std::clamp(options.min_data_tracks, static_cast<std::uint8_t>(2u),
                   static_cast<std::uint8_t>(kMaxDataTracks));
    const std::uint8_t max_tracks =
        std::clamp(options.max_data_tracks, min_tracks,
                   static_cast<std::uint8_t>(kMaxDataTracks));

    for (std::uint8_t k = min_tracks; k <= max_tracks; ++k) {
        const std::array<std::vector<std::uint8_t>, 3> partitions = {
            frequent_byte_partition(scalars, k),
            bucket_partition(scalars, k),
            index_mod_partition(scalars.size(), k),
        };
        for (const std::vector<std::uint8_t>& partition : partitions) {
            if (cancelled(options)) {
                result.cancelled = true;
                return result;
            }
            WorkspaceModel candidate = workspace_from_partition(scalars, partition, k, options);
            const std::size_t section_count = count_track_sections(candidate);
            const std::size_t cost =
                section_count + static_cast<std::size_t>(k) * kTrackMetadataSectionCost;
            if (candidate_is_better(cost, section_count, best_cost, best_sections)) {
                best_cost = cost;
                best_sections = section_count;
                result.workspace = std::move(candidate);
                result.used_parity = true;
                result.total_section_count = section_count;
            }
        }
    }

    if (options.allow_single_track_fallback && best_cost >= result.single_track_section_count) {
        result.workspace = make_single_track_workspace(
            scalars, options, &result.single_track_section_count);
        result.total_section_count = result.single_track_section_count;
        result.used_parity = false;
    }
    return result;
}

}  // namespace thrystr::app
