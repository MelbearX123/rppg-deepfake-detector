#pragma once

#include "rppg/timing.hpp"

#include <string>
#include <vector>

namespace rppg {

enum class RequirementStatus {
    kOk,
    kTooShort,
    kNoFace,
    kFaceTooSmall,
};

struct RequirementResult {
    RequirementStatus status = RequirementStatus::kOk;
    std::string reason;  // empty when status is kOk
};

RequirementResult check_requirements(const TimingStats& timing,
                                     const std::vector<double>& face_sizes_px);

}  // namespace rppg
