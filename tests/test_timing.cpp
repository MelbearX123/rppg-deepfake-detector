#include "rppg/timing.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

// Timestamps for `count` frames at a constant frame rate.
std::vector<double> constant_rate(int count, double fps) {
    std::vector<double> t;
    for (int i = 0; i < count; ++i) {
        t.push_back(i / fps);
    }
    return t;
}

}  // namespace

TEST(Timing, ConstantFrameRate) {
    const auto stats = rppg::analyze_timing(constant_rate(300, 30.0));
    EXPECT_EQ(stats.frame_count, 300u);
    EXPECT_NEAR(stats.measured_fps, 30.0, 1e-9);
    EXPECT_NEAR(stats.duration_sec, 10.0, 1e-9);  // 300 frames at 30 fps
    EXPECT_NEAR(stats.interval_variation, 0.0, 1e-9);
    EXPECT_FALSE(stats.is_variable_frame_rate);
    EXPECT_EQ(stats.non_increasing_count, 0u);
}

TEST(Timing, AlternatingIntervalsAreVariable) {
    // Gaps alternate between 25 ms and 41 ms: 30 fps on average, but uneven.
    std::vector<double> t = {0.0};
    for (int i = 1; i < 300; ++i) {
        t.push_back(t.back() + (i % 2 == 0 ? 0.025 : 0.041));
    }
    const auto stats = rppg::analyze_timing(t);
    EXPECT_TRUE(stats.is_variable_frame_rate);
    EXPECT_GT(stats.interval_variation, 0.2);
}

TEST(Timing, SmallJitterIsStillConstant) {
    // +-0.5 ms of jitter on 33.3 ms frames is about 1.5% variation: still constant.
    auto t = constant_rate(300, 30.0);
    for (std::size_t i = 0; i < t.size(); ++i) {
        t[i] += (i % 2 == 0 ? 0.0005 : -0.0005);
    }
    EXPECT_FALSE(rppg::analyze_timing(t).is_variable_frame_rate);
}

TEST(Timing, CountsBackwardAndRepeatedTimestamps) {
    const auto stats = rppg::analyze_timing({0.0, 0.1, 0.1, 0.05, 0.2});
    EXPECT_EQ(stats.non_increasing_count, 2u);
}

TEST(Timing, FewerThanTwoFramesGivesEmptyStats) {
    const auto stats = rppg::analyze_timing({1.0});
    EXPECT_EQ(stats.frame_count, 1u);
    EXPECT_DOUBLE_EQ(stats.duration_sec, 0.0);
    EXPECT_DOUBLE_EQ(stats.measured_fps, 0.0);
}
