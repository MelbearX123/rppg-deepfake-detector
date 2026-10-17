#pragma once

#include <vector>

namespace rppg {

// Arithmetic mean. Returns 0 for an empty input.
double mean(const std::vector<double>& x);

// Subtract the mean so the signal is centered on zero.
std::vector<double> remove_mean(const std::vector<double>& x);

}  // namespace rppg
