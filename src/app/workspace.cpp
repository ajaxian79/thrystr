// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/workspace.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
#include <quadmath.h>
#endif

namespace thrystr::app {
namespace {

Scalar scalar_sin(Scalar value) {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    return sinq(value);
#else
    return std::sin(value);
#endif
}

Scalar scalar_pi() {
#if defined(__SIZEOF_FLOAT128__) && !defined(_MSC_VER)
    static const Scalar pi = strtoflt128("3.141592653589793238462643383279502884", nullptr);
    return pi;
#else
    return std::numbers::pi_v<Scalar>;
#endif
}

} // namespace

std::uint64_t section_end(const Section& section) {
    return static_cast<std::uint64_t>(section.start_index) +
           static_cast<std::uint64_t>(section.length);
}

bool section_contains(const Section& section, std::size_t index) {
    return index >= section.start_index && index < section_end(section);
}

Scalar wave_value_at_index(const Section& section, std::size_t index) {
    if (section.wave_amplitude == 0.0) {
        return static_cast<Scalar>(section.wave_amplitude_offset);
    }

    const Scalar spacing =
        std::max(static_cast<Scalar>(1.0e-12), static_cast<Scalar>(section.section_spacing_nm));
    const Scalar wavelength =
        std::max(static_cast<Scalar>(1.0e-12), static_cast<Scalar>(section.wave_wavelength_nm));
    const Scalar local_index =
        static_cast<Scalar>(index >= section.start_index ? index - section.start_index : 0u);
    const Scalar x_nm = local_index * spacing;
    const Scalar theta = static_cast<Scalar>(2.0) * scalar_pi() *
                         (x_nm - static_cast<Scalar>(section.wave_phase_nm)) / wavelength;
    return static_cast<Scalar>(section.wave_amplitude_offset) +
           static_cast<Scalar>(section.wave_amplitude) *
               ((scalar_sin(theta) + static_cast<Scalar>(1.0)) * static_cast<Scalar>(0.5));
}

std::size_t owned_mask_size(std::size_t sample_count) { return (sample_count + 7u) / 8u; }

void reset_owned_mask(std::vector<std::uint8_t>& mask, std::size_t sample_count) {
    mask.assign(owned_mask_size(sample_count), 0u);
}

bool get_owned_bit(std::span<const std::uint8_t> mask, std::size_t index) {
    const std::size_t byte_index = index / 8u;
    if (byte_index >= mask.size()) {
        return false;
    }
    const auto bit = static_cast<std::uint8_t>(1u << (index % 8u));
    return (mask[byte_index] & bit) != 0u;
}

void set_owned_bit(std::span<std::uint8_t> mask, std::size_t index, bool value) {
    const std::size_t byte_index = index / 8u;
    if (byte_index >= mask.size()) {
        return;
    }
    const auto bit = static_cast<std::uint8_t>(1u << (index % 8u));
    if (value) {
        mask[byte_index] = static_cast<std::uint8_t>(mask[byte_index] | bit);
    } else {
        mask[byte_index] = static_cast<std::uint8_t>(mask[byte_index] & ~bit);
    }
}

std::size_t popcount_owned(std::span<const std::uint8_t> mask) {
    std::size_t count = 0;
    for (const std::uint8_t byte : mask) {
        count += static_cast<std::size_t>(std::popcount(byte));
    }
    return count;
}

std::size_t first_owned_index(std::span<const std::uint8_t> mask, std::size_t sample_count) {
    for (std::size_t index = 0; index < sample_count; ++index) {
        if (get_owned_bit(mask, index)) {
            return index;
        }
    }
    return sample_count;
}

std::size_t last_owned_index_exclusive(std::span<const std::uint8_t> mask,
                                       std::size_t sample_count) {
    for (std::size_t index = sample_count; index > 0u; --index) {
        if (get_owned_bit(mask, index - 1u)) {
            return index;
        }
    }
    return 0u;
}

Track make_full_data_track(std::uint8_t id, std::size_t sample_count) {
    Track track;
    track.id = id;
    track.kind = TrackKind::Data;
    track.name = "data track " + std::to_string(static_cast<unsigned>(id));
    reset_owned_mask(track.owned_mask, sample_count);
    for (std::size_t index = 0; index < sample_count; ++index) {
        set_owned_bit(track.owned_mask, index, true);
    }
    return track;
}

Section& append_section(Track& track, const Section& section) {
    track.sections.push_back(section);
    return track.sections.back();
}

Track* find_track(WorkspaceModel& workspace, std::uint8_t id) {
    const auto found = std::find_if(workspace.tracks.begin(), workspace.tracks.end(),
                                    [id](const Track& track) { return track.id == id; });
    return found == workspace.tracks.end() ? nullptr : &*found;
}

const Track* find_track(const WorkspaceModel& workspace, std::uint8_t id) {
    const auto found = std::find_if(workspace.tracks.begin(), workspace.tracks.end(),
                                    [id](const Track& track) { return track.id == id; });
    return found == workspace.tracks.end() ? nullptr : &*found;
}

} // namespace thrystr::app
