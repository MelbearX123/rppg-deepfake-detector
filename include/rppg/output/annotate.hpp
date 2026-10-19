#pragma once

#include "rppg/face/face_geometry.hpp"
#include "rppg/regions/regions.hpp"

#include <opencv2/core.hpp>

namespace rppg {

void draw_face(cv::Mat& image, const FaceGeometry& face, const cv::Scalar& color);
void draw_quad(cv::Mat& image, const Quad& quad, const cv::Scalar& color);

}  // namespace rppg
