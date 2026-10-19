#pragma once

#include "rppg/face/face_geometry.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>

#include <string>
#include <vector>

namespace rppg {

class FaceDetector {
public:
    explicit FaceDetector(const std::string& model_path);

    std::vector<FaceGeometry> detect(const cv::Mat& image);

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Mat resized_;
};

}  // namespace rppg
