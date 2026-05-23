#include <thrystr/scalar_analysis.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

void test_scalar_mapping() {
    assert(thrystr::byte_to_scalar(0) == -1.0f);
    assert(thrystr::byte_to_scalar(128) == 0.0f);
    assert(std::abs(thrystr::byte_to_scalar(255) - 0.9921875f) < 0.00001f);
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

}  // namespace

int main() {
    test_scalar_mapping();
    test_adjacent_delta();
    test_highest_entropy_window();
    test_x_scale();
    test_phase_fit_smoke();
    return 0;
}
