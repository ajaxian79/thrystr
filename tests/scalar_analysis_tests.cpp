#include <thrystr/scalar_analysis.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

void test_scalar_mapping() {
    assert(thrystr::byte_to_scalar(0) == -1.0f);
    assert(thrystr::byte_to_scalar(128) == 0.0f);
    assert(std::abs(thrystr::byte_to_scalar(255) - 0.9921875f) < 0.00001f);
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

    const thrystr::PhaseFit fit = thrystr::fit_wave_phase(scalars, false, 0.02, 180, 1024);
    assert(fit.hits >= 24);
    assert(fit.tested_points == scalars.size());
}

void test_analyze_file_uses_mapper_stack() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "thrystr_mapper_test.bin";
    {
        std::ofstream output(path, std::ios::binary);
        const char bytes[] = {
            static_cast<char>(250),
            static_cast<char>(251),
            static_cast<char>(252),
            static_cast<char>(253),
        };
        output.write(bytes, sizeof(bytes));
    }

    const std::vector<thrystr::ValueMapper> mappers = {
        {thrystr::ValueMapperKind::Add, 10.0, true},
    };
    const thrystr::Analysis analysis = thrystr::analyze_file(
        path, 4, thrystr::kDefaultMaxSlope, thrystr::kDefaultWaveTolerance,
        90, 1024, mappers);

    assert(analysis.bytes.size() == 4);
    assert(analysis.mapped_bytes.size() == 4);
    assert(analysis.mapped_bytes[0] == 4);
    assert(analysis.mapped_bytes[3] == 7);
    assert(analysis.mappers.size() == 1);
    assert(analysis.scalars[0] == thrystr::byte_to_scalar(4));

    std::filesystem::remove(path);
}

}  // namespace

int main() {
    test_scalar_mapping();
    test_value_mapper_stack();
    test_divide_by_zero_rejected();
    test_adjacent_delta();
    test_highest_entropy_window();
    test_x_scale();
    test_phase_fit_smoke();
    test_analyze_file_uses_mapper_stack();
    return 0;
}
