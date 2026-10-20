#include "rppg/heart_rate/heart_rate.hpp"

#include "rppg/heart_rate/spectrum.hpp"
#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace rppg {

namespace {

constexpr double kSecPerMin = 60.0;

constexpr std::array<Region, 4> kFaceRegions = {kGlabella, kForehead, kRightMalar, kLeftMalar};

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

FaceWindow combine_window(const std::vector<const WindowEstimate*>& estimates) {
    std::vector<double> bpm;
    std::vector<double> snr;
    for (const WindowEstimate* estimate : estimates) {
        bpm.push_back(estimate->smoothed_bpm);
        snr.push_back(estimate->snr_db);
    }
    FaceWindow face;
    face.center_sec = estimates.front()->center_sec;
    face.bpm = median(bpm);
    std::vector<double> deviations;
    for (double b : bpm) {
        deviations.push_back(std::abs(b - face.bpm));
    }
    face.deviation_bpm = median(deviations);
    face.snr_db = median(snr);
    face.regions = estimates.size();
    face.trusted = face.regions >= limits::kMinRegionsPerWindow &&
                   face.deviation_bpm <= limits::kMaxRegionDeviationBpm &&
                   face.snr_db >= limits::kMinWindowSnrDb;
    return face;
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

std::vector<FaceWindow> combine_regions(const RegionHeartRates& regions) {
    std::vector<const RegionHeartRate*> present;
    for (Region r : kFaceRegions) {
        if (regions[r]) {
            present.push_back(&*regions[r]);
        }
    }
    if (present.empty()) {
        return {};
    }
    const std::size_t window_count = present.front()->windows.size();
    for (const RegionHeartRate* region : present) {
        if (region->windows.size() != window_count) {
            throw std::invalid_argument("combine_regions: regions have different window counts");
        }
    }

    std::vector<FaceWindow> faces;
    for (std::size_t i = 0; i < window_count; ++i) {
        std::vector<const WindowEstimate*> estimates;
        for (const RegionHeartRate* region : present) {
            estimates.push_back(&region->windows[i]);
        }
        faces.push_back(combine_window(estimates));
    }
    return faces;
}

std::optional<double> trusted_bpm(const std::vector<FaceWindow>& windows) {
    std::vector<double> trusted;
    for (const FaceWindow& window : windows) {
        if (window.trusted) {
            trusted.push_back(window.bpm);
        }
    }
    const double needed = limits::kMinTrustedWindowFraction * static_cast<double>(windows.size());
    if (trusted.empty() || static_cast<double>(trusted.size()) < needed) {
        return std::nullopt;
    }
    return median(trusted);
}

HeartRates estimate_heart_rates(const RegionPulses& pulses) {
    HeartRates rates;
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        if (pulses.pulses[r]) {
            rates.regions[r] =
                summarize(estimate_windows(*pulses.pulses[r], pulses.start_sec, pulses.rate_hz));
        }
    }
    rates.face_windows = combine_regions(rates.regions);
    rates.bpm = trusted_bpm(rates.face_windows);
    return rates;
}

}  // namespace rppg
