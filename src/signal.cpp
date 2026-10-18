#include "rppg/signal.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stdexcept>

namespace rppg {

double mean(const std::vector<double>& x) {
    if (x.empty()) {
        return 0.0;
    }
    return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
}

double median(std::vector<double> x) {
    if (x.empty()) {
        return 0.0;
    }
    const std::size_t mid = x.size() / 2;
    std::nth_element(x.begin(), x.begin() + mid, x.end());
    const double upper = x[mid];
    if (x.size() % 2 == 1) {
        return upper;
    }
    const double lower = *std::max_element(x.begin(), x.begin() + mid);
    return (lower + upper) / 2.0;
}

std::vector<double> remove_mean(const std::vector<double>& x) {
    const double m = mean(x);
    std::vector<double> out;
    out.reserve(x.size());
    for (double v : x) {
        out.push_back(v - m);
    }
    return out;
}

std::vector<double> resample_uniform(const std::vector<double>& times_sec,
                                     const std::vector<double>& values,
                                     double rate_hz) {
    if (times_sec.size() != values.size()) {
        throw std::invalid_argument("resample_uniform: times and values differ in size");
    }
    if (times_sec.size() < 2) {
        throw std::invalid_argument("resample_uniform: need at least two samples");
    }
    if (rate_hz <= 0.0) {
        throw std::invalid_argument("resample_uniform: rate must be positive");
    }
    for (std::size_t i = 1; i < times_sec.size(); ++i) {
        if (times_sec[i] <= times_sec[i - 1]) {
            throw std::invalid_argument("resample_uniform: times must be strictly increasing");
        }
    }

    const double start = times_sec.front();
    const double end = times_sec.back();
    const double step = 1.0 / rate_hz;

    const auto sample_count = static_cast<std::size_t>((end - start) / step + 1e-9) + 1;

    std::vector<double> out;
    out.reserve(sample_count);

    std::size_t seg = 0;
    for (std::size_t k = 0; k < sample_count; ++k) {
        const double t = start + static_cast<double>(k) * step;
        while (seg + 2 < times_sec.size() && times_sec[seg + 1] < t) {
            ++seg;
        }
        const double t0 = times_sec[seg];
        const double t1 = times_sec[seg + 1];
        //frac is how far t is between t0 (0.0) and t1 (1.0).
        const double frac = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
        out.push_back(values[seg] + frac * (values[seg + 1] - values[seg]));
    }
    return out;
}

}  // namespace rppg
