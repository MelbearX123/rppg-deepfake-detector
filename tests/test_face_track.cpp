#include "rppg/face_track.hpp"

#include "rppg/limits.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

// A face whose box has its top-left corner at (x, y), with landmarks placed
// at fixed offsets inside the box so they move with it.
rppg::FaceGeometry face_at(double x, double y, double size = 100.0) {
    rppg::FaceGeometry face;
    face.box = rppg::Box{x, y, size, size};
    for (std::size_t l = 0; l < rppg::kLandmarkCount; ++l) {
        face.landmarks[l] = rppg::Point{x + 10.0 * static_cast<double>(l), y + 50.0};
    }
    return face;
}

}  // namespace

// --- iou ---

TEST(FaceGeometry, IouOfIdenticalBoxesIsOne) {
    const rppg::Box box{0, 0, 10, 10};
    EXPECT_DOUBLE_EQ(rppg::iou(box, box), 1.0);
}

TEST(FaceGeometry, IouOfSeparateBoxesIsZero) {
    EXPECT_DOUBLE_EQ(rppg::iou({0, 0, 10, 10}, {20, 0, 10, 10}), 0.0);
}

TEST(FaceGeometry, IouOfHalfShiftedBoxes) {
    // Overlap 50, combined 100 + 100 - 50 = 150.
    EXPECT_DOUBLE_EQ(rppg::iou({0, 0, 10, 10}, {5, 0, 10, 10}), 50.0 / 150.0);
}

// --- FaceFollower ---

TEST(FaceFollower, StartsWithLargestFace) {
    rppg::FaceFollower follower;
    const auto face = follower.next({face_at(0, 0, 50), face_at(300, 0, 120)}, 0.0);
    ASSERT_TRUE(face);
    EXPECT_DOUBLE_EQ(face->box.x, 300.0);
}

TEST(FaceFollower, StaysOnSamePersonWhenALargerFaceAppears) {
    rppg::FaceFollower follower;
    follower.next({face_at(0, 0, 100)}, 0.0);
    const auto face = follower.next({face_at(2, 0, 100), face_at(300, 0, 200)}, 0.033);
    ASSERT_TRUE(face);
    EXPECT_DOUBLE_EQ(face->box.x, 2.0);
}

TEST(FaceFollower, NoMatchWhenOnlySomeoneElseIsVisible) {
    rppg::FaceFollower follower;
    follower.next({face_at(0, 0)}, 0.0);
    EXPECT_FALSE(follower.next({face_at(500, 0)}, 0.033));
}

TEST(FaceFollower, RelocksOntoLargestFaceAfterLongLoss) {
    rppg::FaceFollower follower;
    follower.next({face_at(0, 0)}, 0.0);
    const double after_long_gap = rppg::limits::kMaxGapSec + 0.1;
    const auto face = follower.next({face_at(500, 0)}, after_long_gap);
    ASSERT_TRUE(face);
    EXPECT_DOUBLE_EQ(face->box.x, 500.0);
}

// --- fill_short_gaps / longest_segment ---

TEST(FaceTrack, FillsShortGapByInterpolation) {
    const rppg::FaceTrack track = {face_at(0, 0), std::nullopt, std::nullopt, face_at(30, 0)};
    const auto filled = rppg::fill_short_gaps(track, 2);
    ASSERT_TRUE(filled[1] && filled[2]);
    EXPECT_DOUBLE_EQ(filled[1]->box.x, 10.0);
    EXPECT_DOUBLE_EQ(filled[2]->landmarks[rppg::kNoseTip].x, 20.0 + 20.0);
}

TEST(FaceTrack, LeavesLongGapMissing) {
    const rppg::FaceTrack track = {face_at(0, 0), std::nullopt, std::nullopt, face_at(30, 0)};
    const auto filled = rppg::fill_short_gaps(track, 1);
    EXPECT_FALSE(filled[1]);
    EXPECT_FALSE(filled[2]);
}

TEST(FaceTrack, LongestSegmentPicksLongestRun) {
    const rppg::FaceTrack track = {face_at(0, 0), std::nullopt,  face_at(0, 0),
                                   face_at(0, 0), face_at(0, 0), std::nullopt};
    const auto segment = rppg::longest_segment(track);
    ASSERT_TRUE(segment);
    EXPECT_EQ(segment->begin, 2u);
    EXPECT_EQ(segment->end, 5u);
}

TEST(FaceTrack, LongestSegmentTieGoesToEarliest) {
    const rppg::FaceTrack track = {face_at(0, 0), std::nullopt, face_at(0, 0)};
    EXPECT_EQ(rppg::longest_segment(track)->begin, 0u);
}

TEST(FaceTrack, NoSegmentWithoutFaces) {
    EXPECT_FALSE(rppg::longest_segment({std::nullopt, std::nullopt}));
}

// --- smooth_segment / landmark_jitter_px ---

TEST(FaceTrack, SmoothingDoesNotLagSteadyMotion) {
    // Moving 2 px per frame: a centered average of a straight line is the line
    // itself, so the smoothed position must be exactly where the face is.
    rppg::FaceTrack track;
    for (int i = 0; i < 20; ++i) {
        track.push_back(face_at(2.0 * i, 0));
    }
    const auto smoothed = rppg::smooth_segment(track, {0, track.size()}, 5);
    for (std::size_t i = 0; i < track.size(); ++i) {
        EXPECT_NEAR(smoothed[i]->box.x, track[i]->box.x, 1e-9);
    }
}

TEST(FaceTrack, SmoothingReducesJitter) {
    // Still face, wobbling 2 px left and right every frame.
    rppg::FaceTrack track;
    for (int i = 0; i < 20; ++i) {
        track.push_back(face_at(i % 2 == 0 ? 2.0 : -2.0, 0));
    }
    const double before = rppg::landmark_jitter_px(track);
    const double after =
        rppg::landmark_jitter_px(rppg::smooth_segment(track, {0, track.size()}, 5));
    EXPECT_DOUBLE_EQ(before, 8.0);  // |2 - 2(-2) + 2| = 8
    EXPECT_LT(after, before / 4.0);
}

TEST(FaceTrack, SmoothingLeavesFramesOutsideSegmentEmpty) {
    const rppg::FaceTrack track = {face_at(0, 0), face_at(1, 0), face_at(2, 0), face_at(3, 0)};
    const auto smoothed = rppg::smooth_segment(track, {1, 3}, 1);
    ASSERT_EQ(smoothed.size(), track.size());
    EXPECT_FALSE(smoothed[0]);
    EXPECT_TRUE(smoothed[1] && smoothed[2]);
    EXPECT_FALSE(smoothed[3]);
}

TEST(FaceTrack, JitterOfSteadyMotionIsZero) {
    rppg::FaceTrack track;
    for (int i = 0; i < 10; ++i) {
        track.push_back(face_at(3.0 * i, 1.0 * i));
    }
    EXPECT_NEAR(rppg::landmark_jitter_px(track), 0.0, 1e-9);
}

TEST(FaceTrack, JitterSkipsFramesNextToGaps) {
    const rppg::FaceTrack track = {face_at(0, 0),  face_at(1, 0),  face_at(2, 0), std::nullopt,
                                   face_at(50, 0), face_at(51, 0), face_at(52, 0)};
    EXPECT_NEAR(rppg::landmark_jitter_px(track), 0.0, 1e-9);
}
