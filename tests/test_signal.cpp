#include "rppg/signal.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

// --- mean / median / stddev ---

TEST(Signal, EmptyInputThrows) {
    EXPECT_THROW(rppg::mean({}), std::invalid_argument);
    EXPECT_THROW(rppg::median({}), std::invalid_argument);
    EXPECT_THROW(rppg::stddev({}), std::invalid_argument);
}

TEST(Signal, Mean) {
    EXPECT_DOUBLE_EQ(rppg::mean({1.0, 2.0, 3.0, 4.0}), 2.5);
}

TEST(Signal, MedianOddSize) {
    EXPECT_DOUBLE_EQ(rppg::median({5.0, 1.0, 3.0}), 3.0);
}

TEST(Signal, MedianEvenSizeAveragesMiddleTwo) {
    EXPECT_DOUBLE_EQ(rppg::median({4.0, 1.0, 3.0, 2.0}), 2.5);
}

TEST(Signal, StddevOfConstantIsZero) {
    EXPECT_DOUBLE_EQ(rppg::stddev({3.0, 3.0, 3.0}), 0.0);
}

TEST(Signal, StddevDividesByN) {
    // Mean 5, squared deviations sum to 32, 32 / 8 = 4, sqrt(4) = 2.
    EXPECT_DOUBLE_EQ(rppg::stddev({2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}), 2.0);
}

// --- resample_uniform ---

TEST(Resample, EvenInputAtSameRateIsUnchanged) {
    const std::vector<double> t = {0.0, 0.1, 0.2, 0.3};
    const std::vector<double> v = {1.0, 5.0, 2.0, 7.0};
    const auto out = rppg::resample_uniform(t, v, 10.0);
    ASSERT_EQ(out.size(), v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        EXPECT_NEAR(out[i], v[i], 1e-12);
    }
}

TEST(Resample, UnevenInputInterpolatesLinearly) {
    // Samples of the line v = 10 * t at uneven times. Linear interpolation of a
    // straight line is exact, so every output must sit on the same line.
    const std::vector<double> t = {0.0, 0.03, 0.11, 0.2, 0.31, 0.4};
    std::vector<double> v;
    for (double ti : t) {
        v.push_back(10.0 * ti);
    }
    const auto out = rppg::resample_uniform(t, v, 10.0);
    ASSERT_EQ(out.size(), 5u);  // 0.0, 0.1, 0.2, 0.3, 0.4
    for (std::size_t k = 0; k < out.size(); ++k) {
        EXPECT_NEAR(out[k], 10.0 * (0.1 * static_cast<double>(k)), 1e-9);
    }
}

TEST(Resample, OutputStopsAtLastInputTime) {
    // 0.0 to 0.25 at 10 Hz: 0.0, 0.1, 0.2. A sample at 0.3 would be past the
    // data and would need extrapolation, so it must not appear.
    const auto out = rppg::resample_uniform({0.0, 0.25}, {0.0, 1.0}, 10.0);
    EXPECT_EQ(out.size(), 3u);
}

TEST(Resample, RejectsBadInput) {
    EXPECT_THROW(rppg::resample_uniform({0.0, 1.0}, {1.0}, 10.0), std::invalid_argument);
    EXPECT_THROW(rppg::resample_uniform({0.0}, {1.0}, 10.0), std::invalid_argument);
    EXPECT_THROW(rppg::resample_uniform({0.0, 1.0}, {1.0, 2.0}, 0.0), std::invalid_argument);
    EXPECT_THROW(rppg::resample_uniform({0.0, 0.5, 0.5}, {1.0, 2.0, 3.0}, 10.0),
                 std::invalid_argument);
}
