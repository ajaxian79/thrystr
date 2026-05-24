// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/convergent_fit.hpp>
#include <thrystr/app/fit_validation.hpp>
#include <thrystr/app/workspace.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

std::vector<float> sample_section(const thrystr::app::Section& section) {
    std::vector<float> values;
    values.reserve(section.length);
    for (std::size_t i = 0; i < section.length; ++i) {
        values.push_back(static_cast<float>(
            thrystr::app::wave_value_at_index(section, section.start_index + i)));
    }
    return values;
}

void test_owned_mask_helpers() {
    std::vector<std::uint8_t> mask;
    thrystr::app::reset_owned_mask(mask, 17);
    assert(mask.size() == 3);
    thrystr::app::set_owned_bit(mask, 0, true);
    thrystr::app::set_owned_bit(mask, 8, true);
    thrystr::app::set_owned_bit(mask, 16, true);
    assert(thrystr::app::get_owned_bit(mask, 0));
    assert(thrystr::app::get_owned_bit(mask, 8));
    assert(thrystr::app::get_owned_bit(mask, 16));
    assert(!thrystr::app::get_owned_bit(mask, 17));
    assert(thrystr::app::popcount_owned(mask) == 3);
    assert(thrystr::app::first_owned_index(mask, 17) == 0);
    assert(thrystr::app::last_owned_index_exclusive(mask, 17) == 17);
}

void test_two_point_fit_is_exact() {
    const std::vector<float> values = {-0.75f, 0.625f};
    thrystr::app::ConvergentFitOptions options;
    const thrystr::app::Section section =
        thrystr::app::fit_section(values, 0, values.size(), options);
    assert(section.length == 2);
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_sections(values, std::span<const thrystr::app::Section>(&section, 1));
    assert(report.pass);
    assert(report.max_residual < 1.0e-9);
}

void test_pure_sine_input_yields_single_section() {
    thrystr::app::Section source;
    source.start_index = 0;
    source.length = 129;
    source.section_spacing_nm = 1.0;
    source.wave_wavelength_nm = 32.0;
    source.wave_amplitude = 2.0;
    source.wave_amplitude_offset = -1.0;
    source.wave_phase_nm = 0.0;
    const std::vector<float> values = sample_section(source);

    thrystr::app::ConvergentFitOptions options;
    options.tolerance = 0.025;
    options.max_section_length = values.size();
    options.wavelength_steps = 64;
    options.phase_steps = 64;
    const thrystr::app::ConvergentFitResult result =
        thrystr::app::fit_convergent_sections(values, options);
    assert(!result.cancelled);
    assert(result.sections.size() == 1);
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_sections(values, result.sections);
    assert(report.pass);
}

void test_noise_fit_covers_every_sample() {
    std::vector<float> values;
    values.reserve(64);
    std::uint32_t state = 0x1234abcd;
    for (int i = 0; i < 64; ++i) {
        state = state * 1664525u + 1013904223u;
        const auto byte = static_cast<std::uint8_t>((state >> 16u) & 0xffu);
        values.push_back(static_cast<float>(byte) / 128.0f - 1.0f);
    }

    thrystr::app::ConvergentFitOptions options;
    options.max_section_length = 16;
    const thrystr::app::ConvergentFitResult result =
        thrystr::app::fit_convergent_sections(values, options);
    assert(!result.sections.empty());
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_sections(values, result.sections);
    assert(report.pass);
}

void test_track_validation_detects_overlap() {
    const std::vector<float> values = {0.0f, 0.25f, 0.5f, 0.75f};
    thrystr::app::WorkspaceModel workspace;
    workspace.tracks.push_back(thrystr::app::make_full_data_track(0, values.size()));
    workspace.tracks.push_back(thrystr::app::make_full_data_track(1, values.size()));
    thrystr::app::Section section;
    section.start_index = 0;
    section.length = static_cast<std::uint32_t>(values.size());
    section.wave_amplitude = 0.0;
    section.wave_amplitude_offset = 0.0;
    workspace.tracks[0].sections.push_back(section);
    workspace.tracks[1].sections.push_back(section);
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_tracks(values, workspace);
    assert(!report.pass);
}

void test_track_validation_passes_single_track() {
    const std::vector<float> values = {0.0f, 0.25f};
    thrystr::app::WorkspaceModel workspace;
    workspace.tracks.push_back(thrystr::app::make_full_data_track(0, values.size()));
    workspace.tracks[0].sections.push_back(
        thrystr::app::fit_section(values, 0, values.size()));
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_tracks(values, workspace);
    assert(report.pass);
}

}  // namespace

int main() {
    test_owned_mask_helpers();
    test_two_point_fit_is_exact();
    test_pure_sine_input_yields_single_section();
    test_noise_fit_covers_every_sample();
    test_track_validation_detects_overlap();
    test_track_validation_passes_single_track();
    return 0;
}
