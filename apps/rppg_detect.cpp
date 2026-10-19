// rppg_detect: reads a video, finds and tracks the face, checks the video is
// usable for pulse measurement, and prints a report. Stages 1-2 of the pipeline.
//
// Usage: rppg_detect <video> [--model <yunet.onnx>] [--debug-dir <dir>]
//                            [--landmarks-csv <file>]
//
// Exit codes:
//   0  OK
//   1  bad arguments, or the video/model couldn't be opened
//   2  video too short
//   3  no face found, or the face is too small
//   4  face never tracked long enough without a break

#include "rppg/annotate.hpp"
#include "rppg/compression.hpp"
#include "rppg/face.hpp"
#include "rppg/face_track.hpp"
#include "rppg/limits.hpp"
#include "rppg/requirements.hpp"
#include "rppg/signal.hpp"
#include "rppg/timing.hpp"
#include "rppg/track_csv.hpp"
#include "rppg/video.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitTooShort = 2;
constexpr int kExitNoUsableFace = 3;
constexpr int kExitTrackTooShort = 4;

constexpr int kLabelWidth = 14;

const cv::Scalar kRawColor(0, 0, 255);       // red
const cv::Scalar kSmoothedColor(0, 255, 0);  // green

struct Options {
    std::string video_path;
    std::string model_path = "models/face_detection_yunet_2023mar.onnx";
    std::string debug_dir;
    std::string landmarks_csv;
};

struct DetectionPass {
    std::vector<double> timestamps_sec;
    rppg::FaceTrack raw_track;
    std::vector<double> detect_ms;
};

struct CleanTrack {
    rppg::FaceTrack filled;
    std::optional<rppg::Segment> segment;
    std::vector<rppg::FaceGeometry> raw_faces;
    std::vector<rppg::FaceGeometry> smoothed;
};

void print_usage() {
    std::cerr << "usage: rppg_detect <video> [--model <yunet.onnx>] [--debug-dir <dir>]\n"
                 "                           [--landmarks-csv <file>]\n";
}

bool require_file(const std::string& path, const std::string& description) {
    if (std::filesystem::is_regular_file(path)) {
        return true;
    }
    std::cerr << "error: " << description << " not found: " << path << "\n";
    return false;
}

std::optional<Options> parse_args(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--model" && has_value) {
            options.model_path = argv[++i];
        } else if (arg == "--debug-dir" && has_value) {
            options.debug_dir = argv[++i];
        } else if (arg == "--landmarks-csv" && has_value) {
            options.landmarks_csv = argv[++i];
        } else if (options.video_path.empty() && arg.rfind("--", 0) != 0) {
            options.video_path = arg;
        } else {
            return std::nullopt;
        }
    }
    if (options.video_path.empty()) {
        return std::nullopt;
    }
    return options;
}

std::size_t frames_for(double seconds, double fps) {
    return static_cast<std::size_t>(std::lround(seconds * fps));
}

std::size_t odd_frames_for(double seconds, double fps) {
    const std::size_t frames = std::max<std::size_t>(1, frames_for(seconds, fps));
    return frames % 2 == 1 ? frames : frames + 1;
}

DetectionPass run_detection_pass(rppg::VideoReader& reader, rppg::FaceDetector& detector) {
    DetectionPass pass;
    rppg::FaceFollower follower;
    rppg::Frame frame;

    while (reader.read(frame)) {
        const auto start = std::chrono::steady_clock::now();
        const std::vector<rppg::FaceGeometry> detections = detector.detect(frame.image);
        const auto elapsed = std::chrono::steady_clock::now() - start;

        pass.timestamps_sec.push_back(frame.time_sec);
        pass.raw_track.push_back(follower.next(detections, frame.time_sec));
        pass.detect_ms.push_back(std::chrono::duration<double, std::milli>(elapsed).count());
    }
    return pass;
}

CleanTrack clean_track(const rppg::FaceTrack& raw_track, double fps) {
    CleanTrack clean;
    clean.filled = rppg::fill_short_gaps(raw_track, frames_for(rppg::limits::kMaxGapSec, fps));
    clean.segment = rppg::longest_segment(clean.filled);
    if (clean.segment) {
        clean.raw_faces = rppg::faces_in(clean.filled, *clean.segment);
        clean.smoothed = rppg::smooth_track(clean.raw_faces,
                                            odd_frames_for(rppg::limits::kSmoothingWindowSec, fps));
    }
    return clean;
}

double duration_of(const rppg::Segment& segment, const std::vector<double>& timestamps_sec,
                   const rppg::TimingStats& timing) {
    return timestamps_sec[segment.end - 1] - timestamps_sec[segment.begin] +
           timing.median_interval_sec;
}

std::vector<double> face_sizes_of(const std::vector<rppg::FaceGeometry>& faces) {
    std::vector<double> sizes;
    sizes.reserve(faces.size());
    for (const rppg::FaceGeometry& face : faces) {
        sizes.push_back(rppg::face_size_px(face.box));
    }
    return sizes;
}

std::size_t count_present(const rppg::FaceTrack& track) {
    return static_cast<std::size_t>(std::count_if(
        track.begin(), track.end(), [](const auto& face) { return face.has_value(); }));
}

std::size_t write_debug_frames(const Options& options, const CleanTrack& clean, double fps) {
    std::filesystem::create_directories(options.debug_dir);
    const std::size_t step =
        std::max<std::size_t>(1, frames_for(rppg::limits::kDebugFrameIntervalSec, fps));
    const rppg::Segment& segment = *clean.segment;

    rppg::VideoReader reader(options.video_path);
    rppg::Frame frame;
    std::size_t saved = 0;
    for (std::size_t i = 0; i < segment.end && reader.read(frame); ++i) {
        if (i < segment.begin || (i - segment.begin) % step != 0) {
            continue;
        }
        rppg::draw_face(frame.image, clean.raw_faces[i - segment.begin], kRawColor);
        rppg::draw_face(frame.image, clean.smoothed[i - segment.begin], kSmoothedColor);

        std::ostringstream name;
        name << "frame_" << std::setw(6) << std::setfill('0') << i << ".png";
        cv::imwrite((std::filesystem::path(options.debug_dir) / name.str()).string(), frame.image);
        ++saved;
    }
    return saved;
}

