#include "rppg/annotate.hpp"

#include <opencv2/imgproc.hpp>

#include <vector>

namespace rppg {

void draw_face(cv::Mat& image, const FaceGeometry& face, const cv::Scalar& color) {
    const cv::Rect box(cvRound(face.box.x), cvRound(face.box.y), cvRound(face.box.width),
                       cvRound(face.box.height));
    cv::rectangle(image, box, color, 2);
    for (const Point& p : face.landmarks) {
        cv::circle(image, cv::Point(cvRound(p.x), cvRound(p.y)), 3, color, cv::FILLED);
    }
}

void draw_quad(cv::Mat& image, const Quad& quad, const cv::Scalar& color) {
    std::vector<cv::Point> corners;
    for (const Point& p : quad) {
        corners.emplace_back(cvRound(p.x), cvRound(p.y));
    }
    cv::polylines(image, corners, true, color, 2);
}

}  // namespace rppg
