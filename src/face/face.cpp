#include "rppg/face/face.hpp"

#include "rppg/limits.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace rppg {

namespace {

Box clip_to_image(const Box& box, int image_width, int image_height) {
    const double left = std::clamp(box.x, 0.0, static_cast<double>(image_width));
    const double top = std::clamp(box.y, 0.0, static_cast<double>(image_height));
    const double right = std::clamp(box.x + box.width, 0.0, static_cast<double>(image_width));
    const double bottom = std::clamp(box.y + box.height, 0.0, static_cast<double>(image_height));
    return Box{left, top, right - left, bottom - top};
}

}  // namespace

FaceDetector::FaceDetector(const std::string& model_path)
    : detector_(cv::FaceDetectorYN::create(model_path, "", cv::Size(320, 320))) {}

std::vector<FaceGeometry> FaceDetector::detect(const cv::Mat& image) {
    const cv::Mat* input = &image;
    double scale = 1.0;
    if (image.cols > limits::kDetectionWidthPx) {
        scale = static_cast<double>(image.cols) / limits::kDetectionWidthPx;
        const int height = cvRound(image.rows / scale);
        cv::resize(image, resized_, cv::Size(limits::kDetectionWidthPx, height), 0, 0,
                   cv::INTER_AREA);
        input = &resized_;
    }

    detector_->setInputSize(input->size());
    cv::Mat output;
    detector_->detect(*input, output);
    std::vector<FaceGeometry> faces;
    faces.reserve(static_cast<std::size_t>(output.rows));
    for (int row = 0; row < output.rows; ++row) {
        const float* v = output.ptr<float>(row);
        FaceGeometry face;
        face.box = clip_to_image(Box{v[0] * scale, v[1] * scale, v[2] * scale, v[3] * scale},
                                 image.cols, image.rows);
        for (std::size_t l = 0; l < kLandmarkCount; ++l) {
            face.landmarks[l] = Point{v[4 + 2 * l] * scale, v[5 + 2 * l] * scale};
        }
        faces.push_back(face);
    }
    return faces;
}

}  // namespace rppg
