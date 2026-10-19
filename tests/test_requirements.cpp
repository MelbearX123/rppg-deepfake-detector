#include "rppg/requirements.hpp"

#include "rppg/limits.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

rppg::TimingStats timing_with_duration(double seconds) {
    rppg::TimingStats timing;
    timing.duration_sec = seconds;
    return timing;
}

const std::vector<double> kGoodFaces = {150.0, 152.0, 148.0};

}  // namespace

TEST(Requirements, GoodVideoPasses) {
    const auto result = rppg::check_requirements(timing_with_duration(12.0), 12.0, kGoodFaces, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kOk);
    EXPECT_TRUE(result.reason.empty());
}

TEST(Requirements, TooShortIsRejected) {
    const auto result = rppg::check_requirements(timing_with_duration(9.9), 9.9, kGoodFaces, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kTooShort);
    EXPECT_EQ(result.reason, "video is 9.90 s, minimum is 10 s");
}

TEST(Requirements, ExactlyMinimumDurationPasses) {
    const double minimum = rppg::limits::kMinDurationSec;
    const auto result =
        rppg::check_requirements(timing_with_duration(minimum), minimum, kGoodFaces, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kOk);
}

TEST(Requirements, NoFaceIsRejected) {
    const auto result = rppg::check_requirements(timing_with_duration(12.0), 0.0, {}, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kNoFace);
}

TEST(Requirements, ShortTrackedStretchIsRejected) {
    // A long video where the face is never tracked for 10 s without a break.
    const auto result = rppg::check_requirements(timing_with_duration(60.0), 7.5, kGoodFaces, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kTrackTooShort);
    EXPECT_EQ(result.reason, "face tracked for at most 7.50 s in a row, minimum is 10 s");
}

TEST(Requirements, SmallFaceIsRejected) {
    const auto result =
        rppg::check_requirements(timing_with_duration(12.0), 12.0, {80.0, 90.0, 85.0}, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kFaceTooSmall);
    EXPECT_EQ(result.reason, "face is 85 px, minimum is 100 px");
}

TEST(Requirements, FaceSizeUsesMedianSoOneBadDetectionDoesNotCount) {
    // One wildly wrong 400 px detection must not lift a small face over the limit.
    const auto result =
        rppg::check_requirements(timing_with_duration(12.0), 12.0, {80.0, 85.0, 400.0}, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kFaceTooSmall);
}

TEST(Requirements, LengthIsCheckedBeforeFace) {
    const auto result = rppg::check_requirements(timing_with_duration(5.0), 0.0, {}, 5);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kTooShort);
}

TEST(Requirements, NoUsableRegionIsRejected) {
    const auto result = rppg::check_requirements(timing_with_duration(12.0), 12.0, kGoodFaces, 0);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kNoUsableRegion);
}

TEST(Requirements, FaceSizeIsCheckedBeforeRegions) {
    const auto result =
        rppg::check_requirements(timing_with_duration(12.0), 12.0, {80.0, 85.0, 90.0}, 0);
    EXPECT_EQ(result.status, rppg::RequirementStatus::kFaceTooSmall);
}
