#include "rppg/pulse/filter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace rppg {

namespace {

constexpr double kPi = 3.14159265358979323846;
// Q = 1/sqrt(2) is what makes a second-order section Butterworth (flattest passband).
constexpr double kButterworthQ = 0.70710678118654752440;

struct CookbookTerms {
    double cos_w0 = 0.0;
    double alpha = 0.0;
};

CookbookTerms cookbook_terms(double cutoff_hz, double rate_hz, const char* function_name) {
    if (!(cutoff_hz > 0.0 && cutoff_hz < rate_hz / 2.0)) {
        throw std::invalid_argument(std::string(function_name) +
                                    ": cutoff must be between 0 and half the sample rate");
    }
    const double w0 = 2.0 * kPi * cutoff_hz / rate_hz;
    return {std::cos(w0), std::sin(w0) / (2.0 * kButterworthQ)};
}

Biquad divided_by_a0(double b0, double b1, double b2, const CookbookTerms& terms) {
    const double a0 = 1.0 + terms.alpha;
    return {b0 / a0, b1 / a0, b2 / a0, -2.0 * terms.cos_w0 / a0, (1.0 - terms.alpha) / a0};
}

std::vector<double> pad_with_edges(const std::vector<double>& x, std::size_t pad) {
    std::vector<double> padded(pad, x.front());
    padded.reserve(x.size() + 2 * pad);
    padded.insert(padded.end(), x.begin(), x.end());
    padded.insert(padded.end(), pad, x.back());
    return padded;
}

std::vector<double> filter_both_ways(const Biquad& biquad, std::vector<double> x) {
    x = filter_forward(biquad, x);
    std::reverse(x.begin(), x.end());
    x = filter_forward(biquad, x);
    std::reverse(x.begin(), x.end());
    return x;
}

}  // namespace

Biquad butterworth_low_pass(double cutoff_hz, double rate_hz) {
    const CookbookTerms terms = cookbook_terms(cutoff_hz, rate_hz, "butterworth_low_pass");
    const double c = terms.cos_w0;
    return divided_by_a0((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, terms);
}

Biquad butterworth_high_pass(double cutoff_hz, double rate_hz) {
    const CookbookTerms terms = cookbook_terms(cutoff_hz, rate_hz, "butterworth_high_pass");
    const double c = terms.cos_w0;
    return divided_by_a0((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, terms);
}

std::vector<double> filter_forward(const Biquad& biquad, const std::vector<double>& x) {
    std::vector<double> y(x.size());
    double x1 = 0.0;
    double x2 = 0.0;
    double y1 = 0.0;
    double y2 = 0.0;
    for (std::size_t n = 0; n < x.size(); ++n) {
        y[n] = biquad.b0 * x[n] + biquad.b1 * x1 + biquad.b2 * x2 - biquad.a1 * y1 - biquad.a2 * y2;
        x2 = x1;
        x1 = x[n];
        y2 = y1;
        y1 = y[n];
    }
    return y;
}

std::vector<double> bandpass_zero_phase(const std::vector<double>& x, double rate_hz, double low_hz,
                                        double high_hz, std::size_t pad) {
    if (x.empty()) {
        throw std::invalid_argument("bandpass_zero_phase: input is empty");
    }
    if (low_hz >= high_hz) {
        throw std::invalid_argument("bandpass_zero_phase: low_hz must be below high_hz");
    }
    std::vector<double> y = pad_with_edges(x, pad);
    y = filter_both_ways(butterworth_high_pass(low_hz, rate_hz), std::move(y));
    y = filter_both_ways(butterworth_low_pass(high_hz, rate_hz), std::move(y));
    const auto first = y.begin() + static_cast<std::ptrdiff_t>(pad);
    return std::vector<double>(first, first + static_cast<std::ptrdiff_t>(x.size()));
}

}  // namespace rppg
