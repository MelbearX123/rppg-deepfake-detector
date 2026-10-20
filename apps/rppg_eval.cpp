// rppg_eval: measures heart-rate accuracy on UBFC-rPPG without the videos. For
// each subject folder it reads the region colors that rppg_detect saved
// (--signals-csv), reruns the pulse and heart-rate stages, and compares the
// result with the ground truth computed from the finger sensor's waveform.
//
// Usage: rppg_eval <dataset dir> [--suffix <s>] [--windows-csv <file>]
//
//   <dataset dir>    holds subjectN/ folders with signals<s>.csv and ground_truth.txt
//   --suffix <s>     reads signals<s>.csv instead, e.g. _crf6 for a re-encoded run
//   --windows-csv    writes one row per region per window, for tuning thresholds
//
// Exit codes: 0 OK, 1 bad arguments, or no subject could be evaluated.
// A subject that fails (e.g. a malformed file) is reported and skipped.

#include "rppg/eval/ground_truth.hpp"
#include "rppg/eval/metrics.hpp"
#include "rppg/heart_rate/heart_rate.hpp"
#include "rppg/limits.hpp"
#include "rppg/output/signals_csv.hpp"
#include "rppg/pulse/pulse.hpp"
#include "rppg/pulse/signal.hpp"
#include "rppg/regions/regions.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBadInput = 1;

constexpr int kNameWidth = 13;

struct Options {
    std::string dataset_dir;
    std::string suffix;
    std::string windows_csv;
};

struct WindowRow {
    std::string subject;
    rppg::Region region = rppg::kGlabella;
    double center_sec = 0.0;
    double estimate_bpm = 0.0;
    double truth_bpm = 0.0;
    double snr_db = 0.0;
};

struct FaceRow {
    rppg::FaceWindow face;
    double truth_bpm = 0.0;
};

struct SubjectResult {
    std::string name;
    std::optional<double> tool_bpm;  // nullopt = inconclusive or no pulse
    std::optional<double> truth_bpm;
    std::vector<WindowRow> windows;
    std::vector<FaceRow> faces;
};

void print_usage() {
    std::cerr << "usage: rppg_eval <dataset dir> [--suffix <s>] [--windows-csv <file>]\n";
}

std::optional<Options> parse_args(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--suffix" && has_value) {
            options.suffix = argv[++i];
        } else if (arg == "--windows-csv" && has_value) {
            options.windows_csv = argv[++i];
        } else if (options.dataset_dir.empty() && arg.rfind("--", 0) != 0) {
            options.dataset_dir = arg;
        } else {
            return std::nullopt;
        }
    }
    if (options.dataset_dir.empty()) {
        return std::nullopt;
    }
    return options;
}

std::size_t trailing_number(const std::string& name) {
    const auto digits =
        std::find_if(name.rbegin(), name.rend(), [](unsigned char c) { return !std::isdigit(c); });
    const std::string number(digits.base(), name.end());
    return number.empty() ? 0 : std::stoul(number);
}

std::vector<std::filesystem::path> subject_dirs(const Options& options) {
    std::vector<std::filesystem::path> dirs;
    for (const auto& entry : std::filesystem::directory_iterator(options.dataset_dir)) {
        const std::filesystem::path& dir = entry.path();
        if (entry.is_directory() && std::filesystem::exists(dir / "ground_truth.txt") &&
            std::filesystem::exists(dir / ("signals" + options.suffix + ".csv"))) {
            dirs.push_back(dir);
        }
    }
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) {
        return trailing_number(a.filename().string()) < trailing_number(b.filename().string());
    });
    return dirs;
}

rppg::HeartRates replay(const rppg::SavedSignals& saved) {
    return rppg::estimate_heart_rates(rppg::bandpass_pulses(
        rppg::extract_pulses(saved.signals, saved.timestamps_sec, saved.segment)));
}

std::vector<double> window_truths(const rppg::HeartRates& rates, const rppg::GroundTruth& truth) {
    std::vector<double> truths;
    for (const auto& region : rates.regions) {
        if (region) {
            for (const rppg::WindowEstimate& window : region->windows) {
                truths.push_back(rppg::waveform_bpm(truth, window.center_sec,
                                                    rppg::limits::kHeartRateWindowSec / 2.0));
            }
            break;
        }
    }
    return truths;
}

SubjectResult evaluate_subject(const std::filesystem::path& dir, const Options& options) {
    const rppg::SavedSignals saved =
        rppg::read_signals_csv((dir / ("signals" + options.suffix + ".csv")).string());
    const rppg::GroundTruth truth =
        rppg::read_ubfc_ground_truth((dir / "ground_truth.txt").string());
    const rppg::HeartRates rates = replay(saved);
    const std::vector<double> truths = window_truths(rates, truth);

    SubjectResult result;
    result.name = dir.filename().string();
    result.tool_bpm = rates.bpm;
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        if (!rates.regions[r]) {
            continue;
        }
        const std::vector<rppg::WindowEstimate>& windows = rates.regions[r]->windows;
        for (std::size_t i = 0; i < windows.size(); ++i) {
            result.windows.push_back({result.name, static_cast<rppg::Region>(r),
                                      windows[i].center_sec, windows[i].smoothed_bpm, truths[i],
                                      windows[i].snr_db});
        }
    }
    for (std::size_t i = 0; i < rates.face_windows.size(); ++i) {
        result.faces.push_back({rates.face_windows[i], truths[i]});
    }
    if (!truths.empty()) {
        result.truth_bpm = rppg::median(truths);
    }
    return result;
}

