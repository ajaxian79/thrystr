// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/scalar_analysis.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path write_temp_fixture(std::span<const std::uint8_t> bytes,
                                         std::string_view name_hint) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        (std::string("thrystr_") + std::string(name_hint) + ".bin");
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return path;
}

std::vector<std::uint8_t> random_bytes(std::size_t count, std::uint64_t seed) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(count);
    std::uint64_t state = seed;
    for (std::size_t i = 0; i < count; ++i) {
        state += 0x9e3779b97f4a7c15ull;
        std::uint64_t value = state;
        value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
        value ^= value >> 31u;
        bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
    }
    return bytes;
}

void test_scalar_mapping() {
    assert(thrystr::byte_to_scalar(0) == -1.0f);
    assert(thrystr::byte_to_scalar(128) == 0.0f);
    assert(std::abs(thrystr::byte_to_scalar(255) - 0.9921875f) < 0.00001f);
}

void test_unsigned_mod_256_edges() {
    assert(thrystr::unsigned_mod_256(-1.5) == 255);
    assert(thrystr::unsigned_mod_256(1.0e15) ==
           thrystr::unsigned_mod_256(1.0e15));

    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
        bool threw = false;
        try {
            (void)thrystr::unsigned_mod_256(value);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }
}

void test_value_mapper_stack() {
    using Kind = thrystr::ValueMapperKind;

    const std::vector<thrystr::ValueMapper> add = {
        {Kind::Add, 20.0, true},
    };
    assert(thrystr::map_byte_to_wrapped(10, add) == 30);

    const std::vector<thrystr::ValueMapper> subtract = {
        {Kind::Subtract, 20.0, true},
    };
    assert(thrystr::map_byte_to_wrapped(10, subtract) == 246);

    const std::vector<thrystr::ValueMapper> multiply = {
        {Kind::Multiply, 2.0, true},
    };
    assert(thrystr::map_byte_to_wrapped(200, multiply) == 144);

    const std::vector<thrystr::ValueMapper> divide = {
        {Kind::Divide, 4.0, true},
    };
    assert(thrystr::map_byte_to_wrapped(100, divide) == 25);

    const std::vector<thrystr::ValueMapper> disabled = {
        {Kind::Add, 100.0, false},
    };
    assert(thrystr::map_byte_to_wrapped(7, disabled) == 7);
}

void test_apply_value_mappers_rejects_nonfinite_intermediate() {
    const std::vector<thrystr::ValueMapper> inf = {
        {thrystr::ValueMapperKind::Multiply, std::numeric_limits<double>::infinity(), true},
    };
    bool threw_inf = false;
    try {
        (void)thrystr::apply_value_mappers(42.0, inf);
    } catch (const std::invalid_argument&) {
        threw_inf = true;
    }
    assert(threw_inf);

    const std::vector<thrystr::ValueMapper> nan = {
        {thrystr::ValueMapperKind::Add, std::numeric_limits<double>::quiet_NaN(), true},
    };
    bool threw_nan = false;
    try {
        (void)thrystr::apply_value_mappers(42.0, nan);
    } catch (const std::invalid_argument&) {
        threw_nan = true;
    }
    assert(threw_nan);
}

