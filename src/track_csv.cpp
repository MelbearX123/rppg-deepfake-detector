#include "rppg/track_csv.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace rppg {

namespace {

constexpr std::array<const char*, kLandmarkCount> kLandmarkNames = {
    "right_eye", "left_eye", "nose_tip", "right_mouth", "left_mouth"};

void write_header(std::ofstream& out) {
    out << "frame,time_sec,kind,detected,box_x,box_y,box_w,box_h";
    for (const char* name : kLandmarkNames) {
        out << ',' << name << "_x," << name << "_y";
    }
    out << '\n';
}

void write_row(std::ofstream& out, std::size_t frame, double time_sec, const char* kind,
               bool detected, const FaceGeometry& face) {
    out << frame << ',' << time_sec << ',' << kind << ',' << (detected ? 1 : 0) << ',' << face.box.x
        << ',' << face.box.y << ',' << face.box.width << ',' << face.box.height;
    for (const Point& p : face.landmarks) {
        out << ',' << p.x << ',' << p.y;
    }
    out << '\n';
}

}  // namespace

void write_track_csv(const std::string& path, const std::vector<double>& timestamps_sec,
                     const FaceTrack& detected, const FaceTrack& filled, const Segment& segment,
                     const std::vector<FaceGeometry>& smoothed) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }

    write_header(out);
    out << std::fixed << std::setprecision(3);
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        const bool was_detected = detected[i].has_value();
        write_row(out, i, timestamps_sec[i], "raw", was_detected, *filled[i]);
        write_row(out, i, timestamps_sec[i], "smoothed", was_detected, smoothed[i - segment.begin]);
    }
}

}  // namespace rppg
