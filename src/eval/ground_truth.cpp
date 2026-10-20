#include "rppg/eval/ground_truth.hpp"

#include "rppg/heart_rate/spectrum.hpp"
#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rppg {

namespace {

constexpr double kSecPerMin = 60.0;

std::vector<double> parse_numbers(const std::string& line) {
    std::istringstream stream(line);
    std::vector<double> values;
    double value = 0.0;
    while (stream >> value) {
        values.push_back(value);
    }
    if (!stream.eof()) {
        throw std::runtime_error(
            "read_ubfc_ground_truth: a line holds something that isn't a number");
    }
    return values;
}

}  // namespace

GroundTruth read_ubfc_ground_truth(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("read_ubfc_ground_truth: could not open " + path);
    }
    std::vector<std::vector<double>> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            lines.push_back(parse_numbers(line));
        }
    }
    if (lines.size() != 3) {
        throw std::runtime_error("read_ubfc_ground_truth: " + path + " should have 3 lines, has " +
                                 std::to_string(lines.size()));
    }
    if (lines[0].empty() || lines[1].size() != lines[0].size() ||
        lines[2].size() != lines[0].size()) {
        throw std::runtime_error("read_ubfc_ground_truth: " + path +
                                 " lines are empty or differ in length");
    }
    return {lines[0], lines[1], lines[2]};
}

double waveform_bpm(const GroundTruth& truth, double center_sec, double half_window_sec) {
    std::vector<double> times;
    std::vector<double> values;
    for (std::size_t i = 0; i < truth.times_sec.size(); ++i) {
        const double t = truth.times_sec[i];
        const bool moves_forward = times.empty() || t > times.back();
        if (t >= center_sec - half_window_sec && t < center_sec + half_window_sec &&
            moves_forward) {
            times.push_back(t);
            values.push_back(truth.ppg[i]);
        }
    }
    if (times.size() < 2) {
        throw std::invalid_argument("waveform_bpm: fewer than two samples in the window");
    }

    std::vector<double> even = resample_uniform(times, values, limits::kPulseRateHz);
    const double level = mean(even);
    for (double& v : even) {
        v -= level;
    }
    const Spectrum spectrum =
        power_spectrum(even, limits::kPulseRateHz, limits::kMinPulseHz, limits::kMaxPulseHz,
                       limits::kSpectrumStepBpm / kSecPerMin);
    return peak_hz(spectrum) * kSecPerMin;
}

}  // namespace rppg
