#include "rppg/output/signals_csv.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace rppg {

void write_signals_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                       const RegionSignals& signals, const Segment& segment) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    out << "frame,time_sec,region,r,g,b,skin_pixels,coverage\n";
    out << std::fixed << std::setprecision(4);
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        for (std::size_t r = 0; r < kRegionCount; ++r) {
            const RegionSample& sample = signals[r][i];
            out << i << ',' << timestamps_sec[i] << ',' << region_name(static_cast<Region>(r))
                << ',';
            if (sample.color) {
                out << sample.color->r << ',' << sample.color->g << ',' << sample.color->b;
            } else {
                out << ",,";
            }
            out << ',' << sample.skin_pixels << ',' << sample.coverage << '\n';
        }
    }
}

void write_pulse_csv(const std::string& path, const RegionPulses& pulses) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    out << "time_sec";
    std::size_t sample_count = 0;
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        out << ',' << region_name(static_cast<Region>(r));
        if (pulses.pulses[r]) {
            sample_count = std::max(sample_count, pulses.pulses[r]->size());
        }
    }
    out << '\n';

    out << std::fixed << std::setprecision(6);
    for (std::size_t k = 0; k < sample_count; ++k) {
        out << pulses.start_sec + static_cast<double>(k) / pulses.rate_hz;
        for (const auto& pulse : pulses.pulses) {
            out << ',';
            if (pulse) {
                out << (*pulse)[k];
            }
        }
        out << '\n';
    }
}

void write_heart_rate_csv(const std::string& path, const HeartRates& rates) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    out << "region,center_sec,peak_bpm,smoothed_bpm,snr_db\n";
    out << std::fixed << std::setprecision(3);
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        if (!rates.regions[r]) {
            continue;
        }
        for (const WindowEstimate& window : rates.regions[r]->windows) {
            out << region_name(static_cast<Region>(r)) << ',' << window.center_sec << ','
                << window.peak_bpm << ',' << window.smoothed_bpm << ',' << window.snr_db << '\n';
        }
    }
}

}  // namespace rppg
