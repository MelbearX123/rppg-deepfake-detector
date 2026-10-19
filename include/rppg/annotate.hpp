#pragma once

#include "rppg/face_geometry.hpp"

#include <opencv2/core.hpp>

namespace rppg {

void draw_face(cv::Mat& image, const FaceGeometry& face, const cv::Scalar& color);

}  // namespace rppg
