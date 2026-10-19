#pragma once

#include "rppg/region_signal.hpp"
#include "rppg/regions.hpp"

#include <opencv2/core.hpp>

namespace rppg {

RegionPixels pixels_in(const cv::Mat& frame, const Quad& quad);

}  // namespace rppg
