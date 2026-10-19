#pragma once

#include <cstddef>
#include <vector>

namespace rppg {

struct Biquad {
    double b0 = 0.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

Biquad butterworth_low_pass(double cutoff_hz, double rate_hz);
Biquad butterworth_high_pass(double cutoff_hz, double rate_hz);

std::vector<double> filter_forward(const Biquad& biquad, const std::vector<double>& x);

std::vector<double> bandpass_zero_phase(const std::vector<double>& x, double rate_hz, double low_hz,
                                        double high_hz, std::size_t pad);

}  // namespace rppg
