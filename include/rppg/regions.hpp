#pragma once

#include "rppg/face_geometry.hpp"

#include <array>
#include <cstddef>

namespace rppg {

enum Region : std::size_t {
    kGlabella = 0,
    kForehead,
    kUpperNasalDorsum,
    kRightMalar,
    kLeftMalar,
    kRegionCount,
};

const char* region_name(Region region);

using Quad = std::array<Point, 4>;

std::array<Quad, kRegionCount> face_regions(const FaceGeometry& face);

}  // namespace rppg
