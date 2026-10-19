#include "rppg/signal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>

namespace rppg {

namespace {

constexpr double kRoundingTolerance = 1e-9;

void require_non_empty(const std::vector<double>& x, const char* function_name) {
    if (x.empty()) {
        throw std::invalid_argument(std::string(function_name) + ": input is empty");
    }
}

}  // namespace

double mean(const std::vector<double>& x) {
    require_non_empty(x, "mean");
    return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
}

double median(std::vector<double> x) {
    require_non_empty(x, "median");
    const std::size_t mid = x.size() / 2;
    std::nth_element(x.begin(), x.begin() + mid, x.end());
    const double upper = x[mid];
    if (x.size() % 2 == 1) {
        return upper;
    }
    const double lower = *std::max_element(x.begin(), x.begin() + mid);
    return (lower + upper) / 2.0;
}

double stddev(const std::vector<double>& x) {
    const double m = mean(x);
    double sum_squared_diff = 0.0;
    for (double v : x) {
        sum_squared_diff += (v - m) * (v - m);
    }
    return std::sqrt(sum_squared_diff / static_cast<double>(x.size()));
}

std::vector<double> resample_uniform(const std::vector<double>& times_sec,
                                     const std::vector<double>& values, double rate_hz) {
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
    const auto sample_count =
        static_cast<std::size_t>((end - start) / step + kRoundingTolerance) + 1;

    std::vector<double> out;
    out.reserve(sample_count);

    std::size_t segment = 0;
    for (std::size_t k = 0; k < sample_count; ++k) {
        const double t = start + static_cast<double>(k) * step;
        while (segment + 2 < times_sec.size() && times_sec[segment + 1] < t) {
            ++segment;
        }
        const double t0 = times_sec[segment];
        const double t1 = times_sec[segment + 1];
        // How far t is between t0 (0.0) and t1 (1.0)
        const double frac = std::clamp((t - t0) / (t1 - t0), 0.0, 1.0);
        out.push_back(values[segment] + frac * (values[segment + 1] - values[segment]));
    }
    return out;
}

std::vector<double> moving_average_centered(const std::vector<double>& x, std::size_t window) {
    if (window % 2 == 0) {
        throw std::invalid_argument("moving_average_centered: window must be odd");
    }
    const std::size_t half = window / 2;

    std::vector<double> out;
    out.reserve(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        const std::size_t reach = std::min({half, i, x.size() - 1 - i});
        const auto first = x.begin() + static_cast<std::ptrdiff_t>(i - reach);
        const auto last = x.begin() + static_cast<std::ptrdiff_t>(i + reach + 1);
        out.push_back(std::accumulate(first, last, 0.0) / static_cast<double>(2 * reach + 1));
    }
    return out;
}

std::vector<std::optional<double>> fill_gaps(const std::vector<std::optional<double>>& x,
                                             std::size_t max_gap) {
    std::vector<std::optional<double>> out = x;
    std::size_t i = 0;
    while (i < out.size()) {
        if (out[i]) {
            ++i;
            continue;
        }
        const std::size_t gap_begin = i;
        while (i < out.size() && !out[i]) {
            ++i;
        }
        const std::size_t gap_end = i;
        const bool has_both_sides = gap_begin > 0 && gap_end < out.size();
        if (!has_both_sides || gap_end - gap_begin > max_gap) {
            continue;
        }
        const double before = *out[gap_begin - 1];
        const double after = *out[gap_end];
        const auto steps = static_cast<double>(gap_end - gap_begin + 1);
        for (std::size_t k = gap_begin; k < gap_end; ++k) {
            const double frac = static_cast<double>(k - gap_begin + 1) / steps;
            out[k] = before + frac * (after - before);
        }
    }
    return out;
}

}  // namespace rppg
