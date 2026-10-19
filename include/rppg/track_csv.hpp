#pragma once

#include "rppg/face_track.hpp"

#include <string>
#include <vector>

namespace rppg {

void write_track_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                     const FaceTrack& detected, const FaceTrack& filled, const Segment& segment,
                     const std::vector<FaceGeometry>& smoothed);

}  // namespace rppg
