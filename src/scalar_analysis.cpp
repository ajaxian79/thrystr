#include <thrystr/scalar_analysis.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#if defined(__unix__)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace thrystr {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr std::size_t kScanChunkBytes = 8u * 1024u * 1024u;

struct WindowScanResult {
    WindowEntropy window;
    std::size_t source_size = 0;
    std::size_t window_count = 0;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> mapped_bytes;
};

std::uint8_t delta_at(std::span<const std::uint8_t> bytes, std::size_t index) {
    return adjacent_delta(bytes[index], bytes[index + 1]);
}

std::size_t file_size_or_throw(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("could not determine file size: " + path.string());
    }
    if (file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("file too large for this build: " + path.string());
    }
    return static_cast<std::size_t>(file_size);
}

bool has_enabled_mappers(std::span<const ValueMapper> mappers) {
    return std::any_of(mappers.begin(), mappers.end(), [](const ValueMapper& mapper) {
        return mapper.enabled;
    });
}

void copy_window_from_ring(std::vector<std::uint8_t>& output,
                           const std::vector<std::uint8_t>& ring,
                           std::size_t offset,
                           std::size_t length) {
    output.resize(length);
    for (std::size_t i = 0; i < length; ++i) {
        output[i] = ring[(offset + i) % length];
    }
}

WindowScanResult scan_highest_entropy_window_stream(const std::filesystem::path& path,
                                                    std::size_t window_bytes,
                                                    std::span<const ValueMapper> mappers) {
    WindowScanResult result;
    result.source_size = file_size_or_throw(path);
    if (result.source_size == 0 || window_bytes == 0) {
        return result;
    }

    const std::size_t window_length = std::min(window_bytes, result.source_size);
    result.window = WindowEntropy{0, window_length, 0, 0};
    result.window_count = result.source_size - window_length + 1u;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    if (window_length == 1u) {
        char byte = 0;
        input.read(&byte, 1);
        if (!input) {
            throw std::runtime_error("could not read file: " + path.string());
        }
        const std::uint8_t source_byte = static_cast<std::uint8_t>(byte);
        result.bytes = {source_byte};
        result.mapped_bytes = {
            has_enabled_mappers(mappers) ? map_byte_to_wrapped(source_byte, mappers)
                                         : source_byte,
        };
        return result;
    }

    std::vector<std::uint8_t> source_ring(window_length);
    std::vector<std::uint8_t> mapped_ring(window_length);
    const std::size_t delta_span = window_length > 1u ? window_length - 1u : 0u;
    std::vector<std::uint8_t> delta_ring(delta_span == 0u ? 1u : delta_span);
    std::array<std::uint32_t, 256> delta_counts{};
    std::vector<char> chunk(kScanChunkBytes);
    const bool map_values = has_enabled_mappers(mappers);

    std::size_t index = 0;
    std::uint8_t previous_mapped = 0;
    std::uint8_t current_max_delta = 0;
    bool have_previous = false;
    bool have_best_window = false;
    bool stop_scanning = false;

    while (input && !stop_scanning) {
        input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        const std::streamsize read_count = input.gcount();
        if (read_count < 0) {
            throw std::runtime_error("could not read file: " + path.string());
        }

        for (std::streamsize i = 0; i < read_count && !stop_scanning; ++i) {
            const std::uint8_t source_byte = static_cast<std::uint8_t>(chunk[static_cast<std::size_t>(i)]);
            const std::uint8_t mapped_byte = map_values
                ? map_byte_to_wrapped(source_byte, mappers)
                : source_byte;
            source_ring[index % window_length] = source_byte;
            mapped_ring[index % window_length] = mapped_byte;

            if (have_previous) {
                const std::size_t delta_index = index - 1u;
                const std::uint8_t delta = adjacent_delta(previous_mapped, mapped_byte);
                if (delta_index >= delta_span) {
                    const std::size_t expired_index = delta_index - delta_span;
                    const std::uint8_t expired_delta =
                        delta_ring[expired_index % delta_span];
                    --delta_counts[expired_delta];
                    while (current_max_delta > 0u &&
                           delta_counts[current_max_delta] == 0u) {
                        --current_max_delta;
                    }
                }
                delta_ring[delta_index % delta_span] = delta;
                ++delta_counts[delta];
                current_max_delta = std::max(current_max_delta, delta);

                if (delta_index + 1u >= delta_span) {
                    const std::size_t window_offset = delta_index + 1u - delta_span;
                    const std::uint8_t max_delta = current_max_delta;
                    if (!have_best_window || max_delta > result.window.max_delta) {
                        std::size_t max_delta_index = window_offset;
                        const std::size_t max_delta_end = window_offset + delta_span;
                        for (std::size_t candidate = window_offset;
                             candidate < max_delta_end;
                             ++candidate) {
                            if (delta_ring[candidate % delta_span] == max_delta) {
                                max_delta_index = candidate;
                                break;
                            }
                        }
                        result.window = WindowEntropy{
                            window_offset,
                            window_length,
                            max_delta,
                            max_delta_index,
                        };
                        copy_window_from_ring(result.bytes,
                                              source_ring,
                                              window_offset,
                                              window_length);
                        copy_window_from_ring(result.mapped_bytes,
                                              mapped_ring,
                                              window_offset,
                                              window_length);
                        have_best_window = true;
                        stop_scanning =
                            max_delta == std::numeric_limits<std::uint8_t>::max();
                    }
                }
            }

            previous_mapped = mapped_byte;
            have_previous = true;
            ++index;
        }
    }

    if (!stop_scanning && !input.eof()) {
        throw std::runtime_error("could not read file: " + path.string());
    }
    if (!stop_scanning && index != result.source_size) {
        throw std::runtime_error("file size changed while reading: " + path.string());
    }

    if (!have_best_window && result.source_size == 1u) {
        result.bytes = {source_ring[0]};
        result.mapped_bytes = {mapped_ring[0]};
    }

    return result;
}

