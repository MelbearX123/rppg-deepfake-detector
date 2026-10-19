// rppg_detect: reads a video, finds and tracks the face, measures the color of
// five skin regions in every frame, turns each region's color into a pulse
// signal (POS), filters it to the heart-rate band, and prints a report.
// Stages 1-5 of the pipeline.
//
// Usage: rppg_detect <video> [--model <yunet.onnx>] [--debug-dir <dir>]
//                            [--landmarks-csv <file>] [--signals-csv <file>]
//                            [--pulse-csv <file>]
//
// Exit codes:
//   0  OK
//   1  bad arguments, or the video/model couldn't be opened
//   2  video too short
//   3  no face found, or the face is too small
//   4  face never tracked long enough without a break
//   5  no skin region is usable (e.g. all covered by hair)

#include "rppg/face/face.hpp"
#include "rppg/face/face_track.hpp"
#include "rppg/limits.hpp"
#include "rppg/output/annotate.hpp"
#include "rppg/output/signals_csv.hpp"
#include "rppg/output/track_csv.hpp"
#include "rppg/pulse/pulse.hpp"
#include "rppg/pulse/signal.hpp"
#include "rppg/regions/region_sampler.hpp"
#include "rppg/regions/region_signal.hpp"
#include "rppg/regions/regions.hpp"
#include "rppg/regions/skin_model.hpp"
#include "rppg/requirements.hpp"
#include "rppg/video/compression.hpp"
#include "rppg/video/timing.hpp"
#include "rppg/video/video.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;
constexpr int kExitTooShort = 2;
constexpr int kExitNoUsableFace = 3;
constexpr int kExitTrackTooShort = 4;
constexpr int kExitNoUsableRegion = 5;

constexpr int kLabelWidth = 14;
constexpr int kRegionNameWidth = 14;
constexpr double kSecPerMin = 60.0;

const cv::Scalar kRawColor(0, 0, 255);       // red
const cv::Scalar kSmoothedColor(0, 255, 0);  // green
const cv::Scalar kRegionColor(0, 255, 255);  // yellow
constexpr double kDebugFrameIntervalSec = 1.0;

struct Options {
    std::string video_path;
    std::string model_path = "models/face_detection_yunet_2023mar.onnx";
    std::string debug_dir;
    std::string landmarks_csv;
    std::string signals_csv;
    std::string pulse_csv;
};

struct DetectionPass {
    std::vector<double> timestamps_sec;
    rppg::FaceTrack raw_track;
    std::vector<double> detect_ms;
};

struct CleanTrack {
    rppg::FaceTrack filled;
    std::optional<rppg::Segment> segment;
    rppg::FaceTrack smoothed;
};

struct Measurement {
    rppg::RegionSignals signals;
    std::size_t debug_frames_saved = 0;
};

using RegionCoverage = std::array<double, rppg::kRegionCount>;

void print_usage() {
    std::cerr << "usage: rppg_detect <video> [--model <yunet.onnx>] [--debug-dir <dir>]\n"
                 "                           [--landmarks-csv <file>] [--signals-csv <file>]\n"
                 "                           [--pulse-csv <file>]\n";
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
        } else if (arg == "--signals-csv" && has_value) {
            options.signals_csv = argv[++i];
        } else if (arg == "--pulse-csv" && has_value) {
            options.pulse_csv = argv[++i];
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

void for_each_frame(const std::string& video_path, std::size_t first, std::size_t last,
                    const std::function<void(std::size_t, rppg::Frame&)>& visit) {
    rppg::VideoReader reader(video_path);
    rppg::Frame frame;
    for (std::size_t i = 0; i < last && reader.read(frame); ++i) {
        if (i >= first) {
            visit(i, frame);
        }
    }
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
        clean.smoothed = rppg::smooth_segment(
            clean.filled, *clean.segment, odd_frames_for(rppg::limits::kSmoothingWindowSec, fps));
    }
    return clean;
}

std::optional<rppg::SkinModel> fit_skin_model_pass(const std::string& video_path,
                                                   const CleanTrack& clean, double fps) {
    const rppg::Segment& segment = *clean.segment;
    const std::size_t step =
        std::max<std::size_t>(1, frames_for(rppg::limits::kSkinModelSampleIntervalSec, fps));

    std::vector<rppg::YCrCb> samples;
    for_each_frame(video_path, segment.begin, segment.end, [&](std::size_t i, rppg::Frame& frame) {
        if ((i - segment.begin) % step != 0) {
            return;
        }
        for (const rppg::Quad& quad : rppg::face_regions(*clean.smoothed[i])) {
            rppg::append_evenly_spaced(samples, rppg::pixels_in(frame.image, quad).ycrcb,
                                       rppg::limits::kSkinSamplesPerRegion);
        }
    });
    if (samples.empty()) {
        return std::nullopt;
    }
    return rppg::fit_skin_model(samples);
}

