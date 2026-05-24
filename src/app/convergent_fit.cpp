// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/convergent_fit.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <vector>

namespace thrystr::app {
namespace {

struct CandidateScore {
    Section section;
    double max_residual = std::numeric_limits<double>::infinity();
    double mean_residual = std::numeric_limits<double>::infinity();
};

bool cancelled(const ConvergentFitOptions& options) {
    return options.cancel_requested && options.cancel_requested->load();
}

std::pair<double, double> min_max_range(std::span<const float> scalars, std::size_t start,
                                        std::size_t length) {
    double low = std::numeric_limits<double>::infinity();
    double high = -std::numeric_limits<double>::infinity();
    for (std::size_t index = start; index < start + length; ++index) {
        const double value = static_cast<double>(scalars[index]);
        low = std::min(low, value);
        high = std::max(high, value);
    }
    return {low, high};
}

CandidateScore score_candidate(std::span<const float> scalars, std::size_t start,
                               std::size_t length, Section section) {
    CandidateScore score;
    score.section = section;
    double sum = 0.0;
    for (std::size_t index = start; index < start + length; ++index) {
        const double residual =
            std::abs(wave_value_at_index(section, index) - static_cast<double>(scalars[index]));
        score.max_residual = std::max(score.max_residual == std::numeric_limits<double>::infinity()
                                          ? 0.0
                                          : score.max_residual,
                                      residual);
        sum += residual;
    }
    score.mean_residual = length == 0u ? 0.0 : sum / static_cast<double>(length);
    score.section.max_residual = score.max_residual;
    score.section.mean_residual = score.mean_residual;
    return score;
}

bool better_score(const CandidateScore& candidate, const CandidateScore& best) {
    if (candidate.max_residual != best.max_residual) {
        return candidate.max_residual < best.max_residual;
    }
    if (candidate.mean_residual != best.mean_residual) {
        return candidate.mean_residual < best.mean_residual;
    }
    return candidate.section.wave_amplitude < best.section.wave_amplitude;
}

Section singleton_section(std::span<const float> scalars, std::size_t start,
                          const ConvergentFitOptions& options) {
    Section section;
    section.start_index = static_cast<std::uint32_t>(start);
    section.length = 1u;
    section.section_spacing_nm = std::max(1.0e-12, options.default_spacing_nm);
    section.wave_wavelength_nm = section.section_spacing_nm * 2.0;
    section.wave_amplitude = 0.0;
    section.wave_amplitude_offset = static_cast<double>(scalars[start]);
    section.fit_tolerance = options.tolerance;
    return section;
}

Section two_point_section(std::span<const float> scalars, std::size_t start,
                          const ConvergentFitOptions& options) {
    const double first = static_cast<double>(scalars[start]);
    const double second = static_cast<double>(scalars[start + 1u]);
    const double low = std::min(first, second);
    const double high = std::max(first, second);
    const double spacing = std::max(1.0e-12, options.default_spacing_nm);

    Section section;
    section.start_index = static_cast<std::uint32_t>(start);
    section.length = 2u;
    section.section_spacing_nm = spacing;
    section.wave_wavelength_nm = spacing * 2.0;
    section.wave_amplitude = high - low;
    section.wave_amplitude_offset = low;
    section.wave_phase_nm = first <= second ? spacing * 0.5 : -spacing * 0.5;
    section.fit_tolerance = options.tolerance;
    const CandidateScore score = score_candidate(scalars, start, 2u, section);
    return score.section;
}

std::vector<double> wavelength_candidates(double spacing, std::size_t length,
                                          int wavelength_steps) {
    const double span_nm = std::max(spacing, static_cast<double>(length - 1u) * spacing);
    const double low = std::max(spacing * 2.0, span_nm / 32.0);
    const double high = std::max(low * 1.01, span_nm * 4.0);
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(std::max(1, wavelength_steps)) + 64u);

    for (int step = 0; step < std::max(1, wavelength_steps); ++step) {
        const double t = wavelength_steps > 1
                             ? static_cast<double>(step) / static_cast<double>(wavelength_steps - 1)
                             : 0.0;
        values.push_back(std::exp(std::log(low) + (std::log(high) - std::log(low)) * t));
    }
    for (double periods = 0.5; periods <= 32.0; periods += 0.5) {
        values.push_back(std::max(spacing * 2.0, span_nm / periods));
    }

