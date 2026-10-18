#pragma once

#include <vector>

namespace rppg {

double mean(const std::vector<double>& x);

double median(std::vector<double> x);

std::vector<double> remove_mean(const std::vector<double>& x);

std::vector<double> resample_uniform(const std::vector<double>& times_sec,
                                     const std::vector<double>& values,
                                     double rate_hz);

}  // namespace rppg
