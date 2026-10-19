#include "rppg/heart_rate/heart_rate.hpp"

#include "rppg/heart_rate/spectrum.hpp"
#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace rppg {

namespace {

constexpr double kSecPerMin = 60.0;

WindowEstimate estimate_window(const std::vector<double>& window, double center_sec,
                               double rate_hz) {
    const Spectrum spectrum =
        power_spectrum(window, rate_hz, limits::kMinPulseHz, limits::kMaxPulseHz,
                       limits::kSpectrumStepBpm / kSecPerMin);
    const double peak = peak_hz(spectrum);

    WindowEstimate estimate;
    estimate.center_sec = center_sec;
    estimate.peak_bpm = peak * kSecPerMin;
    estimate.snr_db = snr_db(spectrum, peak, limits::kSnrHalfWidthHz);
    return estimate;
}

double middle_rate(std::vector<const RegionHeartRate*> regions) {
    std::sort(regions.begin(), regions.end(),
              [](const RegionHeartRate* a, const RegionHeartRate* b) { return a->bpm < b->bpm; });
    const std::size_t mid = regions.size() / 2;
    if (regions.size() % 2 == 1) {
        return regions[mid]->bpm;
    }
    const RegionHeartRate* lower = regions[mid - 1];
    const RegionHeartRate* upper = regions[mid];
    return lower->snr_db >= upper->snr_db ? lower->bpm : upper->bpm;
}

RegionHeartRate summarize(std::vector<WindowEstimate> windows) {
    std::vector<double> bpm;
    std::vector<double> snr;
    for (const WindowEstimate& window : windows) {
        bpm.push_back(window.smoothed_bpm);
        snr.push_back(window.snr_db);
    }
    RegionHeartRate region;
    region.windows = std::move(windows);
    region.bpm = median(bpm);
    region.snr_db = median(snr);
    return region;
}

}  // namespace

std::vector<WindowEstimate> estimate_windows(const std::vector<double>& pulse, double start_sec,
                                             double rate_hz) {
    if (pulse.empty() || rate_hz <= 0.0) {
        throw std::invalid_argument("estimate_windows: need a non-empty pulse and a positive rate");
    }
    const auto full_window =
        static_cast<std::size_t>(std::lround(limits::kHeartRateWindowSec * rate_hz));
    const std::size_t length = std::min(full_window, pulse.size());
    const auto step = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::lround(limits::kHeartRateStepSec * rate_hz)));

    std::vector<WindowEstimate> windows;
    for (std::size_t begin = 0; begin + length <= pulse.size(); begin += step) {
        const auto first = pulse.begin() + static_cast<std::ptrdiff_t>(begin);
        const std::vector<double> window(first, first + static_cast<std::ptrdiff_t>(length));
        const double center_sec =
            start_sec + (static_cast<double>(begin) + static_cast<double>(length) / 2.0) / rate_hz;
        windows.push_back(estimate_window(window, center_sec, rate_hz));
    }

    std::vector<double> peaks;
    for (const WindowEstimate& window : windows) {
        peaks.push_back(window.peak_bpm);
    }
    const std::vector<double> smoothed =
        median_filter_centered(peaks, limits::kHeartRateSmoothingWindows);
    for (std::size_t i = 0; i < windows.size(); ++i) {
        windows[i].smoothed_bpm = smoothed[i];
    }
    return windows;
}

HeartRates estimate_heart_rates(const RegionPulses& pulses) {
    HeartRates rates;
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        if (pulses.pulses[r]) {
            rates.regions[r] =
                summarize(estimate_windows(*pulses.pulses[r], pulses.start_sec, pulses.rate_hz));
        }
    }

    std::vector<const RegionHeartRate*> confident;
    for (const auto& region : rates.regions) {
        if (region && region->snr_db >= limits::kMinSnrDb) {
            confident.push_back(&*region);
        }
    }
    if (!confident.empty()) {
        rates.bpm = middle_rate(confident);
    }
    return rates;
}

}  // namespace rppg
