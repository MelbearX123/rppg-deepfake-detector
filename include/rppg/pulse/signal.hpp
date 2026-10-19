#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace rppg {

double mean(const std::vector<double>& x);

double median(std::vector<double> x);

double stddev(const std::vector<double>& x);

std::vector<double> resample_uniform(const std::vector<double>& times_sec,
                                     const std::vector<double>& values, double rate_hz);

std::vector<double> moving_average_centered(const std::vector<double>& x, std::size_t window);

std::vector<std::optional<double>> fill_gaps(const std::vector<std::optional<double>>& x,
                                             std::size_t max_gap);

}  // namespace rppg
