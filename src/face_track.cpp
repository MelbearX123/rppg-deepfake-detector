#include "rppg/face_track.hpp"

#include "rppg/limits.hpp"
#include "rppg/signal.hpp"

#include <array>
#include <cmath>

namespace rppg {

namespace {

constexpr std::size_t kCoordCount = 4 + 2 * kLandmarkCount;
using Coords = std::array<double, kCoordCount>;

Coords to_coords(const FaceGeometry& face) {
    Coords c{face.box.x, face.box.y, face.box.width, face.box.height};
    for (std::size_t l = 0; l < kLandmarkCount; ++l) {
        c[4 + 2 * l] = face.landmarks[l].x;
        c[5 + 2 * l] = face.landmarks[l].y;
    }
    return c;
}

FaceGeometry from_coords(const Coords& c) {
    FaceGeometry face;
    face.box = Box{c[0], c[1], c[2], c[3]};
    for (std::size_t l = 0; l < kLandmarkCount; ++l) {
        face.landmarks[l] = Point{c[4 + 2 * l], c[5 + 2 * l]};
    }
    return face;
}

}  // namespace

std::optional<FaceGeometry> FaceFollower::next(const std::vector<FaceGeometry>& detections,
                                               double time_sec) {
    if (last_seen_ && time_sec - last_seen_time_sec_ > limits::kMaxGapSec) {
        last_seen_.reset();
    }

    const FaceGeometry* best = nullptr;
    double best_score = 0.0;
    for (const FaceGeometry& face : detections) {
        const double score = last_seen_ ? iou(face.box, last_seen_->box) : area(face.box);
        if (score > best_score) {
            best = &face;
            best_score = score;
        }
    }

    const bool too_little_overlap = last_seen_ && best_score < limits::kMinTrackOverlap;
    if (best == nullptr || too_little_overlap) {
        return std::nullopt;
    }
    last_seen_ = *best;
    last_seen_time_sec_ = time_sec;
    return *best;
}

FaceTrack fill_short_gaps(const FaceTrack& track, std::size_t max_gap_frames) {
    std::array<std::vector<std::optional<double>>, kCoordCount> filled;
    for (std::size_t c = 0; c < kCoordCount; ++c) {
        std::vector<std::optional<double>> series(track.size());
        for (std::size_t i = 0; i < track.size(); ++i) {
            if (track[i]) {
                series[i] = to_coords(*track[i])[c];
            }
        }
        filled[c] = fill_gaps(series, max_gap_frames);
    }
    FaceTrack out(track.size());
    for (std::size_t i = 0; i < track.size(); ++i) {
        if (!filled[0][i]) {
            continue;
        }
        Coords coords;
        for (std::size_t c = 0; c < kCoordCount; ++c) {
            coords[c] = *filled[c][i];
        }
        out[i] = from_coords(coords);
    }
    return out;
}

std::optional<Segment> longest_segment(const FaceTrack& track) {
    std::optional<Segment> best;
    std::size_t run_begin = 0;

    for (std::size_t i = 0; i <= track.size(); ++i) {
        const bool has_face = i < track.size() && track[i].has_value();
        if (has_face) {
            continue;
        }
        const std::size_t length = i - run_begin;
        if (length > 0 && (!best || length > best->size())) {
            best = Segment{run_begin, i};
        }
        run_begin = i + 1;
    }
    return best;
}

std::vector<FaceGeometry> faces_in(const FaceTrack& track, const Segment& segment) {
    std::vector<FaceGeometry> faces;
    faces.reserve(segment.size());
    for (std::size_t i = segment.begin; i < segment.end; ++i) {
        faces.push_back(*track[i]);
    }
    return faces;
}

std::vector<FaceGeometry> smooth_track(const std::vector<FaceGeometry>& faces,
                                       std::size_t window_frames) {
    std::array<std::vector<double>, kCoordCount> smoothed;
    for (std::size_t c = 0; c < kCoordCount; ++c) {
        std::vector<double> series;
        series.reserve(faces.size());
        for (const FaceGeometry& face : faces) {
            series.push_back(to_coords(face)[c]);
        }
        smoothed[c] = moving_average_centered(series, window_frames);
    }
    std::vector<FaceGeometry> out;
    out.reserve(faces.size());
    for (std::size_t i = 0; i < faces.size(); ++i) {
        Coords coords;
        for (std::size_t c = 0; c < kCoordCount; ++c) {
            coords[c] = smoothed[c][i];
        }
        out.push_back(from_coords(coords));
    }
    return out;
}

double landmark_jitter_px(const std::vector<FaceGeometry>& faces) {
    if (faces.size() < 3) {
        return 0.0;
    }
    std::vector<double> wobble;
    wobble.reserve((faces.size() - 2) * kLandmarkCount);
    for (std::size_t i = 1; i + 1 < faces.size(); ++i) {
        for (std::size_t l = 0; l < kLandmarkCount; ++l) {
            const Point& prev = faces[i - 1].landmarks[l];
            const Point& cur = faces[i].landmarks[l];
            const Point& next = faces[i + 1].landmarks[l];
            wobble.push_back(
                std::hypot(prev.x - 2.0 * cur.x + next.x, prev.y - 2.0 * cur.y + next.y));
        }
    }
    return median(wobble);
}

}  // namespace rppg
