#pragma once

#include <string>
#include <vector>

namespace rppg {

struct GroundTruth {
    std::vector<double> ppg;
    std::vector<double> displayed_bpm;
    std::vector<double> times_sec;
};

GroundTruth read_ubfc_ground_truth(const std::string& path);

double waveform_bpm(const GroundTruth& truth, double center_sec, double half_window_sec);

}  // namespace rppg
