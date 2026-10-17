#include "rppg/signal.hpp"

#include <gtest/gtest.h>

TEST(Signal, MeanOfEmptyIsZero) {
    EXPECT_DOUBLE_EQ(rppg::mean({}), 0.0);
}

TEST(Signal, Mean) {
    EXPECT_DOUBLE_EQ(rppg::mean({1.0, 2.0, 3.0, 4.0}), 2.5);
}

TEST(Signal, RemoveMeanCentersOnZero) {
    const auto y = rppg::remove_mean({10.0, 12.0, 14.0});
    ASSERT_EQ(y.size(), 3u);
    EXPECT_DOUBLE_EQ(y[0], -2.0);
    EXPECT_DOUBLE_EQ(y[1], 0.0);
    EXPECT_DOUBLE_EQ(y[2], 2.0);
}
