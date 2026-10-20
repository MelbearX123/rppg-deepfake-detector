# AGENTS.md

Context for AI coding agents working in this repo. Read this before making changes.

## The project

A C++17 command-line tool that estimates a person's pulse from a face video (remote
photoplethysmography, rPPG) and, eventually, uses pulse consistency across face regions to
flag deepfakes. It's a student portfolio project for a C++ co-op application, so the code is
judged on readability, testing, and engineering practice, not just results.

Offline only: video file in, report out. No live webcam, no GUI (OpenCV's highgui is not built).

### Stages

| Stage | Status | What |
|---|---|---|
| 1 Video input | done | per-frame timestamps, VFR detection, compression (bits/pixel), length check |
| 2 Face tracking | done | YuNet on every frame (shrunk to 640 px), IoU follower, gap filling, longest stretch, centered moving average |
| 3 Face regions | done | glabella, forehead, upper nasal dorsum, both malars (Li et al. 2024); per-person skin model; trimmed mean |
| 4 Pulse signal | done | POS (Wang et al. 2017) per region, on colors resampled to an even 30 Hz grid |
| 5 Signal processing | done | zero-phase Butterworth bandpass 0.7-4 Hz (no detrend: POS already removes drift); the first and last ~2 s are less accurate (edge effects) |
| 6 Heart rate | done | spectrum peak per 10 s window (every 0.5 bpm), median filter, SNR (de Haan 2013); regions combined per window (median, nasal dorsum left out); a window is trusted when regions agree; median of trusted windows, else "inconclusive" |
| 7 Deepfake classification | next (stretch) | compare pulse between regions, FaceForensics++ |
| 8 Evaluation | in progress | `rppg_eval` replays saved colors vs waveform truth; 9 UBFC subjects: clip MAE 0.88 bpm, 9/9 decided; trust thresholds tuned on odd subjects, checked on even. Still to do: all 42 subjects, per-skin-tone error |
| 9-10 Performance, packaging | planned | benchmarks, Docker |

The resume-ready milestone is Stage 6 plus heart-rate validation on UBFC (Stage 8).

## Layout

- `include/rppg/`, `src/`: grouped by pipeline step, same folders in both:
  - `video/`: `video`, `timing`, `compression`
  - `face/`: `face`, `face_geometry`, `face_track`
  - `regions/`: `regions`, `region_sampler`, `region_signal`, `skin_model`, `pixel_stats`
  - `pulse/`: `signal`, `filter`, `pulse`
  - `heart_rate/`: `spectrum`, `heart_rate`
  - `output/`: `track_csv`, `signals_csv` (also reads saved colors back), `annotate`
  - `eval/`: `ground_truth` (UBFC files, truth from the waveform), `metrics` (MAE/RMSE/r)
  - top level: `limits.hpp`, `requirements` (used by every step)

  Includes use the folder: `#include "rppg/face/face_track.hpp"`. The folders don't decide the
  library; `CMakeLists.txt` splits the files into two:
  - `rppg_core`: no OpenCV. Signal math, timing, face tracking, regions, skin model, CSV writers,
    requirement checks. Everything here is unit-testable without video.
  - `rppg_vision`: anything touching images/video (`video`, `face`, `region_sampler`, `annotate`).
    Keep it thin: extract data with OpenCV, make decisions in core.
- `apps/rppg_detect.cpp`: the CLI. Runs up to three passes over the video: detect/track, fit
  skin model, measure regions; then extracts and bandpasses a pulse per region and estimates the heart rate. Returns early (no further passes) once a check rejects the video.
- `apps/rppg_eval.cpp`: evaluation on UBFC-rPPG without videos. Reads each subject's saved
  colors (`rppg_detect --signals-csv`) and `ground_truth.txt`, reruns the pulse and heart-rate
  stages, and prints MAE/RMSE/r per clip and per window. Links only `rppg_core`.
- `include/rppg/limits.hpp`: every tunable threshold, named. Don't scatter numbers elsewhere.
- `tests/`: GoogleTest, one file per module.
- `scripts/`: `make_test_clips.sh` (synthetic test videos), `plot_landmarks.py`, `plot_signals.py`,
  `plot_pulse.py`, `plot_heart_rate.py`,
  `tune_thresholds.py` (tunes the window-trust limits from `rppg_eval --windows-csv`).
- `models/`: YuNet ONNX model. `data/`, `debug/`, `results/`: local only, gitignored.

Exit codes: 0 OK, 1 bad input, 2 too short, 3 no face / face too small, 4 face not tracked
10 s in a row, 5 no usable skin region, 6 inconclusive (the face regions agreed in too few windows).

## Conventions

- C++17, `-Wall -Wextra -Wpedantic`; CI also uses warnings-as-errors.
- Format with clang-format (`.clang-format`: Google style, 4-space indent, 100 columns).
- Per-frame data is always indexed by frame number (`FaceTrack`, `RegionSeries`, timestamps).
  Don't introduce segment-relative indexing. The exception is pulse signals (`RegionPulses`): they
  are resampled, so they're indexed by time on an even grid that carries its own start and rate.
- Plain structs and free functions; `std::optional` for "maybe missing"; throw
  `std::invalid_argument` / `std::runtime_error` with the function name in the message.
- Keep functions small and named for what they do; `main` should read as a summary.
- Every tunable value gets a name. Thresholds go in `limits.hpp`.
- Decisions should be backed by measurement. Several thresholds came from measuring real pixels
  (e.g. the skin brightness floor); comments say so. Don't tune by guessing.

## Build, test, run

Run from the repo root. The environment is WSL2 (Ubuntu 24.04); build tools come from Homebrew
(no sudo). clang-format is keg-only: `/home/linuxbrew/.linuxbrew/opt/clang-format/bin/clang-format`
if it isn't on PATH.

```bash
cmake --build --preset conan-release
ctest --test-dir build/Release --output-on-failure
./build/Release/rppg_detect data/webcam.mp4 --debug-dir debug --landmarks-csv results/track.csv --signals-csv results/signals.csv
```

UBFC evaluation (`data/ubfc/subjectN/` with `ground_truth.txt` and `signals.csv` saved by
`rppg_detect <vid.avi> --signals-csv`; the video can then be deleted). Ground truth comes from the
waveform (line 1); the oximeter's displayed rate (line 2) was off by up to 26 bpm:

```bash
./build/Release/rppg_eval data/ubfc --windows-csv results/ubfc/windows.csv
```

Same checks as CI (warnings as errors), then reset:

```bash
cmake --preset conan-release -DCMAKE_COMPILE_WARNING_AS_ERROR=ON && cmake --build --preset conan-release
cmake --preset conan-release -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
```

Format check: `clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.hpp')`

Python scripts use the project venv: `.venv/bin/python scripts/plot_signals.py results/signals.csv`

Test videos: `scripts/make_test_clips.sh` writes synthetic clips to `data/` (no faces). The real
test case is `data/webcam.mp4` (owner's own face: 1080p, ~20 fps variable frame rate, bangs over
part of the forehead). When checking exit codes in a loop, capture `code=$?` immediately after the
command; `$(...)` or pipes in between overwrite `$?`.

`main` is protected: merging needs the `build-and-test` CI check to pass.

## How the owner likes to work

- **Plan before code.** For new work, first give a file-by-file plan and the reasoning for each
  decision, then wait for approval.
- **Write the `.hpp` files first**, then implementations.
- **Short comments that explain the code.** The owner reads them to understand it, then deletes
  most before committing. Don't re-add comments they removed. If asked for "no comments", add none.
- **Readability and no bloat** over cleverness. No unused fields, functions, or includes.
- **Senior-engineer review passes:** when asked to review, list every issue (bugs first, then
  readability, then small items, then optional), with file:line references, before changing anything.
- **Verify, don't assume:** build with warnings-as-errors, run all tests, run the binary on the test
  clips and `data/webcam.mp4`, and look at debug frames/plots when the change is visual.
- **Explanations** in plain language, line by line when asked, with concrete numbers and examples.
- **"Ready to push? yes or no":** answer yes/no first, then the evidence (clean status, format,
  warnings-as-errors build, tests, exit codes, no private files tracked).
- **PR descriptions:** short, simple, written in a student's voice. Sections: what it does,
  structure, testing.

## Rules

- Do not commit or push unless explicitly asked.
- Never commit videos, dataset files, debug frames, or results. Dataset licenses (UBFC,
  FaceForensics++) forbid redistribution.
- Don't edit shell config (`~/.bashrc`, `~/.profile`) without asking.

## References

- S. Bobbia et al., "Unsupervised skin tissue segmentation for remote photoplethysmography",
  Pattern Recognition Letters, 2017 (UBFC-rPPG dataset).
- S. Li, M. Elgendi, C. Menon, "Optimal facial regions for remote heart rate measurement during
  physical and cognitive activities", npj Cardiovascular Health 1:33, 2024 (region choice).
- W. Wang et al., "Algorithmic Principles of Remote PPG", IEEE TBME 64(7), 2017 (POS).
