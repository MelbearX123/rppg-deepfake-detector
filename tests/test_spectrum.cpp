#include "rppg/heart_rate/spectrum.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRate = 30.0;
constexpr std::size_t kTenSeconds = 300;
constexpr double kMinHz = 0.7;
constexpr double kMaxHz = 4.0;
constexpr double kStepHz = 0.5 / 60.0;  // 0.5 bpm
constexpr double kHalfWidthHz = 0.1;

std::vector<double> sine(double hz, double amplitude = 1.0) {
    std::vector<double> x;
    for (std::size_t n = 0; n < kTenSeconds; ++n) {
        x.push_back(amplitude * std::sin(2.0 * kPi * hz * static_cast<double>(n) / kRate));
    }
    return x;
}

std::vector<double> plus(std::vector<double> a, const std::vector<double>& b) {
    for (std::size_t n = 0; n < a.size(); ++n) {
        a[n] += b[n];
    }
    return a;
}

// Uniform noise in [-1, 1]. Uses mt19937's raw output, which is the same on every platform.
std::vector<double> noise() {
    std::mt19937 generator(42);
    std::vector<double> x;
    for (std::size_t n = 0; n < kTenSeconds; ++n) {
        x.push_back(2.0 * static_cast<double>(generator()) / std::mt19937::max() - 1.0);
    }
    return x;
}

rppg::Spectrum band(const std::vector<double>& x) {
    return rppg::power_spectrum(x, kRate, kMinHz, kMaxHz, kStepHz);
}

double snr(const std::vector<double>& x) {
    const rppg::Spectrum spectrum = band(x);
    return rppg::snr_db(spectrum, rppg::peak_hz(spectrum), kHalfWidthHz);
}

}  // namespace

TEST(Spectrum, GridCoversTheBand) {
    const rppg::Spectrum spectrum = band(sine(1.2));
    EXPECT_DOUBLE_EQ(spectrum.min_hz, kMinHz);
    EXPECT_DOUBLE_EQ(spectrum.step_hz, kStepHz);
    EXPECT_EQ(spectrum.power.size(), 397u);  // 42 to 240 bpm every 0.5 bpm
}

TEST(Spectrum, PeakIsAtTheSineFrequency) {
    EXPECT_NEAR(rppg::peak_hz(band(sine(1.2))) * 60.0, 72.0, 0.5);
}

TEST(Spectrum, FineGridFindsAPeakBetweenFftBins) {
    // A 10 s FFT only has bins every 6 bpm (72, 78); 75 sits halfway between.
    EXPECT_NEAR(rppg::peak_hz(band(sine(1.25))) * 60.0, 75.0, 1.0);
}

TEST(Spectrum, CleanSineHasHighSnrAndNoiseHasLow) {
    EXPECT_GT(snr(sine(1.2)), 10.0);
    EXPECT_LT(snr(noise()), 0.0);
}

TEST(Spectrum, HarmonicCountsAsSignal) {
    // The same extra power helps the SNR at 2x the pulse, and hurts it elsewhere.
    const auto pulse = sine(1.2);
    EXPECT_GT(snr(plus(pulse, sine(2.4, 0.5))), snr(plus(pulse, sine(3.1, 0.5))));
}

TEST(Spectrum, BadInputThrows) {
    EXPECT_THROW(rppg::power_spectrum({}, kRate, kMinHz, kMaxHz, kStepHz), std::invalid_argument);
    EXPECT_THROW(rppg::power_spectrum(sine(1.2), kRate, kMinHz, kMaxHz, 0.0),
                 std::invalid_argument);
    EXPECT_THROW(rppg::power_spectrum(sine(1.2), kRate, kMaxHz, kMinHz, kStepHz),
                 std::invalid_argument);
    EXPECT_THROW(rppg::peak_hz(rppg::Spectrum{}), std::invalid_argument);
}

TEST(Spectrum, SilenceHasLowestPossibleSnr) {
    // A flat pulse must count as "too weak", not stop the whole run.
    EXPECT_EQ(snr(std::vector<double>(kTenSeconds, 0.0)), -std::numeric_limits<double>::infinity());
}
