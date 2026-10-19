#pragma once

#include <cstddef>

namespace rppg::limits {

inline constexpr double kMinDurationSec = 10.0;
inline constexpr int kMinFaceSizePx = 100;
inline constexpr double kHeavyCompressionBpp = 0.1;
inline constexpr double kVariableFrameRateTolerance = 0.05;

inline constexpr int kDetectionWidthPx = 640;

inline constexpr double kMinTrackOverlap = 0.3;

inline constexpr double kMaxGapSec = 0.5;

inline constexpr double kSmoothingWindowSec = 0.3;

inline constexpr double kSkinModelSampleIntervalSec = 1.0;
inline constexpr std::size_t kSkinSamplesPerRegion = 500;
inline constexpr double kSkinToleranceMads = 3.0;

inline constexpr double kMinSkinBrightnessRatio = 0.5;

inline constexpr double kTrimFraction = 0.1;

inline constexpr double kMinRegionCoverage = 0.5;

}  // namespace rppg::limits
