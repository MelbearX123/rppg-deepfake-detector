#include "rppg/eval/ground_truth.hpp"

#include "temp_file.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string line_of(const std::vector<double>& values) {
    std::ostringstream out;
    for (double v : values) {
        out << "  " << v;
    }
    return out.str() + "\n";
}

// 20 s of a 72 bpm waveform on top of a level of 5, with uneven sample spacing
// (alternating 0.02 s and 0.046 s) like the UBFC sensor.
rppg::GroundTruth uneven_pulse() {
    rppg::GroundTruth truth;
    double t = 0.0;
    for (std::size_t i = 0; t < 20.0; ++i) {
        truth.times_sec.push_back(t);
        truth.ppg.push_back(5.0 + std::sin(2.0 * kPi * 1.2 * t));
        truth.displayed_bpm.push_back(72.0);
        t += i % 2 == 0 ? 0.02 : 0.046;
    }
    return truth;
}

}  // namespace

TEST(GroundTruth, ReadsThreeLines) {
    const TempFile file("truth.txt");
    file.write(line_of({0.5, -0.25, 1.0}) + line_of({70, 71, 72}) + line_of({0.0, 0.033, 0.066}));
    const rppg::GroundTruth truth = rppg::read_ubfc_ground_truth(file.path());
    EXPECT_EQ(truth.ppg, (std::vector<double>{0.5, -0.25, 1.0}));
    EXPECT_EQ(truth.displayed_bpm, (std::vector<double>{70, 71, 72}));
    EXPECT_EQ(truth.times_sec, (std::vector<double>{0.0, 0.033, 0.066}));
}

TEST(GroundTruth, BadFilesThrow) {
    EXPECT_THROW(rppg::read_ubfc_ground_truth("/no/such/ground_truth.txt"), std::runtime_error);
    const TempFile file("bad_truth.txt");
    file.write(line_of({1, 2}) + line_of({70, 71}));  // only two lines
    EXPECT_THROW(rppg::read_ubfc_ground_truth(file.path()), std::runtime_error);
    file.write(line_of({1, 2}) + line_of({70, 71}) + line_of({0.0}));  // lengths differ
    EXPECT_THROW(rppg::read_ubfc_ground_truth(file.path()), std::runtime_error);
    file.write(line_of({1, 2}) + "70 seventy\n" + line_of({0.0, 0.1}));  // not a number
    EXPECT_THROW(rppg::read_ubfc_ground_truth(file.path()), std::runtime_error);
}

TEST(GroundTruth, WaveformRateFromUnevenSamples) {
    EXPECT_NEAR(rppg::waveform_bpm(uneven_pulse(), 10.0, 5.0), 72.0, 0.5);
}

TEST(GroundTruth, RepeatedTimestampIsSkipped) {
    rppg::GroundTruth truth = uneven_pulse();
    truth.times_sec[100] = truth.times_sec[99];
    EXPECT_NEAR(rppg::waveform_bpm(truth, 10.0, 5.0), 72.0, 0.5);
}

TEST(GroundTruth, EmptyWindowThrows) {
    EXPECT_THROW(rppg::waveform_bpm(uneven_pulse(), 100.0, 5.0), std::invalid_argument);
}