std::ostream& field(const std::string& label) {
    return std::cout << std::left << std::setw(kLabelWidth) << label;
}

void print_video_report(const rppg::VideoInfo& info, const rppg::TimingStats& timing, double bpp) {
    std::cout << std::fixed << std::setprecision(2);
    field("Video:") << info.path << "\n";
    field("Codec:") << info.codec << ", " << info.width << "x" << info.height << "\n";
    field("Frames:") << timing.frame_count << " read, " << timing.duration_sec << " s\n";
    field("Frame rate:") << info.nominal_fps << " fps nominal, " << timing.measured_fps
                         << " fps measured ("
                         << (timing.is_variable_frame_rate ? "variable" : "constant") << ")\n";
    field("Compression:") << std::setprecision(3) << bpp << " bits/pixel\n" << std::setprecision(2);
}

void print_face_report(const DetectionPass& pass, const CleanTrack& clean,
                       const rppg::TimingStats& timing) {
    const std::size_t detected = count_present(pass.raw_track);
    const std::size_t filled = count_present(clean.filled) - detected;
    field("Face:") << "detected in " << detected << " of " << pass.raw_track.size() << " frames, "
                   << filled << " filled in\n";

    if (clean.segment) {
        const rppg::Segment& segment = *clean.segment;
        field("Tracked:") << duration_of(segment, pass.timestamps_sec, timing)
                          << " s in a row (frames " << segment.begin << "-" << segment.end - 1
                          << "), median size " << std::setprecision(0)
                          << rppg::median(face_sizes_of(clean.smoothed)) << " px\n"
                          << std::setprecision(2);
        field("Jitter:") << rppg::landmark_jitter_px(clean.raw_faces) << " px raw, "
                         << rppg::landmark_jitter_px(clean.smoothed) << " px smoothed\n";
    }
    if (!pass.detect_ms.empty()) {
        field("Detection:") << rppg::median(pass.detect_ms) << " ms/frame (median)\n";
    }
}

void print_warnings(const rppg::TimingStats& timing, double bpp) {
    if (rppg::is_heavily_compressed(bpp)) {
        field("WARNING:") << "heavy compression (below " << rppg::limits::kHeavyCompressionBpp
                          << " bits/pixel); the pulse signal may be degraded\n";
    }
    if (timing.is_variable_frame_rate) {
        field("NOTE:") << "variable frame rate (interval variation "
                       << timing.interval_variation * 100.0
                       << "%); signals will be resampled to even spacing\n";
    }
    if (timing.non_increasing_count > 0) {
        field("WARNING:") << timing.non_increasing_count
                          << " timestamps went backward or repeated; the file may be damaged\n";
    }
}

void print_result(const rppg::RequirementResult& result) {
    if (result.status == rppg::RequirementStatus::kOk) {
        field("Result:") << "OK\n";
    } else {
        field("Result:") << "REJECTED: " << result.reason << "\n";
    }
}

int exit_code_for(rppg::RequirementStatus status) {
    switch (status) {
        case rppg::RequirementStatus::kOk:
            return kExitOk;
        case rppg::RequirementStatus::kTooShort:
            return kExitTooShort;
        case rppg::RequirementStatus::kNoFace:
        case rppg::RequirementStatus::kFaceTooSmall:
            return kExitNoUsableFace;
        case rppg::RequirementStatus::kTrackTooShort:
            return kExitTrackTooShort;
    }
    return kExitBadInput;
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = parse_args(argc, argv);
    if (!options) {
        print_usage();
        return kExitBadInput;
    }
    if (!require_file(options->video_path, "video") ||
        !require_file(options->model_path, "face detector model")) {
        return kExitBadInput;
    }

    try {
        rppg::VideoReader reader(options->video_path);
        rppg::FaceDetector detector(options->model_path);

        const DetectionPass pass = run_detection_pass(reader, detector);
        const rppg::TimingStats timing = rppg::analyze_timing(pass.timestamps_sec);
        const CleanTrack clean = clean_track(pass.raw_track, timing.measured_fps);
        const rppg::VideoInfo& info = reader.info();
        const double bpp =
            rppg::bits_per_pixel(info.file_bytes, info.width, info.height, timing.frame_count);

        const double tracked_sec =
            clean.segment ? duration_of(*clean.segment, pass.timestamps_sec, timing) : 0.0;
        const rppg::RequirementResult result =
            rppg::check_requirements(timing, tracked_sec, face_sizes_of(clean.smoothed));

        print_video_report(info, timing, bpp);
        print_face_report(pass, clean, timing);
        print_warnings(timing, bpp);

        if (clean.segment && !options->landmarks_csv.empty()) {
            rppg::write_track_csv(options->landmarks_csv, pass.timestamps_sec, pass.raw_track,
                                  clean.filled, *clean.segment, clean.smoothed);
            field("Landmarks:") << "written to " << options->landmarks_csv << "\n";
        }
        if (clean.segment && !options->debug_dir.empty()) {
            const std::size_t saved = write_debug_frames(*options, clean, timing.measured_fps);
            field("Debug:") << saved << " frames saved to " << options->debug_dir << "\n";
        }

        print_result(result);
        return exit_code_for(result.status);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitBadInput;
    }
}
