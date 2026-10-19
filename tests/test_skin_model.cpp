#include "rppg/region_signal.hpp"
#include "rppg/skin_model.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

// Samples scattered around one color, like real skin with sensor noise.
std::vector<rppg::YCrCb> samples_around(double y, double cr, double cb) {
    std::vector<rppg::YCrCb> samples;
    for (int i = 0; i < 100; ++i) {
        const double wobble = (i % 5) - 2.0;  // -2 .. +2
        samples.push_back({y + 3.0 * wobble, cr + wobble, cb - wobble});
    }
    return samples;
}

// Synthetic test colors in YCrCb (illustrative, not measured).
constexpr rppg::YCrCb kLightSkin{170, 150, 110};
constexpr rppg::YCrCb kDarkSkin{70, 145, 118};
constexpr rppg::YCrCb kDarkHair{40, 131, 127};

}  // namespace

TEST(SkinModel, EmptySamplesThrow) {
    EXPECT_THROW(rppg::fit_skin_model({}), std::invalid_argument);
}

TEST(SkinModel, LightSkinKeepsSkinAndRejectsHair) {
    const auto model =
        rppg::fit_skin_model(samples_around(kLightSkin.y, kLightSkin.cr, kLightSkin.cb));
    EXPECT_TRUE(rppg::is_skin(model, kLightSkin));
    EXPECT_FALSE(rppg::is_skin(model, kDarkHair));
}

TEST(SkinModel, DarkSkinKeepsSkinAndRejectsHair) {
    const auto model =
        rppg::fit_skin_model(samples_around(kDarkSkin.y, kDarkSkin.cr, kDarkSkin.cb));
    EXPECT_TRUE(rppg::is_skin(model, kDarkSkin));
    EXPECT_FALSE(rppg::is_skin(model, kDarkHair));
}

TEST(SkinModel, ModerateShadowStillCountsAsSkin) {
    // Same color at 65% of the usual brightness: still skin.
    const auto model =
        rppg::fit_skin_model(samples_around(kLightSkin.y, kLightSkin.cr, kLightSkin.cb));
    EXPECT_TRUE(rppg::is_skin(model, {0.65 * kLightSkin.y, kLightSkin.cr, kLightSkin.cb}));
}

TEST(SkinModel, DarkHairWithSkinLikeColorIsRejectedByBrightness) {
    // The webcam case: hair with the same Cr/Cb as skin, but much darker.
    const auto model = rppg::fit_skin_model(samples_around(185, 133, 132));
    EXPECT_TRUE(rppg::is_skin(model, {185, 133, 132}));
    EXPECT_FALSE(rppg::is_skin(model, {51, 132, 130}));
}

TEST(SkinModel, StaysOnSkinWhenSomeSamplesAreHair) {
    // 30% hair in the fitting samples (e.g. bangs over the forehead).
    auto samples = samples_around(kLightSkin.y, kLightSkin.cr, kLightSkin.cb);
    for (int i = 0; i < 43; ++i) {
        samples.push_back(kDarkHair);
    }
    const auto model = rppg::fit_skin_model(samples);
    EXPECT_TRUE(rppg::is_skin(model, kLightSkin));
    EXPECT_FALSE(rppg::is_skin(model, kDarkHair));
}

// --- measure_region / is_usable ---

TEST(RegionSignal, MeasuresOnlySkinPixels) {
    const auto model =
        rppg::fit_skin_model(samples_around(kLightSkin.y, kLightSkin.cr, kLightSkin.cb));
    rppg::RegionPixels pixels;
    for (int i = 0; i < 6; ++i) {
        pixels.colors.push_back({90, 120, 200});
        pixels.ycrcb.push_back(kLightSkin);
    }
    for (int i = 0; i < 4; ++i) {
        pixels.colors.push_back({20, 20, 25});
        pixels.ycrcb.push_back(kDarkHair);
    }
    const auto sample = rppg::measure_region(pixels, model);
    EXPECT_EQ(sample.skin_pixels, 6u);
    EXPECT_DOUBLE_EQ(sample.coverage, 0.6);
    ASSERT_TRUE(sample.color);
    EXPECT_DOUBLE_EQ(sample.color->r, 200.0);
}

TEST(RegionSignal, UsableDependsOnTypicalCoverage) {
    rppg::RegionSeries series(5);
    for (std::size_t i = 0; i < series.size(); ++i) {
        series[i].coverage = i < 3 ? 0.2 : 0.9;  // mostly hair in a typical frame
    }
    EXPECT_FALSE(rppg::is_usable(series, {0, 5}));
    EXPECT_TRUE(rppg::is_usable(series, {3, 5}));
}
