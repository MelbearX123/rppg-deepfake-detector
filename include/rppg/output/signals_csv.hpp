#pragma once

#include "rppg/face/face_track.hpp"
#include "rppg/heart_rate/heart_rate.hpp"
#include "rppg/pulse/pulse.hpp"
#include "rppg/regions/region_signal.hpp"

#include <string>
#include <vector>

namespace rppg {

struct SavedSignals {
    std::vector<double> timestamps_sec;
    RegionSignals signals;
    Segment segment;
};

void write_signals_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                       const RegionSignals& signals, const Segment& segment);

SavedSignals read_signals_csv(const std::string& path);

void write_pulse_csv(const std::string& path, const RegionPulses& pulses);

void write_heart_rate_csv(const std::string& path, const HeartRates& rates);

}  // namespace rppg
