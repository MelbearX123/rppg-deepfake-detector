#include "rppg/heart_rate/spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace rppg {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRoundingTolerance = 1e-9;


std::vector<double> hann_tapered(const std::vector<double>& x) {
    std::vector<double> out;
    out.reserve(x.size());
    const auto last = static_cast<double>(x.size() - 1);
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double weight =
            x.size() == 1 ? 1.0 : 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(n) / last);
        out.push_back(x[n] * weight);
    }
    return out;
}

double power_at(const std::vector<double>& x, double hz, double rate_hz) {
    double real = 0.0;
    double imag = 0.0;
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double angle = 2.0 * kPi * hz * static_cast<double>(n) / rate_hz;
        real += x[n] * std::cos(angle);
        imag -= x[n] * std::sin(angle);
    }
    return real * real + imag * imag;
}

double frequency_at(const Spectrum& spectrum, std::size_t k) {
    return spectrum.min_hz + static_cast<double>(k) * spectrum.step_hz;
}

}  // namespace

Spectrum power_spectrum(const std::vector<double>& x, double rate_hz, double min_hz, double max_hz,
                        double step_hz) {
    if (x.empty()) {
        throw std::invalid_argument("power_spectrum: input is empty");
    }
    if (rate_hz <= 0.0 || step_hz <= 0.0 || min_hz < 0.0 || min_hz > max_hz) {
        throw std::invalid_argument(
            "power_spectrum: need positive rate and step, and 0 <= min_hz <= max_hz");
    }

    const std::vector<double> tapered = hann_tapered(x);
    Spectrum spectrum{min_hz, step_hz, {}};
    const auto count =
        static_cast<std::size_t>((max_hz - min_hz) / step_hz + kRoundingTolerance) + 1;
    spectrum.power.reserve(count);
    for (std::size_t k = 0; k < count; ++k) {
        spectrum.power.push_back(power_at(tapered, frequency_at(spectrum, k), rate_hz));
    }
    return spectrum;
}

double peak_hz(const Spectrum& spectrum) {
    if (spectrum.power.empty()) {
        throw std::invalid_argument("peak_hz: spectrum is empty");
    }
    const auto tallest = std::max_element(spectrum.power.begin(), spectrum.power.end());
    return frequency_at(spectrum, static_cast<std::size_t>(tallest - spectrum.power.begin()));
}

double snr_db(const Spectrum& spectrum, double peak_hz, double half_width_hz) {
    double signal = 0.0;
    double noise = 0.0;
    for (std::size_t k = 0; k < spectrum.power.size(); ++k) {
        const double hz = frequency_at(spectrum, k);
        const double reach = half_width_hz + kRoundingTolerance;
        const bool near_peak =
            std::abs(hz - peak_hz) <= reach || std::abs(hz - 2.0 * peak_hz) <= reach;
        (near_peak ? signal : noise) += spectrum.power[k];
    }
    if (signal <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    if (noise <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return 10.0 * std::log10(signal / noise);
}

}  // namespace rppg
