#include "rppg/regions/pixel_stats.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

rppg::Color gray(double level) {
    return {level, level, level};
}

}  // namespace

TEST(TrimmedMean, EmptyGivesNothing) {
    EXPECT_FALSE(rppg::trimmed_mean({}, 0.1));
}

TEST(TrimmedMean, DropsGlareAndDarkPixels) {
    // 18 skin pixels at 100, plus one glare pixel and one hair pixel.
    std::vector<rppg::Color> pixels(18, gray(100));
    pixels.push_back(gray(255));
    pixels.push_back(gray(0));
    const auto mean = rppg::trimmed_mean(pixels, 0.05);
    ASSERT_TRUE(mean);
    EXPECT_DOUBLE_EQ(mean->g, 100.0);
}

TEST(TrimmedMean, KeepsFractionalPrecision) {
    // Whole-number pixels can still average to a fraction; a median could not.
    const auto mean = rppg::trimmed_mean({gray(100), gray(101)}, 0.0);
    ASSERT_TRUE(mean);
    EXPECT_DOUBLE_EQ(mean->g, 100.5);
}

TEST(TrimmedMean, DropsWholePixelsNotSingleChannels) {
    // The brightest pixel is bright because of its green, but its red is low.
    // Trimming by brightness drops it entirely, so its low red is gone too.
    std::vector<rppg::Color> pixels(8, rppg::Color{100, 100, 100});
    pixels.push_back(rppg::Color{100, 255, 0});
    pixels.push_back(rppg::Color{0, 0, 0});
    const auto mean = rppg::trimmed_mean(pixels, 0.1);
    ASSERT_TRUE(mean);
    EXPECT_DOUBLE_EQ(mean->r, 100.0);
    EXPECT_DOUBLE_EQ(mean->g, 100.0);
}

TEST(TrimmedMean, TooFewPixelsToTrimKeepsAll) {
    const auto mean = rppg::trimmed_mean({gray(10), gray(20)}, 0.5);
    ASSERT_TRUE(mean);
    EXPECT_DOUBLE_EQ(mean->g, 15.0);
}
