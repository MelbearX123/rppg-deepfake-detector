#pragma once

#include "rppg/face/face_track.hpp"
#include "rppg/regions/pixel_stats.hpp"
#include "rppg/regions/regions.hpp"
#include "rppg/regions/skin_model.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace rppg {

struct RegionPixels {
    std::vector<Color> colors;
    std::vector<YCrCb> ycrcb;
};

struct RegionSample {
    std::optional<Color> color;
    std::size_t skin_pixels = 0;
    double coverage = 0.0;
};

using RegionSeries = std::vector<RegionSample>;
using RegionSignals = std::array<RegionSeries, kRegionCount>;

RegionSample measure_region(const RegionPixels& pixels, const SkinModel& model);

double median_coverage(const RegionSeries& series, const Segment& segment);

bool is_usable(double median_coverage);

}  // namespace rppg