void measure_frame(const cv::Mat& image, const std::array<rppg::Quad, rppg::kRegionCount>& quads,
                   const rppg::SkinModel& skin_model, std::size_t frame_index,
                   rppg::RegionSignals& signals) {
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        signals[r][frame_index] =
            rppg::measure_region(rppg::pixels_in(image, quads[r]), skin_model);
    }
}

void save_annotated_frame(cv::Mat& image, const CleanTrack& clean,
                          const std::array<rppg::Quad, rppg::kRegionCount>& quads,
                          const std::string& debug_dir, std::size_t frame_index) {
    rppg::draw_face(image, *clean.filled[frame_index], kRawColor);
    rppg::draw_face(image, *clean.smoothed[frame_index], kSmoothedColor);
    for (const rppg::Quad& quad : quads) {
        rppg::draw_quad(image, quad, kRegionColor);
    }

    std::ostringstream name;
    name << "frame_" << std::setw(6) << std::setfill('0') << frame_index << ".png";
    const std::string path = (std::filesystem::path(debug_dir) / name.str()).string();
    if (!cv::imwrite(path, image)) {
        throw std::runtime_error("could not write " + path);
    }
}

Measurement measure_regions_pass(const Options& options, const CleanTrack& clean,
                                 const rppg::SkinModel& skin_model, std::size_t frame_count,
                                 double fps) {
    const rppg::Segment& segment = *clean.segment;
    Measurement measurement;
    for (rppg::RegionSeries& series : measurement.signals) {
        series.resize(frame_count);
    }

    const bool save_debug = !options.debug_dir.empty();
    if (save_debug) {
        std::filesystem::create_directories(options.debug_dir);
    }
    const std::size_t debug_step =
        std::max<std::size_t>(1, frames_for(kDebugFrameIntervalSec, fps));

    for_each_frame(options.video_path, segment.begin, segment.end,
                   [&](std::size_t i, rppg::Frame& frame) {
                       const auto quads = rppg::face_regions(*clean.smoothed[i]);
                       measure_frame(frame.image, quads, skin_model, i, measurement.signals);
                       if (save_debug && (i - segment.begin) % debug_step == 0) {
                           save_annotated_frame(frame.image, clean, quads, options.debug_dir, i);
                           ++measurement.debug_frames_saved;
                       }
                   });
    return measurement;
}

RegionCoverage coverage_of(const rppg::RegionSignals& signals, const rppg::Segment& segment) {
    RegionCoverage coverage{};
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        coverage[r] = rppg::median_coverage(signals[r], segment);
    }
    return coverage;
}

std::size_t count_pulses(const rppg::RegionPulses& pulses) {
    return static_cast<std::size_t>(
        std::count_if(pulses.pulses.begin(), pulses.pulses.end(),
                      [](const auto& pulse) { return pulse.has_value(); }));
}

double duration_of(const rppg::Segment& segment, const std::vector<double>& timestamps_sec,
                   const rppg::TimingStats& timing) {
    return timestamps_sec[segment.end - 1] - timestamps_sec[segment.begin] +
           timing.median_interval_sec;
}

std::vector<double> face_sizes_of(const rppg::FaceTrack& track) {
    std::vector<double> sizes;
    for (const auto& face : track) {
        if (face) {
            sizes.push_back(rppg::face_size_px(face->box));
        }
    }
    return sizes;
}

std::size_t count_present(const rppg::FaceTrack& track) {
    return static_cast<std::size_t>(std::count_if(
        track.begin(), track.end(), [](const auto& face) { return face.has_value(); }));
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

void print_face_report(const DetectionPass& pass, const CleanTrack& clean, double tracked_sec,
                       const std::vector<double>& face_sizes) {
    const std::size_t detected = count_present(pass.raw_track);
    const std::size_t filled = count_present(clean.filled) - detected;
    field("Face:") << "detected in " << detected << " of " << pass.raw_track.size() << " frames, "
                   << filled << " filled in\n";

    if (clean.segment) {
        const rppg::Segment& segment = *clean.segment;
        field("Tracked:") << tracked_sec << " s in a row (frames " << segment.begin << "-"
                          << segment.end - 1 << "), median size " << std::setprecision(0)
                          << rppg::median(face_sizes) << " px\n"
                          << std::setprecision(2);
        field("Jitter:") << rppg::landmark_jitter_px(pass.raw_track) << " px raw, "
                         << rppg::landmark_jitter_px(clean.smoothed) << " px smoothed\n";
    }
    if (!pass.detect_ms.empty()) {
        field("Detection:") << rppg::median(pass.detect_ms) << " ms/frame (median)\n";
    }
}

void print_region_report(const rppg::SkinModel& model, const RegionCoverage& coverage) {
    field("Skin model:") << std::setprecision(1) << "Cr " << model.cr_center << " +/- "
                         << model.cr_tolerance << ", Cb " << model.cb_center << " +/- "
                         << model.cb_tolerance << ", Y >= " << model.min_brightness << "\n";

    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        field(r == 0 ? "Regions:" : "")
            << std::left << std::setw(kRegionNameWidth)
            << rppg::region_name(static_cast<rppg::Region>(r)) << std::right << std::setprecision(0)
            << std::setw(3) << coverage[r] * 100.0 << "% skin"
            << (rppg::is_usable(coverage[r]) ? "" : "  (unusable)") << "\n";
    }
    std::cout << std::setprecision(2);
}

