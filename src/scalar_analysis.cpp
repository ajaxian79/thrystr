#include <thrystr/scalar_analysis.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <stdexcept>

namespace thrystr {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

std::uint8_t delta_at(std::span<const std::uint8_t> bytes, std::size_t index) {
    return adjacent_delta(bytes[index], bytes[index + 1]);
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
                        double tolerance,
                        int phase_steps,
                        std::size_t max_test_points) {
    PhaseFit best{};
    best.tolerance = tolerance;
    if (scalars.empty() || phase_steps <= 0 || max_test_points == 0) {
        return best;
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
            const double theta = (2.0 * kPi * static_cast<double>(i) / denom) + phase;
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
                      double wave_tolerance,
                      int phase_steps,
                      std::size_t max_phase_test_points,
                      std::span<const ValueMapper> mappers) {
    std::vector<std::uint8_t> source = read_file_bytes(path);
    std::vector<std::uint8_t> mapped_source = map_bytes_to_wrapped(source, mappers);

    Analysis analysis;
    analysis.source_path = path;
    analysis.source_size = source.size();
    analysis.mappers.assign(mappers.begin(), mappers.end());
    analysis.window = find_highest_entropy_window(mapped_source, window_bytes);
    analysis.window_count = mapped_source.empty()
        ? 0
        : mapped_source.size() - analysis.window.length + 1;

    const auto begin = source.begin() + static_cast<std::ptrdiff_t>(analysis.window.offset);
    const auto end = begin + static_cast<std::ptrdiff_t>(analysis.window.length);
    analysis.bytes.assign(begin, end);

    const auto mapped_begin = mapped_source.begin() +
        static_cast<std::ptrdiff_t>(analysis.window.offset);
    const auto mapped_end = mapped_begin +
        static_cast<std::ptrdiff_t>(analysis.window.length);
    analysis.mapped_bytes.assign(mapped_begin, mapped_end);
    analysis.scalars = map_bytes_to_scalars(analysis.mapped_bytes);
    analysis.max_abs_scalar_delta = max_abs_scalar_delta(analysis.scalars);
    analysis.x_scale = compute_x_scale(analysis.scalars, max_slope);
    analysis.sine = fit_wave_phase(analysis.scalars, false, wave_tolerance,
                                   phase_steps, max_phase_test_points);
    analysis.cosine = fit_wave_phase(analysis.scalars, true, wave_tolerance,
                                     phase_steps, max_phase_test_points);

    if (analysis.window.delta_index >= analysis.window.offset) {
        analysis.max_delta_sample_index = analysis.window.delta_index - analysis.window.offset;
    }

    return analysis;
}

}  // namespace thrystr
