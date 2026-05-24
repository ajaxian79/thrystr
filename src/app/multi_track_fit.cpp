// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/multi_track_fit.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
#include <quadmath.h>
#endif

namespace thrystr::app {
namespace {

constexpr std::size_t kTrackMetadataSectionCost = 1u;
constexpr double kFitEpsilon = 1.0e-12;

struct HorizontalSegment {
    std::size_t start = 0;
    std::size_t end = 0;
};

struct SparseFitScore {
    Section section;
    double max_residual = std::numeric_limits<double>::infinity();
    double mean_residual = std::numeric_limits<double>::infinity();
    std::size_t owned_count = 0;
};

bool cancelled(const MultiTrackOptions& options) {
    return options.cancel_requested && options.cancel_requested->load();
}

std::uint8_t scalar_to_byte(Scalar value) {
    const double scaled =
        static_cast<double>((value + static_cast<Scalar>(1.0)) * static_cast<Scalar>(128.0));
    return static_cast<std::uint8_t>(std::clamp(std::llround(scaled), 0ll, 255ll));
}

std::size_t count_track_sections(const WorkspaceModel& workspace) {
    std::size_t count = 0;
    for (const Track& track : workspace.tracks) {
        count += track.sections.size();
    }
    return count;
}

bool better_sparse_score(const SparseFitScore& candidate, const SparseFitScore& best) {
    if (candidate.max_residual != best.max_residual) {
        return candidate.max_residual < best.max_residual;
    }
    if (candidate.mean_residual != best.mean_residual) {
        return candidate.mean_residual < best.mean_residual;
    }
    return std::abs(candidate.section.wave_amplitude) < std::abs(best.section.wave_amplitude);
}

Scalar scalar_abs(Scalar value) {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    return fabsq(value);
#else
    return std::abs(value);
#endif
}

Scalar scalar_sin(Scalar value) {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    return sinq(value);
#else
    return std::sin(value);
#endif
}

Scalar scalar_fmod(Scalar value, Scalar divisor) {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    return fmodq(value, divisor);
#else
    return std::fmod(value, divisor);
#endif
}

Scalar scalar_pi() {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    static const Scalar pi = strtoflt128("3.141592653589793238462643383279502884", nullptr);
    return pi;
#else
    return std::numbers::pi_v<Scalar>;
#endif
}

double normalized_phase(Scalar phase, Scalar wavelength) {
    if (wavelength <= 0.0) {
        return 0.0;
    }
    phase = scalar_fmod(phase, wavelength);
    if (phase < 0.0) {
        phase += wavelength;
    }
    return static_cast<double>(phase);
}

Section constant_section(std::size_t start, std::size_t length, double value,
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

SparseFitScore score_owned_section(std::span<const Scalar> scalars,
                                   std::span<const std::size_t> owned_indices, Section section) {
    SparseFitScore score;
    score.section = section;
    score.owned_count = owned_indices.size();
    double sum = 0.0;
    for (const std::size_t index : owned_indices) {
        const double residual =
            static_cast<double>(scalar_abs(wave_value_at_index(section, index) - scalars[index]));
        if (score.max_residual == std::numeric_limits<double>::infinity()) {
            score.max_residual = residual;
        } else {
            score.max_residual = std::max(score.max_residual, residual);
        }
        sum += residual;
    }
    if (owned_indices.empty()) {
        score.max_residual = 0.0;
        score.mean_residual = 0.0;
    } else {
        score.mean_residual = sum / static_cast<double>(owned_indices.size());
    }
    score.section.max_residual = score.max_residual;
    score.section.mean_residual = score.mean_residual;
    return score;
}

Section two_owned_section(std::span<const Scalar> scalars, std::size_t first, std::size_t second,
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
    section.wave_phase_nm = first_value <= second_value ? section.wave_wavelength_nm * 0.25
                                                        : -section.wave_wavelength_nm * 0.25;
    section.fit_tolerance = options.tolerance;
    section.max_residual = 0.0;
    section.mean_residual = 0.0;
    return section;
}

std::vector<double> sparse_wavelength_candidates(double spacing, std::size_t first,
                                                 std::size_t last, int wavelength_steps) {
    const double span_nm = std::max(spacing, static_cast<double>(last - first) * spacing);
    const double low = std::max(spacing * 2.0, span_nm / 64.0);
    const double high = std::max(low * 1.01, span_nm * 16.0);
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(std::max(1, wavelength_steps)) + 128u);

    for (int step = 0; step < std::max(1, wavelength_steps); ++step) {
        const double t = wavelength_steps > 1
                             ? static_cast<double>(step) / static_cast<double>(wavelength_steps - 1)
                             : 0.0;
        values.push_back(std::exp(std::log(low) + (std::log(high) - std::log(low)) * t));
    }
    constexpr std::array<double, 14> period_counts = {0.5, 1.0,  1.5,  2.0,  3.0,  4.0,  6.0,
                                                      8.0, 12.0, 16.0, 24.0, 32.0, 48.0, 64.0};
    for (const double periods : period_counts) {
        values.push_back(std::max(spacing * 2.0, span_nm / periods));
    }

    std::sort(values.begin(), values.end());
    values.erase(
        std::unique(values.begin(), values.end(),
                    [](double left, double right) { return std::abs(left - right) <= 1.0e-9; }),
        values.end());
    return values;
}

SparseFitScore fit_linear_sine_owned_section(std::span<const Scalar> scalars,
                                             std::span<const std::size_t> owned_indices,
                                             double wavelength, double phase,
                                             const MultiTrackOptions& options) {
    if (owned_indices.empty()) {
        return {};
    }

    const std::size_t first = owned_indices.front();
    const std::size_t last = owned_indices.back();
    const double spacing = std::max(kFitEpsilon, options.default_spacing_nm);
    wavelength = std::max(kFitEpsilon, wavelength);

    Scalar sum_s = 0.0;
    Scalar sum_s2 = 0.0;
    Scalar sum_y = 0.0;
    Scalar sum_sy = 0.0;
    for (const std::size_t index : owned_indices) {
        const Scalar x_nm = static_cast<Scalar>(index - first) * static_cast<Scalar>(spacing);
        const Scalar s =
            scalar_sin(static_cast<Scalar>(2.0) * scalar_pi() *
                       (x_nm - static_cast<Scalar>(phase)) / static_cast<Scalar>(wavelength));
        const Scalar y = scalars[index];
        sum_s += s;
        sum_s2 += s * s;
        sum_y += y;
        sum_sy += s * y;
    }

    const Scalar n = static_cast<Scalar>(owned_indices.size());
    const Scalar denom = n * sum_s2 - sum_s * sum_s;
    Scalar wave_half_amplitude = 0.0;
    Scalar center = sum_y / n;
    Scalar stored_phase = static_cast<Scalar>(phase);
    if (scalar_abs(denom) > static_cast<Scalar>(kFitEpsilon)) {
        wave_half_amplitude = (n * sum_sy - sum_s * sum_y) / denom;
        center = (sum_y - wave_half_amplitude * sum_s) / n;
    }
    if (wave_half_amplitude < 0.0) {
        wave_half_amplitude = -wave_half_amplitude;
        stored_phase += static_cast<Scalar>(wavelength) * static_cast<Scalar>(0.5);
    }

    Section section;
    section.start_index = static_cast<std::uint32_t>(first);
    section.length = static_cast<std::uint32_t>(last - first + 1u);
    section.section_spacing_nm = spacing;
    section.wave_wavelength_nm = wavelength;
    section.wave_amplitude = static_cast<double>(wave_half_amplitude * static_cast<Scalar>(2.0));
    section.wave_amplitude_offset = static_cast<double>(center - wave_half_amplitude);
    section.wave_phase_nm = normalized_phase(stored_phase, static_cast<Scalar>(wavelength));
    section.fit_tolerance = options.tolerance;
    return score_owned_section(scalars, owned_indices, section);
}

std::vector<HorizontalSegment> horizontal_segments(std::size_t sample_count,
                                                   const MultiTrackOptions& options) {
    std::vector<HorizontalSegment> segments;
    if (sample_count == 0u) {
        return segments;
    }

    const std::size_t min_length =
        std::max<std::size_t>(1u, std::min(options.min_segment_length, sample_count));
    const std::size_t target_length =
        std::max(min_length, std::max<std::size_t>(1u, options.max_section_length));
    if (sample_count <= min_length) {
        segments.push_back({0u, sample_count});
        return segments;
    }

    std::size_t start = 0u;
    while (sample_count - start > target_length + min_length) {
        segments.push_back({start, start + target_length});
        start += target_length;
    }
    segments.push_back({start, sample_count});
    return segments;
}

std::vector<std::uint8_t>
localized_value_range_partition(std::span<const Scalar> scalars, std::uint8_t track_count,
                                std::span<const HorizontalSegment> segments) {
    std::vector<std::uint8_t> partition(scalars.size(), 0u);
    for (const HorizontalSegment& segment : segments) {
        std::array<std::size_t, 256> counts{};
        for (std::size_t index = segment.start; index < segment.end; ++index) {
            ++counts[scalar_to_byte(scalars[index])];
        }

        std::array<double, kMaxDataTracks> centers{};
        const std::size_t total = segment.end - segment.start;
        std::size_t cumulative = 0u;
        std::uint8_t byte = 0u;
        for (std::uint8_t track = 0; track < track_count; ++track) {
            const std::size_t target =
                std::min(total - 1u, (total * (static_cast<std::size_t>(track) * 2u + 1u)) /
                                         (static_cast<std::size_t>(track_count) * 2u));
            while (byte < 255u && cumulative + counts[byte] <= target) {
                cumulative += counts[byte];
                ++byte;
            }
            centers[track] = static_cast<double>(byte);
        }

        std::array<std::size_t, kMaxDataTracks> assigned_counts{};
        std::array<double, kMaxDataTracks> sums{};
        for (int iteration = 0; iteration < 50; ++iteration) {
            assigned_counts.fill(0u);
            sums.fill(0.0);
            for (std::size_t value = 0; value < counts.size(); ++value) {
                if (counts[value] == 0u) {
                    continue;
                }
                std::uint8_t best_track = 0u;
                double best_distance = std::abs(static_cast<double>(value) - centers[0]);
                for (std::uint8_t track = 1u; track < track_count; ++track) {
                    const double distance = std::abs(static_cast<double>(value) - centers[track]);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_track = track;
                    }
                }
                assigned_counts[best_track] += counts[value];
                sums[best_track] += static_cast<double>(value) * static_cast<double>(counts[value]);
            }

            double max_move = 0.0;
            for (std::uint8_t track = 0u; track < track_count; ++track) {
                if (assigned_counts[track] == 0u) {
                    continue;
                }
                const double next_center =
                    sums[track] / static_cast<double>(assigned_counts[track]);
                max_move = std::max(max_move, std::abs(next_center - centers[track]));
                centers[track] = next_center;
            }
            if (max_move < 0.5) {
                break;
            }
        }

        for (std::size_t index = segment.start; index < segment.end; ++index) {
            const double value = static_cast<double>(scalar_to_byte(scalars[index]));
            std::uint8_t best_track = 0u;
            double best_distance = std::abs(value - centers[0]);
            for (std::uint8_t track = 1u; track < track_count; ++track) {
                const double distance = std::abs(value - centers[track]);
                if (distance < best_distance) {
                    best_distance = distance;
                    best_track = track;
                }
            }
            partition[index] = best_track;
        }
    }
    return partition;
}

Section sparse_section_for_owned_range(std::span<const Scalar> scalars,
                                       std::span<const std::size_t> owned_indices,
                                       const MultiTrackOptions& options) {
    if (owned_indices.empty()) {
        return {};
    }

    double low = std::numeric_limits<double>::infinity();
    double high = -std::numeric_limits<double>::infinity();
    double sum = 0.0;
    for (const std::size_t index : owned_indices) {
        const double value = static_cast<double>(scalars[index]);
        low = std::min(low, value);
        high = std::max(high, value);
        sum += value;
    }

    const std::size_t first = owned_indices.front();
    const std::size_t last = owned_indices.back();
    if (owned_indices.size() == 1u || high - low <= options.tolerance) {
        const double mean = sum / static_cast<double>(owned_indices.size());
        Section section = constant_section(first, last - first + 1u, mean, options);
        return score_owned_section(scalars, owned_indices, section).section;
    }
    if (owned_indices.size() == 2u) {
        return two_owned_section(scalars, owned_indices[0], owned_indices[1], options);
    }

    SparseFitScore best;
    const double spacing = std::max(kFitEpsilon, options.default_spacing_nm);
    const std::vector<double> wavelengths =
        sparse_wavelength_candidates(spacing, first, last, options.wavelength_steps);
    const int phase_steps = std::max(1, options.phase_steps);
    for (const double wavelength : wavelengths) {
        for (int phase_step = 0; phase_step < phase_steps; ++phase_step) {
            const double phase =
                wavelength * static_cast<double>(phase_step) / static_cast<double>(phase_steps);
            SparseFitScore score =
                fit_linear_sine_owned_section(scalars, owned_indices, wavelength, phase, options);
            if (better_sparse_score(score, best)) {
                best = score;
            }
            if (best.max_residual <= options.tolerance || cancelled(options)) {
                return best.section;
            }
        }
    }
    return best.section;
}

bool sparse_range_fits(std::span<const Scalar> scalars, std::span<const std::size_t> owned_indices,
                       const MultiTrackOptions& options, Section* section) {
    const Section candidate = sparse_section_for_owned_range(scalars, owned_indices, options);
    if (section) {
        *section = candidate;
    }
    return candidate.length > 0u && candidate.max_residual <= options.tolerance;
}

std::vector<Section> fit_sparse_track_sections(std::span<const Scalar> scalars,
                                               std::span<const std::uint8_t> mask,
                                               const MultiTrackOptions& options,
                                               std::span<const HorizontalSegment> segments) {
    std::vector<Section> sections;
    const std::size_t sample_count = scalars.size();
    for (const HorizontalSegment& segment : segments) {
        std::vector<std::size_t> owned_indices;
        owned_indices.reserve(segment.end - segment.start);
        for (std::size_t index = segment.start; index < segment.end && index < sample_count;
             ++index) {
            if (get_owned_bit(mask, index)) {
                owned_indices.push_back(index);
            }
        }

        std::size_t cursor = 0u;
        while (cursor < owned_indices.size()) {
            if (cancelled(options)) {
                return sections;
            }

            const std::size_t max_length = std::max<std::size_t>(1u, options.max_section_length);
            const std::size_t max_owned_points =
                std::max<std::size_t>(1u, options.max_owned_points_per_curve);
            const std::size_t index = owned_indices[cursor];
            const std::size_t max_last =
                std::min({sample_count - 1u, segment.end - 1u, index + max_length - 1u});
            const auto high_it =
                std::upper_bound(owned_indices.begin() + static_cast<std::ptrdiff_t>(cursor),
                                 owned_indices.end(), max_last);
            std::size_t high_pos =
                high_it == owned_indices.begin() + static_cast<std::ptrdiff_t>(cursor)
                    ? cursor
                    : static_cast<std::size_t>((high_it - owned_indices.begin()) - 1);
            high_pos = std::min(high_pos, cursor + max_owned_points - 1u);

            Section section = sparse_section_for_owned_range(
                scalars, std::span<const std::size_t>(owned_indices.data() + cursor, 1u), options);
            std::size_t low_pos = cursor;
            while (low_pos < high_pos) {
                const std::size_t mid = low_pos + (high_pos - low_pos + 1u) / 2u;
                Section candidate;
                const std::span<const std::size_t> owned_range(owned_indices.data() + cursor,
                                                               mid - cursor + 1u);
                if (sparse_range_fits(scalars, owned_range, options, &candidate)) {
                    low_pos = mid;
                    section = candidate;
                } else {
                    high_pos = mid - 1u;
                }
                if (cancelled(options)) {
                    return sections;
                }
            }

            if (section.length == 0u) {
                section = constant_section(index, 1u, scalars[index], options);
            }
            sections.push_back(section);
            cursor = static_cast<std::size_t>(
                std::lower_bound(owned_indices.begin() + static_cast<std::ptrdiff_t>(cursor),
                                 owned_indices.end(),
                                 static_cast<std::size_t>(section_end(section))) -
                owned_indices.begin());
        }
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
        Section section = constant_section(start, end - start, static_cast<double>(value), options);
        section.fit_tolerance = std::max(1.0e-12, 0.5 - options.parity_margin);
        sections.push_back(section);
        start = end;
    }
    return sections;
}

WorkspaceModel make_single_track_workspace(std::span<const Scalar> scalars,
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

WorkspaceModel workspace_from_partition(std::span<const Scalar> scalars,
                                        std::span<const std::uint8_t> partition,
                                        std::uint8_t data_track_count,
                                        const MultiTrackOptions& options,
                                        std::span<const HorizontalSegment> segments) {
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
        track.sections = fit_sparse_track_sections(scalars, track.owned_mask, options, segments);
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

bool candidate_is_better(std::size_t candidate_cost, std::size_t candidate_sections,
                         std::size_t best_cost, std::size_t best_sections) {
    if (candidate_cost != best_cost) {
        return candidate_cost < best_cost;
    }
    return candidate_sections < best_sections;
}

} // namespace

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
            std::clamp(std::llround(value), 0ll, static_cast<long long>(kMaxDataTracks - 1u)));
    }
    return kNoParityTrack;
}

