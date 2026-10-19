#pragma once

#include "rppg/regions/region_signal.hpp"
#include "rppg/regions/regions.hpp"

#include <opencv2/core.hpp>

namespace rppg {

RegionPixels pixels_in(const cv::Mat& frame, const Quad& quad);

}  // namespace rppg
