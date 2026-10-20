#include "rppg/eval/metrics.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

TEST(Metrics, WorkedExample) {
    // Errors +2, -3, +5, 0.
    const auto stats = rppg::error_stats({72, 77, 95, 100}, {70, 80, 90, 100});
    EXPECT_EQ(stats.count, 4u);
    EXPECT_DOUBLE_EQ(stats.mae, 2.5);
    EXPECT_NEAR(stats.rmse, std::sqrt(38.0 / 4.0), 1e-12);  // about 3.08
    EXPECT_DOUBLE_EQ(stats.within_5_bpm, 1.0);              // 5 bpm counts as within
    EXPECT_GT(stats.pearson_r, 0.9);
}

TEST(Metrics, PerfectEstimates) {
    const auto stats = rppg::error_stats({60, 75, 90}, {60, 75, 90});
    EXPECT_DOUBLE_EQ(stats.mae, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmse, 0.0);
    EXPECT_NEAR(stats.pearson_r, 1.0, 1e-12);
}

TEST(Metrics, OffByASteadyAmountStillCorrelatesPerfectly) {
    // Why r is never reported alone: every estimate 10 bpm high, yet r = 1.
    const auto stats = rppg::error_stats({70, 85, 100}, {60, 75, 90});
    EXPECT_DOUBLE_EQ(stats.mae, 10.0);
    EXPECT_DOUBLE_EQ(stats.within_5_bpm, 0.0);
    EXPECT_NEAR(stats.pearson_r, 1.0, 1e-12);
}

TEST(Metrics, CorrelationIsUndefinedWhenTruthIsFlat) {
    EXPECT_TRUE(std::isnan(rppg::error_stats({70, 72}, {71, 71}).pearson_r));
}

TEST(Metrics, BadInputThrows) {
    EXPECT_THROW(rppg::error_stats({}, {}), std::invalid_argument);
    EXPECT_THROW(rppg::error_stats({70, 71}, {70}), std::invalid_argument);
}
