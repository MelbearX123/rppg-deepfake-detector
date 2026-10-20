#pragma once

#include <cstddef>
#include <vector>

namespace rppg {

struct ErrorStats {
    std::size_t count = 0;
    double mae = 0.0;
    double rmse = 0.0;
    double pearson_r = 0.0;
    double within_5_bpm = 0.0;
};

ErrorStats error_stats(const std::vector<double>& estimate, const std::vector<double>& truth);

}  // namespace rppg
