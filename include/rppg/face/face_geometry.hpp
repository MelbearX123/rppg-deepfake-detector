#pragma once

#include <array>
#include <cstddef>

namespace rppg {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Box {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

enum Landmark : std::size_t {
    kRightEye = 0,
    kLeftEye,
    kNoseTip,
    kRightMouthCorner,
    kLeftMouthCorner,
    kLandmarkCount,
};

struct FaceGeometry {
    Box box;
    std::array<Point, kLandmarkCount> landmarks;
};

double area(const Box& box);

double iou(const Box& a, const Box& b);

double face_size_px(const Box& box);

}  // namespace rppg
