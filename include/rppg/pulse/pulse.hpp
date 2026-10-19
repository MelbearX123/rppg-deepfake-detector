#pragma once

#include "rppg/face/face_track.hpp"
#include "rppg/regions/pixel_stats.hpp"
#include "rppg/regions/region_signal.hpp"
#include "rppg/regions/regions.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace rppg {

struct RegionPulses {
    double start_sec = 0.0;
    double rate_hz = 0.0;
    std::array<std::optional<std::vector<double>>, kRegionCount> pulses;
};

std::optional<std::vector<Color>> resample_colors(const RegionSeries& series,
                                                  const std::vector<double>& timestamps_sec,
                                                  const Segment& segment, double rate_hz,
                                                  double max_gap_sec);

std::vector<double> pos_pulse(const std::vector<Color>& colors, std::size_t window);

RegionPulses extract_pulses(const RegionSignals& signals, const std::vector<double>& timestamps_sec,
                            const Segment& segment);

RegionPulses bandpass_pulses(const RegionPulses& pulses);

}  // namespace rppg
