// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/fit_validation.hpp>
#include <thrystr/app/multi_track_fit.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

std::vector<float> bytes_to_scalars(std::string_view text) {
    std::vector<float> values;
    values.reserve(text.size());
    for (unsigned char byte : text) {
        values.push_back(static_cast<float>(static_cast<double>(byte) / 128.0 - 1.0));
    }
    return values;
}

std::vector<float> pseudo_random_scalars(std::size_t count) {
    std::vector<float> values;
    values.reserve(count);
    std::uint32_t state = 0x7b3419u;
    for (std::size_t i = 0; i < count; ++i) {
        state = state * 1664525u + 1013904223u;
        const auto byte = static_cast<std::uint8_t>((state >> 16u) & 0xffu);
        values.push_back(static_cast<float>(static_cast<double>(byte) / 128.0 - 1.0));
    }
    return values;
}

void assert_reconstruction_matches(std::span<const float> values,
                                   const thrystr::app::WorkspaceModel& workspace,
                                   double tolerance) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double reconstructed = thrystr::app::reconstructed_value_at(workspace, i);
        assert(std::abs(reconstructed - static_cast<double>(values[i])) <= tolerance);
    }
}

void test_all_constant_fixture_single_track_zero_amplitude() {
    const std::vector<float> values(128, 0.25f);
    thrystr::app::MultiTrackOptions options;
    options.tolerance = 1.0e-6;
    const thrystr::app::MultiTrackFitResult result =
        thrystr::app::fit_multi_track_sections(values, options);
    assert(!result.used_parity);
    assert(result.workspace.tracks.size() == 1);
    assert(result.workspace.tracks[0].sections.size() == 1);
    assert(result.workspace.tracks[0].sections[0].wave_amplitude == 0.0);
}

void test_forced_multi_track_round_trip_is_valid() {
    const std::vector<float> values =
        bytes_to_scalars("aaaa bbbb aaaa bbbb aaaa bbbb aaaa bbbb ");
    thrystr::app::MultiTrackOptions options;
    options.allow_single_track_fallback = false;
    options.min_data_tracks = 2;
    options.max_data_tracks = 3;
    options.tolerance = 1.0e-6;
    options.max_section_length = values.size();
    const thrystr::app::MultiTrackFitResult result =
        thrystr::app::fit_multi_track_sections(values, options);
    assert(result.used_parity);
    assert(result.workspace.parity_track_id != thrystr::app::kNoParityTrack);
    const thrystr::app::ValidationReport report =
        thrystr::app::validate_tracks(values, result.workspace, options.parity_margin);
    assert(report.pass);
    assert_reconstruction_matches(values, result.workspace, options.tolerance);
}

void test_random_bytes_fixture_prefers_single_track() {
    const std::vector<float> values = pseudo_random_scalars(256);
    thrystr::app::MultiTrackOptions options;
    options.min_data_tracks = 2;
    options.max_data_tracks = 4;
    options.tolerance = 1.0e-6;
    options.max_section_length = 64;
    const thrystr::app::MultiTrackFitResult result =
        thrystr::app::fit_multi_track_sections(values, options);
    assert(!result.used_parity);
    assert(result.workspace.parity_track_id == thrystr::app::kNoParityTrack);
}

void test_deterministic_multi_track_fit() {
    const std::vector<float> values =
        bytes_to_scalars("the quick brown fox jumps over the lazy dog. "
                         "the quick brown fox jumps over the lazy dog. ");
    thrystr::app::MultiTrackOptions options;
    options.allow_single_track_fallback = false;
    options.min_data_tracks = 4;
    options.max_data_tracks = 4;
    options.max_section_length = 128;
    const thrystr::app::MultiTrackFitResult left =
        thrystr::app::fit_multi_track_sections(values, options);
    const thrystr::app::MultiTrackFitResult right =
        thrystr::app::fit_multi_track_sections(values, options);
    assert(left.used_parity == right.used_parity);
    assert(left.total_section_count == right.total_section_count);
    assert(left.workspace.tracks.size() == right.workspace.tracks.size());
    for (std::size_t i = 0; i < left.workspace.tracks.size(); ++i) {
        assert(left.workspace.tracks[i].id == right.workspace.tracks[i].id);
        assert(left.workspace.tracks[i].kind == right.workspace.tracks[i].kind);
        assert(left.workspace.tracks[i].sections.size() ==
               right.workspace.tracks[i].sections.size());
    }
}

}  // namespace

int main() {
    test_all_constant_fixture_single_track_zero_amplitude();
    test_forced_multi_track_round_trip_is_valid();
    test_random_bytes_fixture_prefers_single_track();
    test_deterministic_multi_track_fit();
    return 0;
}
