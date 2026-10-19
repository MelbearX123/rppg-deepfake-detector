#include "rppg/skin_model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
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

// Measured on a webcam video: hair with nearly the same Cr/Cb as the skin.
constexpr rppg::YCrCb kWebcamSkin{185, 133, 132};
constexpr rppg::YCrCb kWebcamHair{51, 132, 130};

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
    const auto model =
        rppg::fit_skin_model(samples_around(kWebcamSkin.y, kWebcamSkin.cr, kWebcamSkin.cb));
    EXPECT_TRUE(rppg::is_skin(model, kWebcamSkin));
    EXPECT_FALSE(rppg::is_skin(model, kWebcamHair));
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

// --- append_evenly_spaced ---

TEST(SkinModel, EveryRegionContributesTheSameNumberOfSamples) {
    const std::vector<rppg::YCrCb> big(2000, kLightSkin);
    const std::vector<rppg::YCrCb> small(999, kDarkSkin);
    std::vector<rppg::YCrCb> samples;
    rppg::append_evenly_spaced(samples, big, 500);
    rppg::append_evenly_spaced(samples, small, 500);
    ASSERT_EQ(samples.size(), 1000u);
    EXPECT_EQ(std::count_if(samples.begin(), samples.end(),
                            [](const rppg::YCrCb& p) { return p.y == kLightSkin.y; }),
              500);
}

TEST(SkinModel, SmallRegionGivesAllItsPixels) {
    const std::vector<rppg::YCrCb> region(30, kLightSkin);
    std::vector<rppg::YCrCb> samples;
    rppg::append_evenly_spaced(samples, region, 500);
    EXPECT_EQ(samples.size(), 30u);
}

TEST(SkinModel, SamplesSpreadAcrossTheRegion) {
    // Pixels numbered 0..9 by Y; taking 5 should pick every other one.
    std::vector<rppg::YCrCb> region;
    for (int i = 0; i < 10; ++i) {
        region.push_back({static_cast<double>(i), 0, 0});
    }
    std::vector<rppg::YCrCb> samples;
    rppg::append_evenly_spaced(samples, region, 5);
    ASSERT_EQ(samples.size(), 5u);
    for (std::size_t k = 0; k < samples.size(); ++k) {
        EXPECT_DOUBLE_EQ(samples[k].y, 2.0 * k);
    }
}
