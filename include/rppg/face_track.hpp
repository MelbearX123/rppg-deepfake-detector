#pragma once

#include "rppg/face_geometry.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace rppg {

using FaceTrack = std::vector<std::optional<FaceGeometry>>;

class FaceFollower {
public:
    std::optional<FaceGeometry> next(const std::vector<FaceGeometry>& detections, double time_sec);

private:
    std::optional<FaceGeometry> last_seen_;
    double last_seen_time_sec_ = 0.0;
};

FaceTrack fill_short_gaps(const FaceTrack& track, std::size_t max_gap_frames);

struct Segment {
    std::size_t begin = 0;
    std::size_t end = 0;

    std::size_t size() const { return end - begin; }
};

std::optional<Segment> longest_segment(const FaceTrack& track);

FaceTrack smooth_segment(const FaceTrack& track, const Segment& segment, std::size_t window_frames);

double landmark_jitter_px(const FaceTrack& track);

}  // namespace rppg
