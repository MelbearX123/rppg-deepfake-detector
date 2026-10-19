#include "rppg/face_geometry.hpp"

#include <algorithm>

namespace rppg {

double area(const Box& box) {
    return std::max(0.0, box.width) * std::max(0.0, box.height);
}

double iou(const Box& a, const Box& b) {
    const double left = std::max(a.x, b.x);
    const double top = std::max(a.y, b.y);
    const double right = std::min(a.x + a.width, b.x + b.width);
    const double bottom = std::min(a.y + a.height, b.y + b.height);
    const double overlap = std::max(0.0, right - left) * std::max(0.0, bottom - top);

    const double combined = area(a) + area(b) - overlap;
    return combined > 0.0 ? overlap / combined : 0.0;
}

double face_size_px(const Box& box) {
    return std::min(box.width, box.height);
}

}  // namespace rppg