#if defined(__unix__)
WindowScanResult scan_highest_entropy_window_mapped(const std::filesystem::path& path,
                                                    std::size_t window_bytes) {
    WindowScanResult result;
    result.source_size = file_size_or_throw(path);
    if (result.source_size == 0 || window_bytes == 0) {
        return result;
    }

    const std::size_t window_length = std::min(window_bytes, result.source_size);
    result.window = WindowEntropy{0, window_length, 0, 0};
    result.window_count = result.source_size - window_length + 1u;

    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    void* mapping = mmap(nullptr, result.source_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) {
        return scan_highest_entropy_window_stream(path, window_bytes, {});
    }

#if defined(MADV_SEQUENTIAL)
    madvise(mapping, result.source_size, MADV_SEQUENTIAL);
#endif

    const auto* bytes = static_cast<const std::uint8_t*>(mapping);
    if (window_length == 1u) {
        result.bytes = {bytes[0]};
        result.mapped_bytes = result.bytes;
        munmap(mapping, result.source_size);
        return result;
    }

    const std::size_t delta_span = window_length - 1u;
    std::vector<std::uint8_t> delta_ring(delta_span);
    std::array<std::uint32_t, 256> delta_counts{};
    std::uint8_t current_max_delta = 0;
    bool have_best_window = false;

    for (std::size_t delta_index = 0; delta_index + 1u < result.source_size;
         ++delta_index) {
        if (delta_index >= delta_span) {
            const std::size_t expired_index = delta_index - delta_span;
            const std::uint8_t expired_delta = delta_ring[expired_index % delta_span];
            --delta_counts[expired_delta];
            while (current_max_delta > 0u && delta_counts[current_max_delta] == 0u) {
                --current_max_delta;
            }
        }

        const int diff = static_cast<int>(bytes[delta_index]) -
                         static_cast<int>(bytes[delta_index + 1u]);
        const std::uint8_t delta = static_cast<std::uint8_t>(std::abs(diff));
        delta_ring[delta_index % delta_span] = delta;
        ++delta_counts[delta];
        current_max_delta = std::max(current_max_delta, delta);

        if (delta_index + 1u < delta_span) {
            continue;
        }

        const std::size_t window_offset = delta_index + 1u - delta_span;
        const std::uint8_t max_delta = current_max_delta;
        if (!have_best_window || max_delta > result.window.max_delta) {
            std::size_t max_delta_index = window_offset;
            const std::size_t max_delta_end = window_offset + delta_span;
            for (std::size_t candidate = window_offset;
                 candidate < max_delta_end;
                 ++candidate) {
                if (delta_ring[candidate % delta_span] == max_delta) {
                    max_delta_index = candidate;
                    break;
                }
            }
            result.window = WindowEntropy{
                window_offset,
                window_length,
                max_delta,
                max_delta_index,
            };
            have_best_window = true;
            if (max_delta == std::numeric_limits<std::uint8_t>::max()) {
                break;
            }
        }
    }

    result.bytes.assign(bytes + result.window.offset,
                        bytes + result.window.offset + window_length);
    result.mapped_bytes = result.bytes;
    munmap(mapping, result.source_size);
    return result;
}
#endif

