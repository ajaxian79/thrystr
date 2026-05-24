// SPDX-License-Identifier: LicenseRef-thrystr-dual
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
    /// Source-file offset of the first byte in the selected window.
    std::size_t offset = 0;
    /// Number of bytes in the selected window.
    std::size_t length = 0;
    /// Largest absolute adjacent-byte delta inside the selected window.
    std::uint8_t max_delta = 0;
    /// Source-file coordinate of the delta that produced `max_delta`.
    std::size_t delta_index = 0;
};

struct PhaseFit {
    /// Best phase in radians.
    double phase_radians = 0.0;
    /// Number of sampled points hit within `tolerance`.
    std::size_t hits = 0;
    /// Number of points examined while fitting.
    std::size_t tested_points = 0;
    /// Tolerance used during the fit.
    double tolerance = kDefaultWaveTolerance;
};

/// One mapper stage in the deterministic value-mapper stack.
enum class ValueMapperKind {
    Add,
    Subtract,
    Multiply,
    Divide,
};

struct ValueMapper {
    /// Arithmetic operation applied by this stage.
    ValueMapperKind kind = ValueMapperKind::Add;
    /// Constant operand used by the operation.
    double operand = 0.0;
    /// Disabled stages are skipped without changing the value.
    bool enabled = true;
};

struct Analysis {
    /// Source file path analyzed.
    std::filesystem::path source_path;
    /// Full source file size in bytes.
    std::uintmax_t source_size = 0;
    /// Highest-entropy window selected from the source.
    WindowEntropy window;
    /// Number of sliding windows considered.
    std::size_t window_count = 0;
    /// Delta index localized to `bytes`.
    std::size_t max_delta_sample_index = 0;
    /// Raw bytes copied from the selected source window.
    std::vector<std::uint8_t> bytes;
    /// Selected bytes after the mapper stack and unsigned modulo wrap.
    std::vector<std::uint8_t> mapped_bytes;
    /// `mapped_bytes` converted to scalar values in [-1, 1).
    std::vector<float> scalars;
    /// Mapper stack used to produce `mapped_bytes`.
    std::vector<ValueMapper> mappers;
    /// Largest absolute adjacent scalar delta in the selected window.
    float max_abs_scalar_delta = 0.0f;
    /// X scale required to keep adjacent slopes under the requested maximum.
    float x_scale = 1.0f;
    /// Sine/cosine wave frequency multiplier used for phase fitting.
    double wave_scale = kDefaultWaveScale;
    /// Best sine phase fit.
    PhaseFit sine;
    /// Best cosine phase fit.
    PhaseFit cosine;
};

/// Return a stable lowercase display name for a mapper kind.
const char* mapper_kind_name(ValueMapperKind kind);

/// Apply one enabled value mapper to a scalar byte-domain value.
///
/// @throws std::invalid_argument if division by zero is requested.
double apply_value_mapper(double value, const ValueMapper& mapper);

/// Apply every enabled mapper in order.
///
/// @throws std::invalid_argument if any stage produces a non-finite value.
double apply_value_mappers(double value, std::span<const ValueMapper> mappers);

/// Cast a finite value to an unsigned byte via truncation and modulo 256.
///
/// @throws std::invalid_argument if `value` is NaN or infinity.
std::uint8_t unsigned_mod_256(double value);

/// Apply mappers to one byte and return the wrapped byte result.
std::uint8_t map_byte_to_wrapped(std::uint8_t byte,
                                 std::span<const ValueMapper> mappers = {});

/// Apply mappers to one byte and convert the result to a scalar in [-1, 1).
float map_byte_to_scalar(std::uint8_t byte,
                         std::span<const ValueMapper> mappers = {});

/// Apply mappers to a byte span and return wrapped byte values.
std::vector<std::uint8_t> map_bytes_to_wrapped(
    std::span<const std::uint8_t> bytes,
    std::span<const ValueMapper> mappers = {});

/// Convert one byte to the scalar domain using `byte / 128 - 1`.
float byte_to_scalar(std::uint8_t byte);

/// Convert a byte span to scalar values after applying optional mappers.
std::vector<float> map_bytes_to_scalars(
    std::span<const std::uint8_t> bytes,
    std::span<const ValueMapper> mappers = {});

/// Compute the absolute difference between two bytes.
std::uint8_t adjacent_delta(std::uint8_t left, std::uint8_t right);

/// Compute every adjacent byte delta in a byte span.
std::vector<std::uint8_t> adjacent_deltas(std::span<const std::uint8_t> bytes);

/// Find the sliding window with the largest adjacent-byte delta.
WindowEntropy find_highest_entropy_window(
    std::span<const std::uint8_t> bytes,
    std::size_t window_bytes = kDefaultWindowBytes);

/// Return the largest absolute adjacent scalar delta.
float max_abs_scalar_delta(std::span<const float> scalars);

/// Compute an X scale such that adjacent slopes do not exceed `max_slope`.
///
/// @throws std::invalid_argument if `max_slope` is not positive.
float compute_x_scale(std::span<const float> scalars,
                      float max_slope = kDefaultMaxSlope);

/// Fit either a sine or cosine phase against sampled scalar data.
///
/// @throws std::invalid_argument if `wave_scale` is not positive and finite.
PhaseFit fit_wave_phase(std::span<const float> scalars,
                        bool cosine,
                        double wave_scale = kDefaultWaveScale,
                        double tolerance = kDefaultWaveTolerance,
                        int phase_steps = 720,
                        std::size_t max_test_points = 65536);

/// Read an entire file into memory.
///
/// @throws std::runtime_error if the path cannot be opened or read.
std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path);

/// Analyze a file by selecting its highest-entropy window and fitting waves.
///
/// @throws std::runtime_error for file I/O failures.
/// @throws std::invalid_argument for invalid mapper or fit parameters.
Analysis analyze_file(const std::filesystem::path& path,
                      std::size_t window_bytes = kDefaultWindowBytes,
                      float max_slope = kDefaultMaxSlope,
                      double wave_scale = kDefaultWaveScale,
                      double wave_tolerance = kDefaultWaveTolerance,
                      int phase_steps = 720,
                      std::size_t max_phase_test_points = 65536,
                      std::span<const ValueMapper> mappers = {});

}  // namespace thrystr
