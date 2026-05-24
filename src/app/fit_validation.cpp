// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <thrystr/app/fit_validation.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace thrystr::app {
namespace {

void fail(ValidationReport& report,
          std::string message,
          std::size_t track_index = 0,
          std::size_t section_index = 0,
          std::size_t sample_index = 0) {
    report.pass = false;
    report.issues.push_back(ValidationIssue{
        std::move(message),
        track_index,
        section_index,
        sample_index,
    });
}

void record_residual(ValidationReport& report, double residual) {
    report.max_residual = std::max(report.max_residual, residual);
    report.mean_residual += residual;
    ++report.checked_samples;
}

void finalize_mean(ValidationReport& report) {
    if (report.checked_samples > 0u) {
        report.mean_residual /= static_cast<double>(report.checked_samples);
    }
}

const Section* containing_section(std::span<const Section> sections,
                                  std::size_t index,
                                  std::size_t* section_index) {
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (section_contains(sections[i], index)) {
            if (section_index) {
                *section_index = i;
            }
            return &sections[i];
        }
    }
    return nullptr;
}

}  // namespace

ValidationReport validate_sections(std::span<const float> scalars,
                                   std::span<const Section> sections) {
    ValidationReport report;
    std::size_t expected_start = 0;
    for (std::size_t section_index = 0; section_index < sections.size(); ++section_index) {
        const Section& section = sections[section_index];
        if (section.length == 0u) {
            fail(report, "section has zero length", 0, section_index, expected_start);
            continue;
        }
        if (section.start_index != expected_start) {
            fail(report, "section table has a gap or overlap", 0, section_index, expected_start);
            expected_start = section.start_index;
        }
        if (section_end(section) > scalars.size()) {
            fail(report, "section extends past scalar count", 0, section_index, scalars.size());
            continue;
        }
        for (std::size_t index = section.start_index; index < section_end(section); ++index) {
            const double residual =
                std::abs(wave_value_at_index(section, index) -
                         static_cast<double>(scalars[index]));
            record_residual(report, residual);
            if (residual > section.fit_tolerance) {
                fail(report, "section residual exceeds tolerance", 0, section_index, index);
            }
        }
        expected_start = static_cast<std::size_t>(section_end(section));
    }
    if (expected_start != scalars.size()) {
        fail(report, "sections do not cover the scalar range", 0, sections.size(), expected_start);
    }
    finalize_mean(report);
    return report;
}

ValidationReport validate_tracks(std::span<const float> scalars,
                                 const WorkspaceModel& workspace,
                                 double parity_margin) {
    ValidationReport report;
    if (workspace.tracks.empty() || workspace.tracks.size() > kMaxTracks) {
        fail(report, "track count is outside the supported range");
        finalize_mean(report);
        return report;
    }

    std::size_t parity_count = 0;
    std::size_t parity_index = std::numeric_limits<std::size_t>::max();
    std::vector<std::uint8_t> owner(scalars.size(), kNoParityTrack);
    for (std::size_t track_index = 0; track_index < workspace.tracks.size(); ++track_index) {
        const Track& track = workspace.tracks[track_index];
        if (track.kind == TrackKind::Parity) {
            ++parity_count;
            parity_index = track_index;
            continue;
        }
        if (track.owned_mask.size() != owned_mask_size(scalars.size())) {
            fail(report, "owned mask length does not match scalar count", track_index);
            continue;
        }
        for (std::size_t index = 0; index < scalars.size(); ++index) {
            if (!get_owned_bit(track.owned_mask, index)) {
                continue;
            }
            if (owner[index] != kNoParityTrack) {
                fail(report, "owned masks overlap", track_index, 0, index);
            }
            owner[index] = track.id;
        }
    }

    if (parity_count > 1u) {
        fail(report, "more than one parity track is present");
    }
    if ((workspace.parity_track_id == kNoParityTrack) != (parity_count == 0u)) {
        fail(report, "parity track id does not match parity-track presence");
    }

    for (std::size_t index = 0; index < owner.size(); ++index) {
        if (owner[index] == kNoParityTrack) {
            fail(report, "sample index is not owned by any data track", 0, 0, index);
        }
    }

    for (std::size_t track_index = 0; track_index < workspace.tracks.size(); ++track_index) {
        const Track& track = workspace.tracks[track_index];
        if (track.kind != TrackKind::Data) {
            continue;
        }
        for (std::size_t index = 0; index < scalars.size(); ++index) {
            if (!get_owned_bit(track.owned_mask, index)) {
                continue;
            }
            std::size_t section_index = 0;
            const Section* section =
                containing_section(track.sections, index, &section_index);
            if (!section) {
                fail(report, "owned sample is not covered by a data section",
                     track_index, 0, index);
                continue;
            }
            const double residual =
                std::abs(wave_value_at_index(*section, index) -
                         static_cast<double>(scalars[index]));
            record_residual(report, residual);
            if (residual > section->fit_tolerance) {
                fail(report, "data track residual exceeds tolerance",
                     track_index, section_index, index);
            }
        }
    }

    if (parity_index != std::numeric_limits<std::size_t>::max()) {
        const Track& parity_track = workspace.tracks[parity_index];
        for (std::size_t index = 0; index < scalars.size(); ++index) {
            std::size_t section_index = 0;
            const Section* section =
                containing_section(parity_track.sections, index, &section_index);
            if (!section) {
                fail(report, "sample is not covered by parity track",
                     parity_index, 0, index);
                continue;
            }
            const double expected = static_cast<double>(owner[index]);
            const double residual = std::abs(wave_value_at_index(*section, index) - expected);
            if (residual > 0.5 - parity_margin) {
                fail(report, "parity routing residual exceeds rounding margin",
                     parity_index, section_index, index);
            }
        }
    }

    finalize_mean(report);
    return report;
}

}  // namespace thrystr::app
