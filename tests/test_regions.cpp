#include "rppg/regions.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace {

// A face with its eyes at the given points (other landmarks don't matter here).
rppg::FaceGeometry face_with_eyes(rppg::Point right_eye, rppg::Point left_eye) {
    rppg::FaceGeometry face;
    face.landmarks[rppg::kRightEye] = right_eye;
    face.landmarks[rppg::kLeftEye] = left_eye;
    return face;
}

rppg::Point center_of(const rppg::Quad& quad) {
    rppg::Point sum;
    for (const rppg::Point& p : quad) {
        sum.x += p.x / 4.0;
        sum.y += p.y / 4.0;
    }
    return sum;
}

double side_length(const rppg::Quad& quad, std::size_t from, std::size_t to) {
    return std::hypot(quad[to].x - quad[from].x, quad[to].y - quad[from].y);
}

}  // namespace

TEST(Regions, GlabellaIsBetweenAndAboveTheEyes) {
    // Eyes 100 px apart on a level line at y = 100.
    const auto quads = rppg::face_regions(face_with_eyes({100, 100}, {200, 100}));
    const rppg::Point glabella = center_of(quads[rppg::kGlabella]);
    EXPECT_NEAR(glabella.x, 150.0, 1e-9);
    EXPECT_LT(glabella.y, 100.0);
}

TEST(Regions, ForeheadIsAboveGlabellaAndMalarsBelowEyes) {
    const auto quads = rppg::face_regions(face_with_eyes({100, 100}, {200, 100}));
    EXPECT_LT(center_of(quads[rppg::kForehead]).y, center_of(quads[rppg::kGlabella]).y);
    EXPECT_GT(center_of(quads[rppg::kRightMalar]).y, 100.0);
    EXPECT_GT(center_of(quads[rppg::kLeftMalar]).y, 100.0);
}

TEST(Regions, RightMalarIsOnImageLeft) {
    const auto quads = rppg::face_regions(face_with_eyes({100, 100}, {200, 100}));
    EXPECT_LT(center_of(quads[rppg::kRightMalar]).x, 150.0);
    EXPECT_GT(center_of(quads[rppg::kLeftMalar]).x, 150.0);
}

TEST(Regions, RegionsScaleWithEyeDistance) {
    const auto small = rppg::face_regions(face_with_eyes({100, 100}, {200, 100}));
    const auto large = rppg::face_regions(face_with_eyes({100, 100}, {300, 100}));
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        EXPECT_NEAR(side_length(large[r], 0, 1), 2.0 * side_length(small[r], 0, 1), 1e-9);
    }
}

TEST(Regions, RegionsRotateWithHeadTilt) {
    // Head rolled 90 degrees: the eye line runs straight down the image, so
    // "above the eyes" now points toward the image's right.
    const auto quads = rppg::face_regions(face_with_eyes({100, 100}, {100, 200}));
    const rppg::Point glabella = center_of(quads[rppg::kGlabella]);
    EXPECT_NEAR(glabella.y, 150.0, 1e-9);
    EXPECT_GT(glabella.x, 100.0);
}

TEST(Regions, NamesMatchRegions) {
    EXPECT_EQ(std::string(rppg::region_name(rppg::kGlabella)), "glabella");
    EXPECT_EQ(std::string(rppg::region_name(rppg::kLeftMalar)), "left_malar");
}