void test_divide_by_zero_rejected() {
    const std::vector<thrystr::ValueMapper> divide_zero = {
        {thrystr::ValueMapperKind::Divide, 0.0, true},
    };

    bool threw = false;
    try {
        (void)thrystr::map_byte_to_wrapped(42, divide_zero);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void test_adjacent_delta() {
    assert(thrystr::adjacent_delta(0, 255) == 255);
    assert(thrystr::adjacent_delta(240, 12) == 228);
    assert(thrystr::adjacent_delta(8, 8) == 0);
}

void test_highest_entropy_window() {
    const std::vector<std::uint8_t> bytes = {10, 11, 12, 250, 251, 252, 1, 2};
    const thrystr::WindowEntropy window = thrystr::find_highest_entropy_window(bytes, 4);
    assert(window.offset == 3);
    assert(window.length == 4);
    assert(window.max_delta == 251);
    assert(window.delta_index == 5);
}

void test_highest_entropy_window_edges() {
    const std::vector<std::uint8_t> single = {42};
    thrystr::WindowEntropy window = thrystr::find_highest_entropy_window(single, 1024);
    assert(window.offset == 0);
    assert(window.length == 1);
    assert(window.max_delta == 0);

    const std::vector<std::uint8_t> equal = {7, 7, 7, 7};
    window = thrystr::find_highest_entropy_window(equal, 4);
    assert(window.offset == 0);
    assert(window.length == 4);
    assert(window.max_delta == 0);

    window = thrystr::find_highest_entropy_window(equal, 1);
    assert(window.offset == 0);
    assert(window.length == 1);
    assert(window.max_delta == 0);
}

void test_x_scale() {
    const std::vector<float> scalars = {-1.0f, 0.0f, 1.0f};
    assert(thrystr::compute_x_scale(scalars, 0.25f) == 4.0f);

    const std::vector<float> flat = {0.1f, 0.2f, 0.3f};
    assert(thrystr::compute_x_scale(flat, 0.25f) == 1.0f);
}

void test_phase_fit_smoke() {
    std::vector<float> scalars;
    for (int i = 0; i < 32; ++i) {
        const double theta = 2.0 * 3.14159265358979323846 * static_cast<double>(i) / 31.0;
        scalars.push_back(static_cast<float>(std::sin(theta)));
    }

    const thrystr::PhaseFit fit = thrystr::fit_wave_phase(
        scalars, false, 1.0, 0.02, 180, 1024);
    assert(fit.hits >= 24);
    assert(fit.tested_points == scalars.size());
}

void test_phase_fit_zero_tolerance_returns() {
    const std::vector<float> scalars = {0.0f, 0.5f, -0.5f, 0.0f};
    const thrystr::PhaseFit fit =
        thrystr::fit_wave_phase(scalars, false, 1.0, 0.0, 8, 4);
    assert(fit.tested_points == scalars.size());
    assert(fit.tolerance > 0.0);
}

void test_phase_fit_respects_wave_scale() {
    std::vector<float> scalars;
    for (int i = 0; i < 64; ++i) {
        const double theta = 4.0 * 3.14159265358979323846 * static_cast<double>(i) / 63.0;
        scalars.push_back(static_cast<float>(std::sin(theta)));
    }

    const thrystr::PhaseFit fit = thrystr::fit_wave_phase(
        scalars, false, 2.0, 0.02, 360, 1024);
    assert(fit.hits >= 48);
}

void test_analyze_file_uses_mapper_stack() {
    const std::vector<std::uint8_t> bytes = {250, 251, 252, 253};
    const std::filesystem::path path = write_temp_fixture(bytes, "mapper_test");

    const std::vector<thrystr::ValueMapper> mappers = {
        {thrystr::ValueMapperKind::Add, 10.0, true},
    };
    const thrystr::Analysis analysis = thrystr::analyze_file(
        path, 4, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveScale,
        thrystr::kDefaultWaveTolerance, 90, 1024, mappers);

    assert(analysis.bytes.size() == 4);
    assert(analysis.mapped_bytes.size() == 4);
    assert(analysis.mapped_bytes[0] == 4);
    assert(analysis.mapped_bytes[3] == 7);
    assert(analysis.mappers.size() == 1);
    assert(analysis.scalars[0] == thrystr::byte_to_scalar(4));

    std::filesystem::remove(path);
}

void test_analyze_file_streams_best_window_only() {
    const std::vector<std::uint8_t> bytes = {10, 11, 12, 20, 21, 250, 1, 2};
    const std::filesystem::path path = write_temp_fixture(bytes, "streaming_window_test");

    const thrystr::Analysis analysis = thrystr::analyze_file(
        path, 4, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveScale,
        thrystr::kDefaultWaveTolerance, 90, 1024);

    assert(analysis.source_size == 8);
    assert(analysis.window_count == 5);
    assert(analysis.window.offset == 3);
    assert(analysis.window.length == 4);
    assert(analysis.window.max_delta == 249);
    assert(analysis.window.delta_index == 5);
    assert((analysis.bytes == std::vector<std::uint8_t>{20, 21, 250, 1}));
    assert(analysis.bytes == analysis.mapped_bytes);
    assert(analysis.scalars.size() == 4);

    std::filesystem::remove(path);
}

void test_stream_with_mapper_at_wrap_boundary() {
    const std::vector<std::uint8_t> bytes = {200, 100, 50};
    const std::filesystem::path path = write_temp_fixture(bytes, "wrap_boundary");
    const std::vector<thrystr::ValueMapper> mappers = {
        {thrystr::ValueMapperKind::Add, 100.0, true},
    };
    const thrystr::Analysis analysis = thrystr::analyze_file(
        path, 3, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveScale,
        thrystr::kDefaultWaveTolerance, 16, 16, mappers);
    assert((analysis.mapped_bytes == std::vector<std::uint8_t>{44, 200, 150}));
    std::filesystem::remove(path);
}

void test_read_file_bytes_edges() {
    const std::filesystem::path empty_path = write_temp_fixture({}, "empty");
    assert(thrystr::read_file_bytes(empty_path).empty());
    std::filesystem::remove(empty_path);

    bool missing_threw = false;
    try {
        (void)thrystr::read_file_bytes(
            std::filesystem::temp_directory_path() / "thrystr_missing_fixture.bin");
    } catch (const std::runtime_error&) {
        missing_threw = true;
    }
    assert(missing_threw);

    bool dir_threw = false;
    try {
        (void)thrystr::read_file_bytes(std::filesystem::temp_directory_path());
    } catch (const std::runtime_error&) {
        dir_threw = true;
    }
    assert(dir_threw);
}

void test_analyze_file_zero_phase_test_points() {
    const std::vector<std::uint8_t> bytes = {1, 2, 3, 4};
    const std::filesystem::path path = write_temp_fixture(bytes, "zero_phase_points");
    const thrystr::Analysis analysis = thrystr::analyze_file(
        path, 4, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveScale,
        thrystr::kDefaultWaveTolerance, 720, 0);
    assert(analysis.sine.tested_points == 0);
    assert(analysis.cosine.tested_points == 0);
    std::filesystem::remove(path);
}

void test_max_delta_sample_index_coordinates() {
    const std::vector<std::uint8_t> local_zero = {255, 0, 0, 0};
    std::filesystem::path path = write_temp_fixture(local_zero, "delta_local_zero");
    thrystr::Analysis analysis = thrystr::analyze_file(path, 4);
    assert(analysis.max_delta_sample_index == 0);
    std::filesystem::remove(path);

    const std::vector<std::uint8_t> shifted = {1, 2, 3, 250, 4};
    path = write_temp_fixture(shifted, "delta_shifted");
    analysis = thrystr::analyze_file(path, 4);
    assert(analysis.window.delta_index >= analysis.window.offset);
    assert(analysis.max_delta_sample_index ==
           analysis.window.delta_index - analysis.window.offset);
    std::filesystem::remove(path);
}

void test_mapped_and_streaming_scan_parity() {
    const std::vector<std::uint8_t> bytes = random_bytes(4u * 1024u * 1024u, 0x12345678u);
    const std::filesystem::path path = write_temp_fixture(bytes, "mmap_stream_parity");
    const thrystr::Analysis mapped = thrystr::analyze_file(path, 1024u * 1024u);
    const std::vector<thrystr::ValueMapper> identity = {
        {thrystr::ValueMapperKind::Add, 0.0, true},
    };
    const thrystr::Analysis streamed = thrystr::analyze_file(
        path, 1024u * 1024u, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveScale,
        thrystr::kDefaultWaveTolerance, 16, 16, identity);
    assert(mapped.window.offset == streamed.window.offset);
    assert(mapped.window.length == streamed.window.length);
    assert(mapped.window.max_delta == streamed.window.max_delta);
    assert(mapped.window.delta_index == streamed.window.delta_index);
    assert(mapped.bytes == streamed.bytes);
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    test_scalar_mapping();
    test_unsigned_mod_256_edges();
    test_value_mapper_stack();
    test_apply_value_mappers_rejects_nonfinite_intermediate();
    test_divide_by_zero_rejected();
    test_adjacent_delta();
    test_highest_entropy_window();
    test_highest_entropy_window_edges();
    test_x_scale();
    test_phase_fit_smoke();
    test_phase_fit_zero_tolerance_returns();
    test_phase_fit_respects_wave_scale();
    test_analyze_file_uses_mapper_stack();
    test_analyze_file_streams_best_window_only();
    test_stream_with_mapper_at_wrap_boundary();
    test_read_file_bytes_edges();
    test_analyze_file_zero_phase_test_points();
    test_max_delta_sample_index_coordinates();
    test_mapped_and_streaming_scan_parity();
    return 0;
}
