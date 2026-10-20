#include "rppg/eval/metrics.hpp"

#include "rppg/pulse/signal.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace rppg {

namespace {

constexpr double kCloseEnoughBpm = 5.0;

double pearson(const std::vector<double>& x, const std::vector<double>& y) {
    const double sx = stddev(x);
    const double sy = stddev(y);
    if (sx == 0.0 || sy == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double mx = mean(x);
    const double my = mean(y);
    double sum = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        sum += (x[i] - mx) * (y[i] - my);
    }
    return sum / static_cast<double>(x.size()) / (sx * sy);
}

}  // namespace

ErrorStats error_stats(const std::vector<double>& estimate, const std::vector<double>& truth) {
    if (estimate.empty() || estimate.size() != truth.size()) {
        throw std::invalid_argument("error_stats: need two non-empty vectors of the same size");
    }
    double abs_sum = 0.0;
    double square_sum = 0.0;
    std::size_t close = 0;
    for (std::size_t i = 0; i < estimate.size(); ++i) {
        const double error = estimate[i] - truth[i];
        abs_sum += std::abs(error);
        square_sum += error * error;
        close += std::abs(error) <= kCloseEnoughBpm ? 1 : 0;
    }
    const auto n = static_cast<double>(estimate.size());

    ErrorStats stats;
    stats.count = estimate.size();
    stats.mae = abs_sum / n;
    stats.rmse = std::sqrt(square_sum / n);
    stats.pearson_r = pearson(estimate, truth);
    stats.within_5_bpm = static_cast<double>(close) / n;
    return stats;
}

}  // namespace rppg
