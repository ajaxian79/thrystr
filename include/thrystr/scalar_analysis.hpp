#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace thrystr {

constexpr std::size_t kDefaultWindowBytes = 1024u * 1024u;
constexpr float kDefaultMaxSlope = 0.25f;
constexpr double kDefaultWaveScale = 1.0;
constexpr double kDefaultWaveTolerance = 1.0 / 128.0;

struct WindowEntropy {
    std::size_t offset = 0;
    std::size_t length = 0;
    std::uint8_t max_delta = 0;
    std::size_t delta_index = 0;
};

struct PhaseFit {
    double phase_radians = 0.0;
    std::size_t hits = 0;
    std::size_t tested_points = 0;
    double tolerance = kDefaultWaveTolerance;
};

enum class ValueMapperKind {
    Add,
    Subtract,
    Multiply,
    Divide,
};

struct ValueMapper {
    ValueMapperKind kind = ValueMapperKind::Add;
    double operand = 0.0;
    bool enabled = true;
};

struct Analysis {
    std::filesystem::path source_path;
    std::uintmax_t source_size = 0;
    WindowEntropy window;
    std::size_t window_count = 0;
    std::size_t max_delta_sample_index = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mapped_bytes;
    std::vector<float> scalars;
    std::vector<ValueMapper> mappers;
    float max_abs_scalar_delta = 0.0f;
    float x_scale = 1.0f;
    double wave_scale = kDefaultWaveScale;
    PhaseFit sine;
    PhaseFit cosine;
};

const char* mapper_kind_name(ValueMapperKind kind);
double apply_value_mapper(double value, const ValueMapper& mapper);
double apply_value_mappers(double value, std::span<const ValueMapper> mappers);
std::uint8_t unsigned_mod_256(double value);
std::uint8_t map_byte_to_wrapped(std::uint8_t byte,
                                 std::span<const ValueMapper> mappers = {});
float map_byte_to_scalar(std::uint8_t byte,
                         std::span<const ValueMapper> mappers = {});
std::vector<std::uint8_t> map_bytes_to_wrapped(
    std::span<const std::uint8_t> bytes,
    std::span<const ValueMapper> mappers = {});

float byte_to_scalar(std::uint8_t byte);
std::vector<float> map_bytes_to_scalars(
    std::span<const std::uint8_t> bytes,
    std::span<const ValueMapper> mappers = {});
std::uint8_t adjacent_delta(std::uint8_t left, std::uint8_t right);
std::vector<std::uint8_t> adjacent_deltas(std::span<const std::uint8_t> bytes);

WindowEntropy find_highest_entropy_window(
    std::span<const std::uint8_t> bytes,
    std::size_t window_bytes = kDefaultWindowBytes);

float max_abs_scalar_delta(std::span<const float> scalars);
float compute_x_scale(std::span<const float> scalars,
                      float max_slope = kDefaultMaxSlope);

PhaseFit fit_wave_phase(std::span<const float> scalars,
                        bool cosine,
                        double wave_scale = kDefaultWaveScale,
                        double tolerance = kDefaultWaveTolerance,
                        int phase_steps = 720,
                        std::size_t max_test_points = 65536);

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path);

Analysis analyze_file(const std::filesystem::path& path,
                      std::size_t window_bytes = kDefaultWindowBytes,
                      float max_slope = kDefaultMaxSlope,
                      double wave_scale = kDefaultWaveScale,
                      double wave_tolerance = kDefaultWaveTolerance,
                      int phase_steps = 720,
                      std::size_t max_phase_test_points = 65536,
                      std::span<const ValueMapper> mappers = {});

}  // namespace thrystr
