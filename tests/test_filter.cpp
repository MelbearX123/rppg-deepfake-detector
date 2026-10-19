#include "rppg/filter.hpp"

#include "rppg/limits.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRate = 30.0;

std::vector<double> sine(double hz, std::size_t count, double phase = 0.0) {
    std::vector<double> x;
    for (std::size_t k = 0; k < count; ++k) {
        x.push_back(std::sin(2.0 * kPi * hz * static_cast<double>(k) / kRate + phase));
    }
    return x;
}

// Amplitude of a sine from its RMS, skipping the first `skip` samples (start-up).
double amplitude(const std::vector<double>& y, std::size_t skip) {
    double sum_squares = 0.0;
    for (std::size_t k = skip; k < y.size(); ++k) {
        sum_squares += y[k] * y[k];
    }
    return std::sqrt(2.0 * sum_squares / static_cast<double>(y.size() - skip));
}

// |H| at hz, straight from the coefficients.
double gain_at(const rppg::Biquad& q, double hz) {
    const std::complex<double> z1 = std::polar(1.0, -2.0 * kPi * hz / kRate);
    const std::complex<double> z2 = z1 * z1;
    return std::abs((q.b0 + q.b1 * z1 + q.b2 * z2) / (1.0 + q.a1 * z1 + q.a2 * z2));
}

// What the zero-phase bandpass should do to a sine at hz: each filter runs twice.
double bandpass_gain(double hz) {
    const double one_pass =
        gain_at(rppg::butterworth_high_pass(rppg::limits::kMinPulseHz, kRate), hz) *
        gain_at(rppg::butterworth_low_pass(rppg::limits::kMaxPulseHz, kRate), hz);
    return one_pass * one_pass;
}

std::vector<double> bandpass(const std::vector<double>& x) {
    const auto pad = static_cast<std::size_t>(std::lround(rppg::limits::kFilterPadSec * kRate));
    return rppg::bandpass_zero_phase(x, kRate, rppg::limits::kMinPulseHz, rppg::limits::kMaxPulseHz,
                                     pad);
}

}  // namespace

// --- biquad designs ---

TEST(Biquad, LowPassKeepsConstantLevel) {
    const auto y =
        rppg::filter_forward(rppg::butterworth_low_pass(4.0, kRate), std::vector<double>(300, 5.0));
    EXPECT_NEAR(y.back(), 5.0, 1e-9);
}

TEST(Biquad, LowPassRemovesFastFlicker) {
    const auto y = rppg::filter_forward(rppg::butterworth_low_pass(4.0, kRate), sine(14.0, 600));
    EXPECT_LT(amplitude(y, 90), 0.05);
}

TEST(Biquad, HighPassRemovesConstantLevel) {
    const auto y = rppg::filter_forward(rppg::butterworth_high_pass(0.7, kRate),
                                        std::vector<double>(300, 5.0));
    EXPECT_NEAR(y.back(), 0.0, 1e-6);
}

TEST(Biquad, CutoffIsHalfPower) {
    // Butterworth's defining property: a sine at the cutoff comes out at 1/sqrt(2).
    const double half_power = 1.0 / std::sqrt(2.0);
    const auto low = rppg::filter_forward(rppg::butterworth_low_pass(4.0, kRate), sine(4.0, 900));
    const auto high = rppg::filter_forward(rppg::butterworth_high_pass(0.7, kRate), sine(0.7, 900));
    EXPECT_NEAR(amplitude(low, 90), half_power, 0.01);
    EXPECT_NEAR(amplitude(high, 90), half_power, 0.01);
}

TEST(Biquad, CutoffOutsideRangeThrows) {
    for (double cutoff : {0.0, -1.0, kRate / 2.0, 20.0}) {
        EXPECT_THROW(rppg::butterworth_low_pass(cutoff, kRate), std::invalid_argument);
        EXPECT_THROW(rppg::butterworth_high_pass(cutoff, kRate), std::invalid_argument);
    }
}

// --- bandpass_zero_phase ---

TEST(Bandpass, KeepsPulseInPlace) {
    // Away from the edges, a 72 bpm sine comes out scaled but not shifted in time.
    const auto x = sine(1.2, 600);
    const auto y = bandpass(x);
    ASSERT_EQ(y.size(), x.size());
    const double gain = bandpass_gain(1.2);
    EXPECT_GT(gain, 0.85);
    for (std::size_t k = 60; k < x.size() - 60; ++k) {
        EXPECT_NEAR(y[k], gain * x[k], 0.002);
    }
}

TEST(Bandpass, RemovesDriftAndFlicker) {
    const auto drift = sine(0.1, 600);
    const auto flicker = sine(8.0, 600);
    std::vector<double> x;
    for (std::size_t k = 0; k < drift.size(); ++k) {
        x.push_back(drift[k] + flicker[k]);
    }
    const auto y = bandpass(x);
    for (std::size_t k = 60; k < y.size() - 60; ++k) {
        EXPECT_LT(std::abs(y[k]), 0.05);
    }
}

TEST(Bandpass, EdgesSettleWithinTwoSeconds) {
    // Worst case over the band and the phase the clip ends on, with an offset
    // and drift that should be removed. Measured: 2.4% past 1 s, 0.1% past 2 s.
    const std::size_t count = 300;  // 10 s, the shortest clip the app accepts
    double worst_past_1s = 0.0;
    double worst_past_2s = 0.0;
    for (double hz : {0.7, 1.0, 1.2, 2.0, 3.0, 4.0}) {
        for (int step = 0; step < 16; ++step) {
            const auto pulse = sine(hz, count, step * kPi / 8.0);
            std::vector<double> x;
            for (std::size_t k = 0; k < count; ++k) {
                x.push_back(pulse[k] + 5.0 + 3.0 * static_cast<double>(k) / count);
            }
            const auto y = bandpass(x);
            const double gain = bandpass_gain(hz);
            for (std::size_t k = 0; k < count; ++k) {
                const double error = std::abs(y[k] - gain * pulse[k]);
                const double edge_sec = static_cast<double>(std::min(k, count - 1 - k)) / kRate;
                if (edge_sec >= 1.0) {
                    worst_past_1s = std::max(worst_past_1s, error);
                }
                if (edge_sec >= 2.0) {
                    worst_past_2s = std::max(worst_past_2s, error);
                }
            }
        }
    }
    EXPECT_LT(worst_past_1s, 0.03);
    EXPECT_LT(worst_past_2s, 0.005);
}

TEST(Bandpass, BadInputThrows) {
    EXPECT_THROW(rppg::bandpass_zero_phase({}, kRate, 0.7, 4.0, 60), std::invalid_argument);
    EXPECT_THROW(rppg::bandpass_zero_phase(sine(1.0, 100), kRate, 4.0, 0.7, 60),
                 std::invalid_argument);
}
