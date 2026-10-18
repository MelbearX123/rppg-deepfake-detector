#pragma once

#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>

#include <optional>
#include <string>

namespace rppg {

class FaceDetector {
public:
    explicit FaceDetector(const std::string& model_path);

    std::optional<cv::Rect> largest_face(const cv::Mat& image);

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
};

int face_size_px(const cv::Rect& face);

}  // namespace rppg
