#include "rppg/region_signal.hpp"

#include "rppg/limits.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

constexpr rppg::YCrCb kSkin{170, 150, 110};
constexpr rppg::YCrCb kHair{40, 131, 127};

rppg::SkinModel skin_model() {
    return rppg::SkinModel{kSkin.cr, 6.0, kSkin.cb, 6.0, 0.5 * kSkin.y};
}

}  // namespace

TEST(RegionSignal, MeasuresOnlySkinPixels) {
    rppg::RegionPixels pixels;
    for (int i = 0; i < 6; ++i) {
        pixels.colors.push_back({90, 120, 200});
        pixels.ycrcb.push_back(kSkin);
    }
    for (int i = 0; i < 4; ++i) {
        pixels.colors.push_back({20, 20, 25});
        pixels.ycrcb.push_back(kHair);
    }
    const auto sample = rppg::measure_region(pixels, skin_model());
    EXPECT_EQ(sample.skin_pixels, 6u);
    EXPECT_DOUBLE_EQ(sample.coverage, 0.6);
    ASSERT_TRUE(sample.color);
    EXPECT_DOUBLE_EQ(sample.color->r, 200.0);
}

TEST(RegionSignal, EmptyRegionHasNoColorAndZeroCoverage) {
    const auto sample = rppg::measure_region({}, skin_model());
    EXPECT_FALSE(sample.color);
    EXPECT_DOUBLE_EQ(sample.coverage, 0.0);
}

TEST(RegionSignal, MedianCoverageUsesOnlyTheSegment) {
    rppg::RegionSeries series(5);
    for (std::size_t i = 0; i < series.size(); ++i) {
        series[i].coverage = i < 3 ? 0.2 : 0.9;
    }
    EXPECT_DOUBLE_EQ(rppg::median_coverage(series, {0, 5}), 0.2);
    EXPECT_DOUBLE_EQ(rppg::median_coverage(series, {3, 5}), 0.9);
}

TEST(RegionSignal, UsableMeansMostlySkin) {
    EXPECT_FALSE(rppg::is_usable(rppg::limits::kMinRegionCoverage - 0.01));
    EXPECT_TRUE(rppg::is_usable(rppg::limits::kMinRegionCoverage));
}