    std::sort(values.begin(), values.end());
    values.erase(
        std::unique(values.begin(), values.end(),
                    [](double left, double right) { return std::abs(left - right) <= 1.0e-9; }),
        values.end());
    return values;
}

CandidateScore fit_grid_section(std::span<const float> scalars, std::size_t start,
                                std::size_t length, const ConvergentFitOptions& options) {
    const auto [low, high] = min_max_range(scalars, start, length);
    if (high - low <= options.tolerance) {
        Section section = singleton_section(scalars, start, options);
        section.length = static_cast<std::uint32_t>(length);
        section.wave_amplitude_offset = (low + high) * 0.5;
        return score_candidate(scalars, start, length, section);
    }

    CandidateScore best;
    const double spacing = std::max(1.0e-12, options.default_spacing_nm);
    const std::vector<double> wavelengths =
        wavelength_candidates(spacing, length, options.wavelength_steps);
    for (const double wavelength : wavelengths) {
        const int phase_steps = std::max(1, options.phase_steps);
        for (int phase_step = 0; phase_step < phase_steps; ++phase_step) {
            const double phase =
                wavelength * static_cast<double>(phase_step) / static_cast<double>(phase_steps);
            Section section;
            section.start_index = static_cast<std::uint32_t>(start);
            section.length = static_cast<std::uint32_t>(length);
            section.section_spacing_nm = spacing;
            section.wave_wavelength_nm = wavelength;
            section.wave_amplitude = high - low;
            section.wave_amplitude_offset = low;
            section.wave_phase_nm = phase;
            section.fit_tolerance = options.tolerance;
            CandidateScore score = score_candidate(scalars, start, length, section);
            if (better_score(score, best)) {
                best = score;
            }
            if (best.max_residual <= options.tolerance || cancelled(options)) {
                return best;
            }
        }
    }
    return best;
}

bool range_fits(std::span<const float> scalars, std::size_t start, std::size_t length,
                const ConvergentFitOptions& options, Section* section) {
    const Section candidate = fit_section(scalars, start, length, options);
    if (section) {
        *section = candidate;
    }
    return candidate.max_residual <= options.tolerance;
}

} // namespace

Section fit_section(std::span<const float> scalars, std::size_t start, std::size_t length,
                    const ConvergentFitOptions& options) {
    if (start >= scalars.size() || length == 0u) {
        return {};
    }
    length = std::min(length, scalars.size() - start);
    if (length == 1u) {
        return singleton_section(scalars, start, options);
    }
    if (length == 2u) {
        return two_point_section(scalars, start, options);
    }
    return fit_grid_section(scalars, start, length, options).section;
}

ConvergentFitResult fit_convergent_sections(std::span<const float> scalars,
                                            const ConvergentFitOptions& options) {
    ConvergentFitResult result;
    if (scalars.empty()) {
        return result;
    }

    const std::size_t max_len = std::max<std::size_t>(2u, options.max_section_length);
    std::size_t start = 0;
    double residual_sum = 0.0;
    std::size_t residual_samples = 0;
    while (start < scalars.size()) {
        if (cancelled(options)) {
            result.cancelled = true;
            break;
        }

        const std::size_t remaining = scalars.size() - start;
        const std::size_t candidate_max = std::min(remaining, max_len);
        Section best;
        std::size_t low = std::min<std::size_t>(2u, candidate_max);
        std::size_t high = candidate_max;
        if (range_fits(scalars, start, high, options, &best)) {
            low = high;
        } else {
            Section fallback = fit_section(scalars, start, low, options);
            best = fallback;
            while (low + 1u < high) {
                const std::size_t mid = low + (high - low) / 2u;
                Section candidate;
                if (range_fits(scalars, start, mid, options, &candidate)) {
                    low = mid;
                    best = candidate;
                } else {
                    high = mid;
                }
                if (cancelled(options)) {
                    result.cancelled = true;
                    break;
                }
            }
        }

        if (best.length == 0u || best.max_residual > options.tolerance) {
            best = remaining >= 2u ? two_point_section(scalars, start, options)
                                   : singleton_section(scalars, start, options);
        }

        result.max_residual = std::max(result.max_residual, best.max_residual);
        residual_sum += best.mean_residual * static_cast<double>(best.length);
        residual_samples += best.length;
        start += best.length;
        result.sections.push_back(best);
    }

    result.mean_residual =
        residual_samples == 0u ? 0.0 : residual_sum / static_cast<double>(residual_samples);
    return result;
}

} // namespace thrystr::app
