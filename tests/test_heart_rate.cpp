#include "rppg/heart_rate/heart_rate.hpp"

#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRate = 30.0;
constexpr std::size_t kTwentySeconds = 600;

std::vector<double> pulse_at(double bpm, std::size_t count = kTwentySeconds) {
    std::vector<double> x;
    for (std::size_t n = 0; n < count; ++n) {
        x.push_back(std::sin(2.0 * kPi * bpm / 60.0 * static_cast<double>(n) / kRate));
    }
    return x;
}

// Uniform noise in [-1, 1], the same on every platform.
std::vector<double> noise(unsigned seed) {
    std::mt19937 generator(seed);
    std::vector<double> x;
    for (std::size_t n = 0; n < kTwentySeconds; ++n) {
        x.push_back(2.0 * static_cast<double>(generator()) / std::mt19937::max() - 1.0);
    }
    return x;
}

rppg::RegionPulses pulses_starting_at(double start_sec) {
    rppg::RegionPulses pulses;
    pulses.start_sec = start_sec;
    pulses.rate_hz = kRate;
    return pulses;
}

}  // namespace

// --- estimate_windows ---

TEST(HeartRateWindows, SlideOneSecondAtATime) {
    // 20 s with 10 s windows: starts at 0, 1, ..., 10 s.
    const auto windows = rppg::estimate_windows(pulse_at(72.0), 2.0, kRate);
    ASSERT_EQ(windows.size(), 11u);
    EXPECT_DOUBLE_EQ(windows.front().center_sec, 7.0);
    EXPECT_DOUBLE_EQ(windows.back().center_sec, 17.0);
}

TEST(HeartRateWindows, ShortPulseIsOneWindow) {
    // The 10 s minimum clip can resample to 299 samples, one short of a window.
    const auto windows = rppg::estimate_windows(pulse_at(72.0, 299), 0.0, kRate);
    ASSERT_EQ(windows.size(), 1u);
    EXPECT_NEAR(windows.front().peak_bpm, 72.0, 0.5);
}

TEST(HeartRateWindows, CleanPulseGivesItsRate) {
    for (const auto& window : rppg::estimate_windows(pulse_at(72.0), 0.0, kRate)) {
        EXPECT_NEAR(window.peak_bpm, 72.0, 0.5);
        EXPECT_NEAR(window.smoothed_bpm, 72.0, 0.5);
        EXPECT_GT(window.snr_db, 10.0);
    }
}

TEST(HeartRateWindows, SmoothedIsTheMedianOfNeighboringPeaks) {
    const auto windows = rppg::estimate_windows(noise(1), 0.0, kRate);
    std::vector<double> peaks;
    for (const auto& window : windows) {
        peaks.push_back(window.peak_bpm);
    }
    const auto expected =
        rppg::median_filter_centered(peaks, rppg::limits::kHeartRateSmoothingWindows);
    for (std::size_t i = 0; i < windows.size(); ++i) {
        EXPECT_DOUBLE_EQ(windows[i].smoothed_bpm, expected[i]);
    }
}

TEST(HeartRateWindows, BadInputThrows) {
    EXPECT_THROW(rppg::estimate_windows({}, 0.0, kRate), std::invalid_argument);
    EXPECT_THROW(rppg::estimate_windows(pulse_at(72.0), 0.0, 0.0), std::invalid_argument);
}

// --- estimate_heart_rates ---

TEST(HeartRates, MedianOfConfidentRegionsIgnoresNoisyOnes) {
    rppg::RegionPulses pulses = pulses_starting_at(0.0);
    pulses.pulses[rppg::kGlabella] = pulse_at(66.0);
    pulses.pulses[rppg::kRightMalar] = pulse_at(72.0);
    pulses.pulses[rppg::kLeftMalar] = pulse_at(78.0);
    pulses.pulses[rppg::kUpperNasalDorsum] = noise(2);  // would shift a median of four

    const rppg::HeartRates rates = rppg::estimate_heart_rates(pulses);
    ASSERT_TRUE(rates.regions[rppg::kUpperNasalDorsum]);
    EXPECT_LT(rates.regions[rppg::kUpperNasalDorsum]->snr_db, rppg::limits::kMinSnrDb);
    ASSERT_TRUE(rates.regions[rppg::kRightMalar]);
    EXPECT_NEAR(rates.regions[rppg::kRightMalar]->bpm, 72.0, 0.5);
    EXPECT_FALSE(rates.regions[rppg::kForehead]);  // had no pulse
    ASSERT_TRUE(rates.bpm);
    EXPECT_NEAR(*rates.bpm, 72.0, 0.5);
}

TEST(HeartRates, TwoRegionsGiveTheMoreConfidentRateNotTheirAverage) {
    // Averaging 60 and 84 would report 72, which neither region measured.
    std::vector<double> noisy = pulse_at(84.0);
    const std::vector<double> extra = noise(5);
    for (std::size_t n = 0; n < noisy.size(); ++n) {
        noisy[n] += 0.3 * extra[n];
    }
    rppg::RegionPulses pulses = pulses_starting_at(0.0);
    pulses.pulses[rppg::kGlabella] = pulse_at(60.0);
    pulses.pulses[rppg::kLeftMalar] = noisy;

    const rppg::HeartRates rates = rppg::estimate_heart_rates(pulses);
    ASSERT_TRUE(rates.regions[rppg::kLeftMalar]);
    ASSERT_GE(rates.regions[rppg::kLeftMalar]->snr_db, rppg::limits::kMinSnrDb);
    ASSERT_TRUE(rates.bpm);
    EXPECT_NEAR(*rates.bpm, 60.0, 0.5);
}

TEST(HeartRates, OnlyNoiseIsInconclusive) {
    rppg::RegionPulses pulses = pulses_starting_at(0.0);
    pulses.pulses[rppg::kGlabella] = noise(3);
    pulses.pulses[rppg::kLeftMalar] = noise(4);

    const rppg::HeartRates rates = rppg::estimate_heart_rates(pulses);
    EXPECT_TRUE(rates.regions[rppg::kGlabella]);
    EXPECT_FALSE(rates.bpm);
}
