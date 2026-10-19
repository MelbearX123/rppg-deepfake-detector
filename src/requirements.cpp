#include "rppg/requirements.hpp"

#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <iomanip>
#include <sstream>

namespace rppg {

RequirementResult check_face_requirements(const TimingStats& timing, double tracked_duration_sec,
                                          const std::vector<double>& face_sizes_px) {
    std::ostringstream reason;
    reason << std::fixed;

    if (timing.duration_sec < limits::kMinDurationSec) {
        reason << std::setprecision(2) << "video is " << timing.duration_sec << " s, minimum is "
               << std::setprecision(0) << limits::kMinDurationSec << " s";
        return {RequirementStatus::kTooShort, reason.str()};
    }
    if (face_sizes_px.empty()) {
        return {RequirementStatus::kNoFace, "no face found"};
    }
    if (tracked_duration_sec < limits::kMinDurationSec) {
        reason << std::setprecision(2) << "face tracked for at most " << tracked_duration_sec
               << " s in a row, minimum is " << std::setprecision(0) << limits::kMinDurationSec
               << " s";
        return {RequirementStatus::kTrackTooShort, reason.str()};
    }
    const double face_size = median(face_sizes_px);
    if (face_size < limits::kMinFaceSizePx) {
        reason << std::setprecision(0) << "face is " << face_size << " px, minimum is "
               << limits::kMinFaceSizePx << " px";
        return {RequirementStatus::kFaceTooSmall, reason.str()};
    }
    return {RequirementStatus::kOk, ""};
}

RequirementResult check_region_requirements(std::size_t pulse_regions) {
    if (pulse_regions == 0) {
        return {RequirementStatus::kNoUsableRegion, "no skin region gave a pulse signal"};
    }
    return {RequirementStatus::kOk, ""};
}

}  // namespace rppg
