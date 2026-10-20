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

// --- combine_regions / trusted_bpm ---

namespace {

// A region whose windows (one per value) all have the given smoothed bpm and SNR.
rppg::RegionHeartRate region_with(const std::vector<double>& bpm, double snr_db) {
    rppg::RegionHeartRate region;
    for (std::size_t i = 0; i < bpm.size(); ++i) {
        rppg::WindowEstimate w;
        w.center_sec = 5.0 + static_cast<double>(i);
        w.peak_bpm = bpm[i];
        w.smoothed_bpm = bpm[i];
        w.snr_db = snr_db;
        region.windows.push_back(w);
    }
    return region;
}

rppg::FaceWindow face_window(bool trusted, double bpm) {
    rppg::FaceWindow w;
    w.trusted = trusted;
    w.bpm = bpm;
    return w;
}

}  // namespace

TEST(CombineRegions, TakesTheMedianPerWindowAndMeasuresAgreement) {
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({70.0}, 2.0);
    regions[rppg::kRightMalar] = region_with({72.0}, 2.0);
    regions[rppg::kLeftMalar] = region_with({90.0}, 2.0);

    const auto faces = rppg::combine_regions(regions);
    ASSERT_EQ(faces.size(), 1u);
    EXPECT_DOUBLE_EQ(faces[0].center_sec, 5.0);
    EXPECT_DOUBLE_EQ(faces[0].bpm, 72.0);
    EXPECT_DOUBLE_EQ(faces[0].deviation_bpm, 2.0);  // median of |-2|, 0, 18
    EXPECT_EQ(faces[0].regions, 3u);
}

TEST(CombineRegions, AgreeingRegionsAreTrustedEvenWithModestSnr) {
    // Like lightly compressed video: right answer, SNR below the old 0 dB threshold.
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({106.0}, -2.0);
    regions[rppg::kForehead] = region_with({106.5}, -2.0);
    regions[rppg::kRightMalar] = region_with({105.5}, -2.0);
    EXPECT_TRUE(rppg::combine_regions(regions)[0].trusted);
}

TEST(CombineRegions, DisagreeingRegionsAreNotTrusted) {
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({60.0}, 3.0);
    regions[rppg::kRightMalar] = region_with({72.0}, 3.0);
    regions[rppg::kLeftMalar] = region_with({90.0}, 3.0);
    EXPECT_FALSE(rppg::combine_regions(regions)[0].trusted);
}

TEST(CombineRegions, VeryLowSnrIsNotTrustedEvenIfRegionsAgree) {
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({72.0}, -6.0);
    regions[rppg::kRightMalar] = region_with({72.0}, -6.0);
    EXPECT_FALSE(rppg::combine_regions(regions)[0].trusted);
}

TEST(CombineRegions, OneRegionIsNeverTrusted) {
    // With nothing to compare against, agreement means nothing.
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({72.0}, 5.0);
    const auto faces = rppg::combine_regions(regions);
    EXPECT_EQ(faces[0].regions, 1u);
    EXPECT_FALSE(faces[0].trusted);
}

TEST(CombineRegions, LeavesOutTheNasalDorsum) {
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({72.0}, 2.0);
    regions[rppg::kRightMalar] = region_with({72.0}, 2.0);
    regions[rppg::kUpperNasalDorsum] = region_with({150.0}, 2.0);
    const auto faces = rppg::combine_regions(regions);
    EXPECT_EQ(faces[0].regions, 2u);
    EXPECT_DOUBLE_EQ(faces[0].bpm, 72.0);
}

TEST(CombineRegions, NoFaceRegionsGiveNoWindows) {
    rppg::RegionHeartRates regions;
    regions[rppg::kUpperNasalDorsum] = region_with({72.0}, 2.0);
    EXPECT_TRUE(rppg::combine_regions(regions).empty());
}

TEST(CombineRegions, DifferentWindowCountsThrow) {
    rppg::RegionHeartRates regions;
    regions[rppg::kGlabella] = region_with({72.0, 72.0}, 2.0);
    regions[rppg::kRightMalar] = region_with({72.0}, 2.0);
    EXPECT_THROW(rppg::combine_regions(regions), std::invalid_argument);
}

TEST(TrustedBpm, MedianOfTrustedWindowsOnly) {
    // 3 of 4 trusted; the untrusted 140 is ignored.
    const std::vector<rppg::FaceWindow> windows = {face_window(true, 70.0), face_window(true, 72.0),
                                                   face_window(true, 74.0),
                                                   face_window(false, 140.0)};
    ASSERT_TRUE(rppg::trusted_bpm(windows));
    EXPECT_DOUBLE_EQ(*rppg::trusted_bpm(windows), 72.0);
}

TEST(TrustedBpm, TooFewTrustedWindowsIsInconclusive) {
    // 1 of 4 trusted is below kMinTrustedWindowFraction.
    const std::vector<rppg::FaceWindow> windows = {
        face_window(true, 72.0), face_window(false, 60.0), face_window(false, 90.0),
        face_window(false, 140.0)};
    EXPECT_FALSE(rppg::trusted_bpm(windows));
    EXPECT_FALSE(rppg::trusted_bpm({}));
}

// --- estimate_heart_rates ---

TEST(HeartRates, AgreeingRegionsGiveTheirRate) {
    rppg::RegionPulses pulses = pulses_starting_at(0.0);
    pulses.pulses[rppg::kGlabella] = pulse_at(72.0);
    pulses.pulses[rppg::kRightMalar] = pulse_at(72.0);
    pulses.pulses[rppg::kLeftMalar] = pulse_at(72.0);

    const rppg::HeartRates rates = rppg::estimate_heart_rates(pulses);
    EXPECT_FALSE(rates.regions[rppg::kForehead]);  // had no pulse
    ASSERT_EQ(rates.face_windows.size(), 11u);
    for (const auto& window : rates.face_windows) {
        EXPECT_TRUE(window.trusted);
    }
    ASSERT_TRUE(rates.bpm);
    EXPECT_NEAR(*rates.bpm, 72.0, 0.5);
}

TEST(HeartRates, OnlyNoiseIsInconclusive) {
    rppg::RegionPulses pulses = pulses_starting_at(0.0);
    pulses.pulses[rppg::kGlabella] = noise(3);
    pulses.pulses[rppg::kLeftMalar] = noise(4);

    const rppg::HeartRates rates = rppg::estimate_heart_rates(pulses);
    EXPECT_TRUE(rates.regions[rppg::kGlabella]);
    EXPECT_FALSE(rates.bpm);
}
