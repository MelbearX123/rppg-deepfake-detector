#include "rppg/regions.hpp"

namespace rppg {

namespace {

struct RegionLayout {
    double across;
    double down;
    double width;
    double height;
};

constexpr std::array<RegionLayout, kRegionCount> kLayouts = {{
    {0.00, -0.30, 0.35, 0.25},  // glabella: between the eyebrows
    {0.00, -0.80, 0.90, 0.40},  // forehead: centered above the glabella
    {0.00, 0.20, 0.20, 0.30},   // upper nasal dorsum: bridge of the nose
    {-0.55, 0.50, 0.35, 0.35},  // right malar: cheekbone below the right eye
    {0.55, 0.50, 0.35, 0.35},   // left malar: cheekbone below the left eye
}};

constexpr std::array<const char*, kRegionCount> kNames = {"glabella", "forehead", "nasal_dorsum",
                                                          "right_malar", "left_malar"};

Point operator+(const Point& a, const Point& b) {
    return {a.x + b.x, a.y + b.y};
}

Point operator-(const Point& a, const Point& b) {
    return {a.x - b.x, a.y - b.y};
}

Point operator*(double k, const Point& p) {
    return {k * p.x, k * p.y};
}

}  // namespace

const char* region_name(Region region) {
    return kNames[region];
}

std::array<Quad, kRegionCount> face_regions(const FaceGeometry& face) {
    const Point& right_eye = face.landmarks[kRightEye];
    const Point& left_eye = face.landmarks[kLeftEye];

    const Point eye_middle = 0.5 * (right_eye + left_eye);
    const Point across = left_eye - right_eye;
    const Point down{-across.y, across.x};

    std::array<Quad, kRegionCount> quads;
    for (std::size_t r = 0; r < kRegionCount; ++r) {
        const RegionLayout& layout = kLayouts[r];
        const Point center = eye_middle + layout.across * across + layout.down * down;
        const Point half_width = (layout.width / 2.0) * across;
        const Point half_height = (layout.height / 2.0) * down;

        quads[r] = {center - half_width - half_height, center + half_width - half_height,
                    center + half_width + half_height, center - half_width + half_height};
    }
    return quads;
}

}  // namespace rppg
