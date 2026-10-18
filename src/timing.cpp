#include "rppg/timing.hpp"

#include "rppg/limits.hpp"
#include "rppg/signal.hpp"

#include <cmath>

namespace rppg {

TimingStats analyze_timing(const std::vector<double>& timestamps_sec) {
    TimingStats stats;
    stats.frame_count = timestamps_sec.size();
    if (timestamps_sec.size() < 2) {
        return stats;
    }

    std::vector<double> intervals;
    intervals.reserve(timestamps_sec.size() - 1);
    for (std::size_t i = 1; i < timestamps_sec.size(); ++i) {
        const double gap = timestamps_sec[i] - timestamps_sec[i - 1];
        if (gap > 0.0) {
            intervals.push_back(gap);
        } else {
            ++stats.non_increasing_count;
        }
    }
    if (intervals.empty()) {
        return stats;
    }

    stats.median_interval_sec = median(intervals);
    stats.measured_fps = 1.0 / stats.median_interval_sec;

    const double span = timestamps_sec.back() - timestamps_sec.front();
    stats.duration_sec = span + stats.median_interval_sec;

    const double mean_interval = mean(intervals);
    double sum_squared_diff = 0.0;
    for (double gap : intervals) {
        sum_squared_diff += (gap - mean_interval) * (gap - mean_interval);
    }
    const double stddev = std::sqrt(sum_squared_diff / static_cast<double>(intervals.size()));
    stats.interval_variation = stddev / mean_interval;
    stats.is_variable_frame_rate =
        stats.interval_variation > limits::kVariableFrameRateTolerance;

    return stats;
}

}  // namespace rppg
