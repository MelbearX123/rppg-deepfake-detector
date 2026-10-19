#pragma once

#include "rppg/timing.hpp"

#include <cstddef>
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
};

struct RequirementResult {
    RequirementStatus status = RequirementStatus::kOk;
    std::string reason;
};

RequirementResult check_face_requirements(const TimingStats& timing, double tracked_duration_sec,
                                          const std::vector<double>& face_sizes_px);

RequirementResult check_region_requirements(std::size_t pulse_regions);

}  // namespace rppg
