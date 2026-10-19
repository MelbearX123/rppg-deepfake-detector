#pragma once

#include "rppg/pulse/pulse.hpp"
#include "rppg/regions/regions.hpp"

#include <array>
#include <optional>
#include <vector>

namespace rppg {

struct WindowEstimate {
    double center_sec = 0.0;
    double peak_bpm = 0.0;
    double smoothed_bpm = 0.0;
    double snr_db = 0.0;
};

struct RegionHeartRate {
    std::vector<WindowEstimate> windows;
    double bpm = 0.0;
    double snr_db = 0.0;
};

struct HeartRates {
    std::array<std::optional<RegionHeartRate>, kRegionCount> regions;
    std::optional<double> bpm;
};

std::vector<WindowEstimate> estimate_windows(const std::vector<double>& pulse, double start_sec,
                                             double rate_hz);

HeartRates estimate_heart_rates(const RegionPulses& pulses);

}  // namespace rppg
