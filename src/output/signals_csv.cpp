#include "rppg/output/signals_csv.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace rppg {

namespace {

constexpr char kSignalsHeader[] = "frame,time_sec,region,r,g,b,skin_pixels,coverage";
constexpr std::size_t kSignalsColumns = 8;

struct SignalsRow {
    std::size_t frame = 0;
    double time_sec = 0.0;
    Region region = kGlabella;
    RegionSample sample;
};

std::optional<Region> region_named(const std::string& name) {
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        if (name == region_name(static_cast<Region>(r))) {
            return static_cast<Region>(r);
        }
    }
    return std::nullopt;
}

std::vector<std::string> split_commas(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

double to_double(const std::string& field) {
    std::size_t used = 0;
    const double value = std::stod(field, &used);
    if (used != field.size()) {
        throw std::invalid_argument("not a number: " + field);
    }
    return value;
}

std::size_t to_count(const std::string& field) {
    if (field.empty() || field.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("not a whole number: " + field);
    }
    return std::stoul(field);
}

SignalsRow parse_signals_row(const std::string& line) {
    const std::vector<std::string> fields = split_commas(line);
    if (fields.size() != kSignalsColumns) {
        throw std::invalid_argument("wrong number of columns");
    }
    const std::optional<Region> region = region_named(fields[2]);
    if (!region) {
        throw std::invalid_argument("unknown region " + fields[2]);
    }

    SignalsRow row;
    row.frame = to_count(fields[0]);
    row.time_sec = to_double(fields[1]);
    row.region = *region;
    if (!fields[3].empty()) {
        row.sample.color = Color{to_double(fields[5]), to_double(fields[4]), to_double(fields[3])};
    }
    row.sample.skin_pixels = to_count(fields[6]);
    row.sample.coverage = to_double(fields[7]);
    return row;
}

void grow_to(SavedSignals& saved, std::size_t frame_count) {
    if (saved.timestamps_sec.size() >= frame_count) {
        return;
    }
    saved.timestamps_sec.resize(frame_count, std::numeric_limits<double>::quiet_NaN());
    for (RegionSeries& series : saved.signals) {
        series.resize(frame_count);
    }
}

}  // namespace

void write_signals_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                       const RegionSignals& signals, const Segment& segment) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    out << kSignalsHeader << '\n';
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

SavedSignals read_signals_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("read_signals_csv: could not open " + path);
    }
    std::string line;
    if (!std::getline(in, line) || line != kSignalsHeader) {
        throw std::runtime_error("read_signals_csv: " + path + " is not a signals CSV");
    }

    SavedSignals saved;
    std::optional<std::size_t> first_frame;
    std::size_t last_frame = 0;
    for (std::size_t line_number = 2; std::getline(in, line); ++line_number) {
        SignalsRow row;
        try {
            row = parse_signals_row(line);
        } catch (const std::exception& e) {
            throw std::runtime_error("read_signals_csv: " + path + " line " +
                                     std::to_string(line_number) + ": " + e.what());
        }
        grow_to(saved, row.frame + 1);
        saved.timestamps_sec[row.frame] = row.time_sec;
        saved.signals[row.region][row.frame] = row.sample;
        first_frame = std::min(first_frame.value_or(row.frame), row.frame);
        last_frame = std::max(last_frame, row.frame);
    }
    if (!first_frame) {
        throw std::runtime_error("read_signals_csv: " + path + " has no rows");
    }
    saved.segment = {*first_frame, last_frame + 1};
    return saved;
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