WindowScanResult scan_highest_entropy_window(const std::filesystem::path& path,
                                             std::size_t window_bytes,
                                             std::span<const ValueMapper> mappers) {
    if (!has_enabled_mappers(mappers)) {
#if defined(__unix__)
        return scan_highest_entropy_window_mapped(path, window_bytes);
#else
        return scan_highest_entropy_window_stream(path, window_bytes, {});
#endif
    }
    return scan_highest_entropy_window_stream(path, window_bytes, mappers);
}

}  // namespace

const char* mapper_kind_name(ValueMapperKind kind) {
    switch (kind) {
    case ValueMapperKind::Add:
        return "add";
    case ValueMapperKind::Subtract:
        return "subtract";
    case ValueMapperKind::Multiply:
        return "multiply";
    case ValueMapperKind::Divide:
        return "divide";
    }
    return "unknown";
}

double apply_value_mapper(double value, const ValueMapper& mapper) {
    if (!mapper.enabled) {
        return value;
    }

    switch (mapper.kind) {
    case ValueMapperKind::Add:
        return value + mapper.operand;
    case ValueMapperKind::Subtract:
        return value - mapper.operand;
    case ValueMapperKind::Multiply:
        return value * mapper.operand;
    case ValueMapperKind::Divide:
        if (std::abs(mapper.operand) <= 1.0e-12) {
            throw std::invalid_argument("divide mapper operand must not be zero");
        }
        return value / mapper.operand;
    }
    return value;
}

double apply_value_mappers(double value, std::span<const ValueMapper> mappers) {
    for (const ValueMapper& mapper : mappers) {
        value = apply_value_mapper(value, mapper);
        if (!std::isfinite(value)) {
            throw std::invalid_argument("mapper stack produced a non-finite value");
        }
    }
    return value;
}

std::uint8_t unsigned_mod_256(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("cannot wrap non-finite value");
    }

    const double truncated = std::trunc(value);
    double wrapped = std::fmod(truncated, 256.0);
    if (wrapped < 0.0) {
        wrapped += 256.0;
    }
    return static_cast<std::uint8_t>(wrapped);
}

std::uint8_t map_byte_to_wrapped(std::uint8_t byte,
                                 std::span<const ValueMapper> mappers) {
    const double mapped = apply_value_mappers(static_cast<double>(byte), mappers);
    return unsigned_mod_256(mapped);
}

float map_byte_to_scalar(std::uint8_t byte,
                         std::span<const ValueMapper> mappers) {
    return byte_to_scalar(map_byte_to_wrapped(byte, mappers));
}

std::vector<std::uint8_t> map_bytes_to_wrapped(
    std::span<const std::uint8_t> bytes,
    std::span<const ValueMapper> mappers) {
    std::vector<std::uint8_t> mapped;
    mapped.reserve(bytes.size());
    for (const std::uint8_t byte : bytes) {
        mapped.push_back(map_byte_to_wrapped(byte, mappers));
    }
    return mapped;
}

float byte_to_scalar(std::uint8_t byte) {
    return static_cast<float>(byte) / 128.0f - 1.0f;
}

std::vector<float> map_bytes_to_scalars(std::span<const std::uint8_t> bytes,
                                        std::span<const ValueMapper> mappers) {
    std::vector<float> scalars;
    scalars.reserve(bytes.size());
    for (const std::uint8_t byte : bytes) {
        scalars.push_back(map_byte_to_scalar(byte, mappers));
    }
    return scalars;
}

std::uint8_t adjacent_delta(std::uint8_t left, std::uint8_t right) {
    const int diff = static_cast<int>(left) - static_cast<int>(right);
    return static_cast<std::uint8_t>(std::abs(diff));
}

std::vector<std::uint8_t> adjacent_deltas(std::span<const std::uint8_t> bytes) {
    std::vector<std::uint8_t> deltas;
    if (bytes.size() < 2) {
        return deltas;
    }

    deltas.reserve(bytes.size() - 1);
    for (std::size_t i = 0; i + 1 < bytes.size(); ++i) {
        deltas.push_back(adjacent_delta(bytes[i], bytes[i + 1]));
    }
    return deltas;
}

