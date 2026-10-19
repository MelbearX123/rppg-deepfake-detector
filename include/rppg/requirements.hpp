#pragma once

#include "rppg/video/timing.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rppg {

enum class RequirementStatus {
    kOk,
    kTooShort,
    kNoFace,
    kTrackTooShort,
    kFaceTooSmall,
    kNoUsableRegion,
    kInconclusive,
};

struct RequirementResult {
    RequirementStatus status = RequirementStatus::kOk;
    std::string reason;
};

RequirementResult check_face_requirements(const TimingStats& timing, double tracked_duration_sec,
                                          const std::vector<double>& face_sizes_px);

RequirementResult check_region_requirements(std::size_t pulse_regions);

RequirementResult check_heart_rate(const std::optional<double>& face_bpm);

}  // namespace rppg
