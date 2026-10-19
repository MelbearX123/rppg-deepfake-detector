#pragma once

namespace rppg::limits {

inline constexpr double kMinDurationSec = 10.0;
inline constexpr int kMinFaceSizePx = 100;
inline constexpr double kHeavyCompressionBpp = 0.1;
inline constexpr double kVariableFrameRateTolerance = 0.05;

inline constexpr int kDetectionWidthPx = 640;

inline constexpr double kMinTrackOverlap = 0.3;

inline constexpr double kMaxGapSec = 0.5;

inline constexpr double kSmoothingWindowSec = 0.3;

}  // namespace rppg::limits