void print_stats(const std::string& label, const rppg::ErrorStats& stats) {
    std::cout << "  " << std::left << std::setw(kNameWidth) << label << std::right << std::setw(5)
              << stats.count << "  MAE " << std::setw(5) << stats.mae << "  RMSE " << std::setw(5)
              << stats.rmse << "  r " << std::setw(5) << stats.pearson_r << "  within 5 bpm "
              << std::setprecision(0) << std::setw(3) << stats.within_5_bpm * 100.0 << "%\n"
              << std::setprecision(2);
}

void print_clip_report(const std::vector<SubjectResult>& results) {
    std::cout << "Clip level (tool's answer vs median of window truths):\n";
    std::vector<double> estimates;
    std::vector<double> truths;
    for (const SubjectResult& s : results) {
        std::cout << "  " << std::left << std::setw(kNameWidth) << s.name << std::right;
        if (!s.truth_bpm) {
            std::cout << "no pulse in any region\n";
            continue;
        }
        std::cout << "truth " << std::setw(6) << *s.truth_bpm << "  tool ";
        if (s.tool_bpm) {
            std::cout << std::setw(6) << *s.tool_bpm << "  error " << std::showpos << std::setw(6)
                      << *s.tool_bpm - *s.truth_bpm << std::noshowpos << "\n";
            estimates.push_back(*s.tool_bpm);
            truths.push_back(*s.truth_bpm);
        } else {
            std::cout << "inconclusive\n";
        }
    }
    std::cout << "  decided " << estimates.size() << " of " << results.size() << " subjects\n";
    if (!estimates.empty()) {
        print_stats("all decided", rppg::error_stats(estimates, truths));
    }
}

void print_window_report(const std::vector<SubjectResult>& results) {
    std::cout << "\nWindow level, each region's smoothed bpm vs waveform truth (all subjects):\n";
    std::array<std::vector<double>, rppg::kRegionCount> estimates;
    std::array<std::vector<double>, rppg::kRegionCount> truths;
    for (const SubjectResult& s : results) {
        for (const WindowRow& w : s.windows) {
            estimates[w.region].push_back(w.estimate_bpm);
            truths[w.region].push_back(w.truth_bpm);
        }
    }
    for (std::size_t r = 0; r < rppg::kRegionCount; ++r) {
        if (!estimates[r].empty()) {
            print_stats(rppg::region_name(static_cast<rppg::Region>(r)),
                        rppg::error_stats(estimates[r], truths[r]));
        }
    }

    std::cout << "\nWindow level, face (regions combined, nasal dorsum left out):\n";
    std::vector<double> all_estimates;
    std::vector<double> all_truths;
    std::vector<double> trusted_estimates;
    std::vector<double> trusted_truths;
    for (const SubjectResult& s : results) {
        for (const FaceRow& row : s.faces) {
            all_estimates.push_back(row.face.bpm);
            all_truths.push_back(row.truth_bpm);
            if (row.face.trusted) {
                trusted_estimates.push_back(row.face.bpm);
                trusted_truths.push_back(row.truth_bpm);
            }
        }
    }
    if (!all_estimates.empty()) {
        print_stats("all windows", rppg::error_stats(all_estimates, all_truths));
    }
    if (!trusted_estimates.empty()) {
        print_stats("trusted only", rppg::error_stats(trusted_estimates, trusted_truths));
    }
    std::cout << "  trusted " << trusted_estimates.size() << " of " << all_estimates.size()
              << " windows\n";
}

void write_windows_csv(const std::string& path, const std::vector<SubjectResult>& results) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("could not write " + path);
    }
    out << "subject,region,center_sec,estimate_bpm,truth_bpm,snr_db,deviation_bpm,regions,"
           "trusted\n";
    out << std::fixed << std::setprecision(3);
    for (const SubjectResult& s : results) {
        for (const WindowRow& w : s.windows) {
            out << w.subject << ',' << rppg::region_name(w.region) << ',' << w.center_sec << ','
                << w.estimate_bpm << ',' << w.truth_bpm << ',' << w.snr_db << ",,,\n";
        }
        for (const FaceRow& row : s.faces) {
            const rppg::FaceWindow& f = row.face;
            out << s.name << ",face," << f.center_sec << ',' << f.bpm << ',' << row.truth_bpm << ','
                << f.snr_db << ',' << f.deviation_bpm << ',' << f.regions << ','
                << (f.trusted ? 1 : 0) << '\n';
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = parse_args(argc, argv);
    if (!options) {
        print_usage();
        return kExitBadInput;
    }
    if (!std::filesystem::is_directory(options->dataset_dir)) {
        std::cerr << "error: dataset folder not found: " << options->dataset_dir << "\n";
        return kExitBadInput;
    }

    try {
        const std::vector<std::filesystem::path> dirs = subject_dirs(*options);
        if (dirs.empty()) {
            std::cerr << "error: no subject folders with signals" << options->suffix
                      << ".csv and ground_truth.txt in " << options->dataset_dir << "\n";
            return kExitBadInput;
        }

        std::vector<SubjectResult> results;
        for (const auto& dir : dirs) {
            try {
                results.push_back(evaluate_subject(dir, *options));
            } catch (const std::exception& e) {
                std::cerr << "skipped " << dir.filename().string() << ": " << e.what() << "\n";
            }
        }
        if (results.empty()) {
            std::cerr << "error: no subject could be evaluated\n";
            return kExitBadInput;
        }

        std::cout << std::fixed << std::setprecision(2);
        print_clip_report(results);
        print_window_report(results);
        if (!options->windows_csv.empty()) {
            write_windows_csv(options->windows_csv, results);
            std::cout << "\nWindows written to " << options->windows_csv << "\n";
        }
        return kExitOk;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitBadInput;
    }
}
