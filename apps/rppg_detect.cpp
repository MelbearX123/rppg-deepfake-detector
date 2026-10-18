// rppg_detect: reads a video, checks it's usable for pulse measurement, and
// prints a report. Stage 1 of the pipeline.
//
// Usage: rppg_detect <video> [--model <path to YuNet .onnx>]
//
// Exit codes:
//   0  OK
//   1  bad arguments, or the video/model couldn't be opened
//   2  video too short
//   3  no face found, or the face is too small

#include "rppg/compression.hpp"
#include "rppg/face.hpp"
#include "rppg/limits.hpp"
#include "rppg/requirements.hpp"
#include "rppg/signal.hpp"
#include "rppg/timing.hpp"
#include "rppg/video.hpp"

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitTooShort = 2;
constexpr int kExitNoUsableFace = 3;

constexpr int kLabelWidth = 14;

struct Options {
    std::string video_path;
    std::string model_path = "models/face_detection_yunet_2023mar.onnx";
};

struct ScanResult {
    std::vector<double> timestamps_sec;
    std::vector<double> face_sizes_px;
    int sampled_frames = 0;
};

void print_usage() {
    std::cerr << "usage: rppg_detect <video> [--model <yunet.onnx>]\n";
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
        if (arg == "--model" && i + 1 < argc) {
            options.model_path = argv[++i];
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

ScanResult scan_video(rppg::VideoReader& reader, rppg::FaceDetector& detector) {
    ScanResult result;
    rppg::Frame frame;
    double next_sample_time = 0.0;

    while (reader.read(frame)) {
        result.timestamps_sec.push_back(frame.time_sec);

        if (frame.time_sec >= next_sample_time) {
            ++result.sampled_frames;
            if (const auto face = detector.largest_face(frame.image)) {
                result.face_sizes_px.push_back(rppg::face_size_px(*face));
            }
            next_sample_time = frame.time_sec + rppg::limits::kFaceSampleIntervalSec;
        }
    }
    return result;
}

// Starts a report line with its label padded to a fixed width, so the values
// line up in a column: field("Codec:") << "h264";
std::ostream& field(const std::string& label) {
    return std::cout << std::left << std::setw(kLabelWidth) << label;
}

void print_report(const rppg::VideoInfo& info, const rppg::TimingStats& timing, double bpp,
                  const ScanResult& scan) {
    std::cout << std::fixed << std::setprecision(2);
    field("Video:") << info.path << "\n";
    field("Codec:") << info.codec << ", " << info.width << "x" << info.height << "\n";
    field("Frames:") << timing.frame_count << " read, " << timing.duration_sec << " s\n";
    field("Frame rate:") << info.nominal_fps << " fps nominal, " << timing.measured_fps
                         << " fps measured ("
                         << (timing.is_variable_frame_rate ? "variable" : "constant") << ")\n";
    field("Compression:") << std::setprecision(3) << bpp << " bits/pixel\n" << std::setprecision(2);

    if (scan.face_sizes_px.empty()) {
        field("Face:") << "none found in " << scan.sampled_frames << " sampled frames\n";
    } else {
        field("Face:") << "largest face " << std::setprecision(0)
                       << rppg::median(scan.face_sizes_px) << " px (median), found in "
                       << scan.face_sizes_px.size() << " of " << scan.sampled_frames
                       << " sampled frames\n"
                       << std::setprecision(2);
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
    }
    return kExitBadInput;  // unreachable; every status is handled above
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

        const ScanResult scan = scan_video(reader, detector);
        const rppg::TimingStats timing = rppg::analyze_timing(scan.timestamps_sec);
        const rppg::VideoInfo& info = reader.info();
        const double bpp =
            rppg::bits_per_pixel(info.file_bytes, info.width, info.height, timing.frame_count);
        const rppg::RequirementResult result = rppg::check_requirements(timing, scan.face_sizes_px);

        print_report(info, timing, bpp, scan);
        print_warnings(timing, bpp);
        print_result(result);
        return exit_code_for(result.status);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitBadInput;
    }
}
