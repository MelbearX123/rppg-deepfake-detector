#pragma once

#include "rppg/face_track.hpp"
#include "rppg/pulse.hpp"
#include "rppg/region_signal.hpp"

#include <string>
#include <vector>

namespace rppg {

void write_signals_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                       const RegionSignals& signals, const Segment& segment);

void write_pulse_csv(const std::string& path, const RegionPulses& pulses);

}  // namespace rppg
