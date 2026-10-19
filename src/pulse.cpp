#include "rppg/pulse.hpp"

#include "rppg/filter.hpp"
#include "rppg/limits.hpp"
#include "rppg/signal.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rppg {

namespace {

double longest_gap_sec(const std::vector<double>& known_times, double start_sec, double end_sec) {
    double longest = std::max(known_times.front() - start_sec, end_sec - known_times.back());
    for (std::size_t k = 1; k < known_times.size(); ++k) {
        longest = std::max(longest, known_times[k] - known_times[k - 1]);
    }
    return longest;
}

Color mean_color(const std::vector<Color>& colors, std::size_t begin, std::size_t window) {
    Color sum;
    for (std::size_t k = begin; k < begin + window; ++k) {
        sum.r += colors[k].r;
        sum.g += colors[k].g;
        sum.b += colors[k].b;
    }
    const auto n = static_cast<double>(window);
    return Color{sum.b / n, sum.g / n, sum.r / n};
}

void add_window_pulse(const std::vector<Color>& colors, std::size_t begin, std::size_t window,
                      std::vector<double>& pulse) {
    const Color m = mean_color(colors, begin, window);
    if (m.r <= 0.0 || m.g <= 0.0 || m.b <= 0.0) {
        throw std::invalid_argument("pos_pulse: every channel must have a positive mean");
    }

    std::vector<double> s1;
    std::vector<double> s2;
    s1.reserve(window);
    s2.reserve(window);
    for (std::size_t k = begin; k < begin + window; ++k) {
        const double r = colors[k].r / m.r;
        const double g = colors[k].g / m.g;
        const double b = colors[k].b / m.b;
        s1.push_back(g - b);
        s2.push_back(-2.0 * r + g + b);
    }

    const double s2_spread = stddev(s2);
    const double alpha = s2_spread > 0.0 ? stddev(s1) / s2_spread : 0.0;

    std::vector<double> h(window);
    for (std::size_t k = 0; k < window; ++k) {
        h[k] = s1[k] + alpha * s2[k];
    }
    const double h_mean = mean(h);
    for (std::size_t k = 0; k < window; ++k) {
        pulse[begin + k] += h[k] - h_mean;
    }
}

}  // namespace

std::optional<std::vector<Color>> resample_colors(const RegionSeries& series,
                                                  const std::vector<double>& timestamps_sec,
                                                  const Segment& segment, double rate_hz,
                                                  double max_gap_sec) {
    if (segment.size() < 2) {
        throw std::invalid_argument("resample_colors: segment needs at least two frames");
    }

    const double start_sec = timestamps_sec[segment.begin];
    const double end_sec = timestamps_sec[segment.end - 1];

    std::vector<double> times;
    std::vector<double> r;
    std::vector<double> g;
    std::vector<double> b;
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        const double t = timestamps_sec[i];
        const bool in_order = t >= start_sec && t <= end_sec && (times.empty() || t > times.back());
        if (series[i].color && in_order) {
            times.push_back(t);
            r.push_back(series[i].color->r);
            g.push_back(series[i].color->g);
            b.push_back(series[i].color->b);
        }
    }
    if (times.empty()) {
        return std::nullopt;
    }

    if (longest_gap_sec(times, start_sec, end_sec) > max_gap_sec) {
        return std::nullopt;
    }
    if (times.front() > start_sec) {
        times.insert(times.begin(), start_sec);
        r.insert(r.begin(), r.front());
        g.insert(g.begin(), g.front());
        b.insert(b.begin(), b.front());
    }
    if (times.back() < end_sec) {
        times.push_back(end_sec);
        r.push_back(r.back());
        g.push_back(g.back());
        b.push_back(b.back());
    }

    const std::vector<double> even_r = resample_uniform(times, r, rate_hz);
    const std::vector<double> even_g = resample_uniform(times, g, rate_hz);
    const std::vector<double> even_b = resample_uniform(times, b, rate_hz);

    std::vector<Color> colors;
    colors.reserve(even_r.size());
    for (std::size_t k = 0; k < even_r.size(); ++k) {
        colors.push_back(Color{even_b[k], even_g[k], even_r[k]});
    }
    return colors;
}

std::vector<double> pos_pulse(const std::vector<Color>& colors, std::size_t window) {
    if (window < 2 || window > colors.size()) {
        throw std::invalid_argument("pos_pulse: window must be between 2 and the number of colors");
    }
    std::vector<double> pulse(colors.size(), 0.0);
    for (std::size_t begin = 0; begin + window <= colors.size(); ++begin) {
        add_window_pulse(colors, begin, window, pulse);
    }
    return pulse;
}

RegionPulses extract_pulses(const RegionSignals& signals, const std::vector<double>& timestamps_sec,
                            const Segment& segment) {
    RegionPulses result;
    result.start_sec = timestamps_sec[segment.begin];
    result.rate_hz = limits::kPulseRateHz;
    const auto window =
        static_cast<std::size_t>(std::lround(limits::kPosWindowSec * limits::kPulseRateHz));

    for (std::size_t r = 0; r < kRegionCount; ++r) {
        if (!is_usable(median_coverage(signals[r], segment))) {
            continue;
        }
        const auto colors = resample_colors(signals[r], timestamps_sec, segment,
                                            limits::kPulseRateHz, limits::kMaxGapSec);
        if (colors) {
            result.pulses[r] = pos_pulse(*colors, window);
        }
    }
    return result;
}

RegionPulses bandpass_pulses(const RegionPulses& pulses) {
    RegionPulses result = pulses;
    const auto pad = static_cast<std::size_t>(std::lround(limits::kFilterPadSec * pulses.rate_hz));
    for (auto& pulse : result.pulses) {
        if (pulse) {
            *pulse = bandpass_zero_phase(*pulse, pulses.rate_hz, limits::kMinPulseHz,
                                         limits::kMaxPulseHz, pad);
        }
    }
    return result;
}

}  // namespace rppg
