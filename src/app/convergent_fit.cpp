// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/convergent_fit.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <vector>
#if defined(THRYSTR_HAS_FLOAT128)
#include <quadmath.h>
#endif

namespace thrystr::app {
namespace {

struct CandidateScore {
    Section section;
    double max_residual = std::numeric_limits<double>::infinity();
    double mean_residual = std::numeric_limits<double>::infinity();
};

constexpr double kFitEpsilon = 1.0e-12;

Scalar scalar_abs(Scalar value) {
#if defined(THRYSTR_HAS_FLOAT128)
    return fabsq(value);
#else
    return std::abs(value);
#endif
}

Scalar scalar_sin(Scalar value) {
#if defined(THRYSTR_HAS_FLOAT128)
    return sinq(value);
#else
    return std::sin(value);
#endif
}

Scalar scalar_fmod(Scalar value, Scalar divisor) {
#if defined(THRYSTR_HAS_FLOAT128)
    return fmodq(value, divisor);
#else
    return std::fmod(value, divisor);
#endif
}

Scalar scalar_pi() {
#if defined(THRYSTR_HAS_FLOAT128)
    static const Scalar pi = strtoflt128("3.141592653589793238462643383279502884", nullptr);
    return pi;
#else
    return std::numbers::pi_v<Scalar>;
#endif
}

bool cancelled(const ConvergentFitOptions& options) {
    return options.cancel_requested && options.cancel_requested->load();
}

double normalize_phase(Scalar phase, Scalar wavelength) {
    if (wavelength <= 0.0) {
        return 0.0;
    }
    phase = scalar_fmod(phase, wavelength);
    if (phase < 0.0) {
        phase += wavelength;
    }
    return static_cast<double>(phase);
}

std::pair<Scalar, Scalar> min_max_range(std::span<const Scalar> scalars, std::size_t start,
                                        std::size_t length) {
    Scalar low = std::numeric_limits<double>::infinity();
    Scalar high = -std::numeric_limits<double>::infinity();
    for (std::size_t index = start; index < start + length; ++index) {
        const Scalar value = scalars[index];
        low = std::min(low, value);
        high = std::max(high, value);
    }
    return {low, high};
}

CandidateScore score_candidate(std::span<const Scalar> scalars, std::size_t start,
                               std::size_t length, Section section) {
    CandidateScore score;
    score.section = section;
    double sum = 0.0;
    for (std::size_t index = start; index < start + length; ++index) {
        const double residual =
            static_cast<double>(scalar_abs(wave_value_at_index(section, index) - scalars[index]));
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

Section singleton_section(std::span<const Scalar> scalars, std::size_t start,
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

Section two_point_section(std::span<const Scalar> scalars, std::size_t start,
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
    values.reserve(static_cast<std::size_t>(std::max(1, wavelength_steps)) + 16u);

    for (int step = 0; step < std::max(1, wavelength_steps); ++step) {
        const double t = wavelength_steps > 1
                             ? static_cast<double>(step) / static_cast<double>(wavelength_steps - 1)
                             : 0.0;
        values.push_back(std::exp(std::log(low) + (std::log(high) - std::log(low)) * t));
    }
    constexpr std::array<double, 12> period_counts = {0.5, 1.0, 1.5,  2.0,  3.0,  4.0,
                                                      6.0, 8.0, 12.0, 16.0, 24.0, 32.0};
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

CandidateScore fit_grid_section(std::span<const Scalar> scalars, std::size_t start,
                                std::size_t length, const ConvergentFitOptions& options) {
    const auto [low, high] = min_max_range(scalars, start, length);
    if (high - low <= static_cast<Scalar>(options.tolerance)) {
        Section section = singleton_section(scalars, start, options);
        section.length = static_cast<std::uint32_t>(length);
        section.wave_amplitude_offset =
            static_cast<double>((low + high) * static_cast<Scalar>(0.5));
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
            Scalar sum_s = 0.0;
            Scalar sum_s2 = 0.0;
            Scalar sum_y = 0.0;
            Scalar sum_sy = 0.0;
            for (std::size_t index = start; index < start + length; ++index) {
                const Scalar x_nm =
                    static_cast<Scalar>(index - start) * static_cast<Scalar>(spacing);
                const Scalar s = scalar_sin(static_cast<Scalar>(2.0) * scalar_pi() *
                                            (x_nm - static_cast<Scalar>(phase)) /
                                            static_cast<Scalar>(wavelength));
                const Scalar y = scalars[index];
                sum_s += s;
                sum_s2 += s * s;
                sum_y += y;
                sum_sy += s * y;
            }

            const Scalar n = static_cast<Scalar>(length);
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
                stored_phase += wavelength * 0.5;
            }

            Section section;
            section.start_index = static_cast<std::uint32_t>(start);
            section.length = static_cast<std::uint32_t>(length);
            section.section_spacing_nm = spacing;
            section.wave_wavelength_nm = wavelength;
            section.wave_amplitude =
                static_cast<double>(wave_half_amplitude * static_cast<Scalar>(2.0));
            section.wave_amplitude_offset = static_cast<double>(center - wave_half_amplitude);
            section.wave_phase_nm = normalize_phase(stored_phase, static_cast<Scalar>(wavelength));
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

bool range_fits(std::span<const Scalar> scalars, std::size_t start, std::size_t length,
                const ConvergentFitOptions& options, Section* section) {
    const Section candidate = fit_section(scalars, start, length, options);
    if (section) {
        *section = candidate;
    }
    return candidate.max_residual <= options.tolerance;
}

} // namespace

Section fit_section(std::span<const Scalar> scalars, std::size_t start, std::size_t length,
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

ConvergentFitResult fit_convergent_sections(std::span<const Scalar> scalars,
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
