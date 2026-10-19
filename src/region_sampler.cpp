#include "rppg/region_sampler.hpp"

#include <opencv2/imgproc.hpp>

#include <vector>

namespace rppg {

RegionPixels pixels_in(const cv::Mat& frame, const Quad& quad) {
    std::vector<cv::Point> corners;
    for (const Point& p : quad) {
        corners.emplace_back(cvRound(p.x), cvRound(p.y));
    }

    const cv::Rect bounds = cv::boundingRect(corners) & cv::Rect(0, 0, frame.cols, frame.rows);
    RegionPixels pixels;
    if (bounds.empty()) {
        return pixels;
    }

    cv::Mat mask = cv::Mat::zeros(bounds.size(), CV_8U);
    for (cv::Point& corner : corners) {
        corner -= bounds.tl();
    }
    cv::fillConvexPoly(mask, corners, cv::Scalar(255));

    const cv::Mat patch = frame(bounds);
    cv::Mat patch_ycrcb;
    cv::cvtColor(patch, patch_ycrcb, cv::COLOR_BGR2YCrCb);

    for (int row = 0; row < patch.rows; ++row) {
        const auto* bgr = patch.ptr<cv::Vec3b>(row);
        const auto* ycc = patch_ycrcb.ptr<cv::Vec3b>(row);
        const auto* inside = mask.ptr<uchar>(row);
        for (int col = 0; col < patch.cols; ++col) {
            if (inside[col] == 0) {
                continue;
            }
            pixels.colors.push_back(
                Color{double(bgr[col][0]), double(bgr[col][1]), double(bgr[col][2])});
            pixels.ycrcb.push_back(
                YCrCb{double(ycc[col][0]), double(ycc[col][1]), double(ycc[col][2])});
        }
    }
    return pixels;
}

}  // namespace rppg
