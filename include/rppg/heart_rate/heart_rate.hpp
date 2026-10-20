#pragma once

#include "rppg/pulse/pulse.hpp"
#include "rppg/regions/regions.hpp"

#include <array>
#include <cstddef>
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

using RegionHeartRates = std::array<std::optional<RegionHeartRate>, kRegionCount>;

struct FaceWindow {
    double center_sec = 0.0;
    double bpm = 0.0;
    double deviation_bpm = 0.0;
    double snr_db = 0.0;
    std::size_t regions = 0;
    bool trusted = false;
};

struct HeartRates {
    RegionHeartRates regions;
    std::vector<FaceWindow> face_windows;
    std::optional<double> bpm;
};

std::vector<WindowEstimate> estimate_windows(const std::vector<double>& pulse, double start_sec,
                                             double rate_hz);

std::vector<FaceWindow> combine_regions(const RegionHeartRates& regions);

HeartRates estimate_heart_rates(const RegionPulses& pulses);

}  // namespace rppg
