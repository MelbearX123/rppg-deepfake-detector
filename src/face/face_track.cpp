#include "rppg/face/face_track.hpp"

#include "rppg/limits.hpp"
#include "rppg/pulse/signal.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace rppg {

namespace {

constexpr std::size_t kCoordCount = 4 + 2 * kLandmarkCount;
using Coords = std::array<double, kCoordCount>;
using Columns = std::array<std::vector<std::optional<double>>, kCoordCount>;

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

Columns to_columns(const FaceTrack& track) {
    Columns columns;
    for (auto& column : columns) {
        column.resize(track.size());
    }
    for (std::size_t i = 0; i < track.size(); ++i) {
        if (!track[i]) {
            continue;
        }
        const Coords coords = to_coords(*track[i]);
        for (std::size_t c = 0; c < kCoordCount; ++c) {
            columns[c][i] = coords[c];
        }
    }
    return columns;
}

FaceTrack from_columns(const Columns& columns) {
    FaceTrack track(columns[0].size());
    for (std::size_t i = 0; i < track.size(); ++i) {
        if (!columns[0][i]) {
            continue;
        }
        Coords coords;
        for (std::size_t c = 0; c < kCoordCount; ++c) {
            coords[c] = *columns[c][i];
        }
        track[i] = from_coords(coords);
    }
    return track;
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
    Columns columns = to_columns(track);
    for (auto& column : columns) {
        column = fill_gaps(column, max_gap_frames);
    }
    return from_columns(columns);
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

FaceTrack smooth_segment(const FaceTrack& track, const Segment& segment,
                         std::size_t window_frames) {
    Columns columns = to_columns(track);
    for (auto& column : columns) {
        std::vector<double> values;
        values.reserve(segment.size());
        for (std::size_t i = segment.begin; i < segment.end; ++i) {
            values.push_back(*column[i]);
        }
        const std::vector<double> smoothed = moving_average_centered(values, window_frames);

        std::fill(column.begin(), column.end(), std::nullopt);
        for (std::size_t i = segment.begin; i < segment.end; ++i) {
            column[i] = smoothed[i - segment.begin];
        }
    }
    return from_columns(columns);
}

double landmark_jitter_px(const FaceTrack& track) {
    std::vector<double> wobble;
    for (std::size_t i = 1; i + 1 < track.size(); ++i) {
        if (!track[i - 1] || !track[i] || !track[i + 1]) {
            continue;
        }
        for (std::size_t l = 0; l < kLandmarkCount; ++l) {
            const Point& prev = track[i - 1]->landmarks[l];
            const Point& cur = track[i]->landmarks[l];
            const Point& next = track[i + 1]->landmarks[l];
            wobble.push_back(
                std::hypot(prev.x - 2.0 * cur.x + next.x, prev.y - 2.0 * cur.y + next.y));
        }
    }
    return wobble.empty() ? 0.0 : median(wobble);
}

}  // namespace rppg