WindowEntropy find_highest_entropy_window(std::span<const std::uint8_t> bytes,
                                          std::size_t window_bytes) {
    if (bytes.empty()) {
        return {};
    }

    window_bytes = std::min(window_bytes, bytes.size());
    if (window_bytes < 2) {
        return WindowEntropy{0, window_bytes, 0, 0};
    }

    const std::size_t delta_span = window_bytes - 1;
    const std::size_t delta_count = bytes.size() - 1;
    const std::size_t window_count = bytes.size() - window_bytes + 1;

    WindowEntropy best{0, window_bytes, 0, 0};
    std::deque<std::size_t> max_queue;

    for (std::size_t right = 0; right < delta_count; ++right) {
        const std::uint8_t current = delta_at(bytes, right);
        while (!max_queue.empty() && delta_at(bytes, max_queue.back()) <= current) {
            max_queue.pop_back();
        }
        max_queue.push_back(right);

        if (right + 1 < delta_span) {
            continue;
        }

        const std::size_t window_offset = right + 1 - delta_span;
        while (!max_queue.empty() && max_queue.front() < window_offset) {
            max_queue.pop_front();
        }
        if (window_offset >= window_count) {
            continue;
        }

        const std::uint8_t max_delta = delta_at(bytes, max_queue.front());
        if (max_delta > best.max_delta) {
            best.offset = window_offset;
            best.length = window_bytes;
            best.max_delta = max_delta;
            best.delta_index = max_queue.front();
        }
    }

    return best;
}

float max_abs_scalar_delta(std::span<const float> scalars) {
    float max_delta = 0.0f;
    for (std::size_t i = 0; i + 1 < scalars.size(); ++i) {
        max_delta = std::max(max_delta, std::abs(scalars[i + 1] - scalars[i]));
    }
    return max_delta;
}

float compute_x_scale(std::span<const float> scalars, float max_slope) {
    if (max_slope <= 0.0f) {
        throw std::invalid_argument("max_slope must be positive");
    }
    const float max_delta = max_abs_scalar_delta(scalars);
    return std::max(1.0f, max_delta / max_slope);
}

PhaseFit fit_wave_phase(std::span<const float> scalars,
                        bool cosine,
                        double wave_scale,
                        double tolerance,
                        int phase_steps,
                        std::size_t max_test_points) {
    PhaseFit best{};
    best.tolerance = tolerance;
    if (scalars.empty() || phase_steps <= 0 || max_test_points == 0) {
        return best;
    }
    if (!std::isfinite(wave_scale) || wave_scale <= 0.0) {
        throw std::invalid_argument("wave scale must be positive and finite");
    }

    const std::size_t stride = std::max<std::size_t>(1, scalars.size() / max_test_points);
    const double denom = scalars.size() > 1
        ? static_cast<double>(scalars.size() - 1)
        : 1.0;

    for (int step = 0; step < phase_steps; ++step) {
        const double phase = (2.0 * kPi * static_cast<double>(step)) /
                             static_cast<double>(phase_steps);
        std::size_t hits = 0;
        std::size_t tested = 0;

        for (std::size_t i = 0; i < scalars.size(); i += stride) {
            const double theta = (2.0 * kPi * wave_scale *
                                  static_cast<double>(i) / denom) + phase;
            const double wave = cosine ? std::cos(theta) : std::sin(theta);
            if (std::abs(wave - static_cast<double>(scalars[i])) <= tolerance) {
                ++hits;
            }
            ++tested;
        }

        if (hits > best.hits) {
            best.phase_radians = phase;
            best.hits = hits;
            best.tested_points = tested;
        }
    }

    return best;
}

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    const std::streamsize size = input.tellg();
    if (size < 0) {
        throw std::runtime_error("could not determine file size: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty() &&
        !input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("could not read file: " + path.string());
    }

    return bytes;
}

Analysis analyze_file(const std::filesystem::path& path,
                      std::size_t window_bytes,
                      float max_slope,
                      double wave_scale,
                      double wave_tolerance,
                      int phase_steps,
                      std::size_t max_phase_test_points,
                      std::span<const ValueMapper> mappers) {
    const WindowScanResult scan =
        scan_highest_entropy_window(path, window_bytes, mappers);

    Analysis analysis;
    analysis.source_path = path;
    analysis.source_size = scan.source_size;
    analysis.mappers.assign(mappers.begin(), mappers.end());
    analysis.window = scan.window;
    analysis.window_count = scan.window_count;
    analysis.bytes = scan.bytes;
    analysis.mapped_bytes = scan.mapped_bytes;
    analysis.scalars = map_bytes_to_scalars(analysis.mapped_bytes);
    analysis.max_abs_scalar_delta = max_abs_scalar_delta(analysis.scalars);
    analysis.x_scale = compute_x_scale(analysis.scalars, max_slope);
    analysis.wave_scale = wave_scale;
    analysis.sine = fit_wave_phase(analysis.scalars, false, wave_scale, wave_tolerance,
                                   phase_steps, max_phase_test_points);
    analysis.cosine = fit_wave_phase(analysis.scalars, true, wave_scale, wave_tolerance,
                                     phase_steps, max_phase_test_points);

    if (analysis.window.delta_index >= analysis.window.offset) {
        analysis.max_delta_sample_index = analysis.window.delta_index - analysis.window.offset;
    }

    return analysis;
}

}  // namespace thrystr
