#pragma once

#include "rppg/face_track.hpp"

#include <string>
#include <vector>

namespace rppg {

void write_track_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                     const FaceTrack& detected, const FaceTrack& filled, const FaceTrack& smoothed,
                     const Segment& segment);

}  // namespace rppg
