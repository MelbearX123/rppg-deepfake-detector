#pragma once

#include <cstddef>
#include <vector>

namespace rppg {

struct TimingStats {
    std::size_t frame_count = 0;
    double duration_sec = 0.0;
    double median_interval_sec = 0.0;
    double measured_fps = 0.0;
    double interval_variation = 0.0;
    bool is_variable_frame_rate = false;
    std::size_t non_increasing_count = 0;
};

TimingStats analyze_timing(const std::vector<double>& timestamps_sec);

}  // namespace rppg