void print_pulse_report(const RegionCoverage& coverage, const rppg::RegionPulses& pulses) {
    field("Pulse:") << count_pulses(pulses) << " of " << rppg::kRegionCount << " regions (POS, "
                    << std::setprecision(0) << pulses.rate_hz << " Hz, bandpass "
                    << rppg::limits::kMinPulseHz * kSecPerMin << "-"
                    << rppg::limits::kMaxPulseHz * kSecPerMin << " bpm)\n"
                    << std::setprecision(2);
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        if (rppg::is_usable(coverage[r]) && !pulses.pulses[r]) {
            field("") << rppg::region_name(static_cast<rppg::Region>(r))
                      << ": no pulse, gap in skin color too long\n";
        }
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
        case rppg::RequirementStatus::kNoUsableRegion:
            return kExitNoUsableRegion;
    }
    return kExitBadInput;
}

int report_result(const rppg::RequirementResult& result) {
    if (result.status == rppg::RequirementStatus::kOk) {
        field("Result:") << "OK\n";
    } else {
        field("Result:") << "REJECTED: " << result.reason << "\n";
    }
    return exit_code_for(result.status);
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

        // Pass 1: find and track the face.
        const DetectionPass pass = run_detection_pass(reader, detector);
        const rppg::TimingStats timing = rppg::analyze_timing(pass.timestamps_sec);
        const CleanTrack clean = clean_track(pass.raw_track, timing.measured_fps);
        const rppg::VideoInfo& info = reader.info();
        const double bpp =
            rppg::bits_per_pixel(info.file_bytes, info.width, info.height, timing.frame_count);
        const double tracked_sec =
            clean.segment ? duration_of(*clean.segment, pass.timestamps_sec, timing) : 0.0;
        const std::vector<double> face_sizes = face_sizes_of(clean.smoothed);

        print_video_report(info, timing, bpp);
        print_warnings(timing, bpp);
        print_face_report(pass, clean, tracked_sec, face_sizes);
        if (clean.segment && !options->landmarks_csv.empty()) {
            rppg::write_track_csv(options->landmarks_csv, pass.timestamps_sec, pass.raw_track,
                                  clean.filled, clean.smoothed, *clean.segment);
            field("Landmarks:") << "written to " << options->landmarks_csv << "\n";
        }

        const rppg::RequirementResult face_result =
            rppg::check_face_requirements(timing, tracked_sec, face_sizes);
        if (face_result.status != rppg::RequirementStatus::kOk) {
            return report_result(face_result);
        }

        // Pass 2: learn the person's skin color.
        const auto skin_model =
            fit_skin_model_pass(options->video_path, clean, timing.measured_fps);
        if (!skin_model) {
            return report_result(
                {rppg::RequirementStatus::kNoUsableRegion, "no region had any pixels in frame"});
        }

        // Pass 3: measure the regions.
        const Measurement measurement = measure_regions_pass(
            *options, clean, *skin_model, pass.timestamps_sec.size(), timing.measured_fps);
        const RegionCoverage coverage = coverage_of(measurement.signals, *clean.segment);
        print_region_report(*skin_model, coverage);

        if (!options->signals_csv.empty()) {
            rppg::write_signals_csv(options->signals_csv, pass.timestamps_sec, measurement.signals,
                                    *clean.segment);
            field("Signals:") << "written to " << options->signals_csv << "\n";
        }
        if (!options->debug_dir.empty()) {
            field("Debug:") << measurement.debug_frames_saved << " frames saved to "
                            << options->debug_dir << "\n";
        }

        const rppg::RegionPulses pulses = rppg::bandpass_pulses(
            rppg::extract_pulses(measurement.signals, pass.timestamps_sec, *clean.segment));
        print_pulse_report(coverage, pulses);
        if (!options->pulse_csv.empty()) {
            rppg::write_pulse_csv(options->pulse_csv, pulses);
            field("Pulse CSV:") << "written to " << options->pulse_csv << "\n";
        }

        return report_result(rppg::check_region_requirements(count_pulses(pulses)));
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitBadInput;
    }
}
