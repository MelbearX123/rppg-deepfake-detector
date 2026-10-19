#pragma once

#include <vector>

namespace rppg {

struct Spectrum {
    double min_hz = 0.0;
    double step_hz = 0.0;
    std::vector<double> power;
};

Spectrum power_spectrum(const std::vector<double>& x, double rate_hz, double min_hz, double max_hz,
                        double step_hz);

double peak_hz(const Spectrum& spectrum);

double snr_db(const Spectrum& spectrum, double peak_hz, double half_width_hz);

}  // namespace rppg