Scalar reconstructed_value_at(const WorkspaceModel& workspace, std::size_t index) {
    const std::uint8_t owner = parity_owner_at(workspace, index);
    const Track* track = find_track(workspace, owner);
    if (!track || track->kind != TrackKind::Data || !get_owned_bit(track->owned_mask, index)) {
        return 0.0;
    }
    for (const Section& section : track->sections) {
        if (section_contains(section, index)) {
            return wave_value_at_index(section, index);
        }
    }
    return 0.0;
}

MultiTrackFitResult fit_multi_track_sections(std::span<const Scalar> scalars,
                                             const MultiTrackOptions& options) {
    MultiTrackFitResult result;
    if (scalars.empty()) {
        return result;
    }

    if (options.allow_single_track_fallback) {
        result.workspace =
            make_single_track_workspace(scalars, options, &result.single_track_section_count);
        result.total_section_count = result.single_track_section_count;
    }
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
        std::clamp(options.max_data_tracks, min_tracks, static_cast<std::uint8_t>(kMaxDataTracks));
    const std::vector<HorizontalSegment> segments = horizontal_segments(scalars.size(), options);

    for (std::uint8_t k = min_tracks; k <= max_tracks; ++k) {
        const std::array<std::vector<std::uint8_t>, 1> partitions = {
            localized_value_range_partition(scalars, k, segments),
        };
        for (const std::vector<std::uint8_t>& partition : partitions) {
            if (cancelled(options)) {
                result.cancelled = true;
                return result;
            }
            WorkspaceModel candidate =
                workspace_from_partition(scalars, partition, k, options, segments);
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
        result.workspace =
            make_single_track_workspace(scalars, options, &result.single_track_section_count);
        result.total_section_count = result.single_track_section_count;
        result.used_parity = false;
    }
    return result;
}

} // namespace thrystr::app
