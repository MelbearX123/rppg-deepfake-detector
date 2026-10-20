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

inline constexpr double kPulseRateHz = 30.0;

inline constexpr double kPosWindowSec = 1.6;

inline constexpr double kMinPulseHz = 0.7;
inline constexpr double kMaxPulseHz = 4.0;
inline constexpr double kFilterPadSec = 2.0;
inline constexpr double kHeartRateWindowSec = 10.0;
inline constexpr double kHeartRateStepSec = 1.0;
inline constexpr double kSpectrumStepBpm = 0.5;
inline constexpr std::size_t kHeartRateSmoothingWindows = 5;
inline constexpr double kSnrHalfWidthHz = 0.1;

// Tuned with scripts/tune_thresholds.py on UBFC subjects 1, 3, 5, 9, 11 (raw + CRF 6/14/23),
// checked on 4, 8, 10, 12: 9 of 16 clips decided there, none off by more than 5 bpm.
inline constexpr double kMaxRegionDeviationBpm = 3.0;
inline constexpr double kMinWindowSnrDb = -3.0;
inline constexpr std::size_t kMinRegionsPerWindow = 2;
inline constexpr double kMinTrustedWindowFraction = 0.4;

}  // namespace rppg::limits
