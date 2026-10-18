#include "rppg/face.hpp"

#include <algorithm>

namespace rppg {

FaceDetector::FaceDetector(const std::string& model_path)
    //placeholder input size
    : detector_(cv::FaceDetectorYN::create(model_path, "", cv::Size(320, 320))) {}

std::optional<cv::Rect> FaceDetector::largest_face(const cv::Mat& image) {
    detector_->setInputSize(image.size());

    cv::Mat faces;
    detector_->detect(image, faces);
    if (faces.empty()) {
        return std::nullopt;
    }

    std::optional<cv::Rect> largest;
    for (int row = 0; row < faces.rows; ++row) {
        const float* f = faces.ptr<float>(row);
        const cv::Rect box(cvRound(f[0]), cvRound(f[1]), cvRound(f[2]), cvRound(f[3]));
        if (!largest || box.area() > largest->area()) {
            largest = box;
        }
    }
    return largest;
}

int face_size_px(const cv::Rect& face) {
    return std::min(face.width, face.height);
}

}  // namespace rppg
