"""Tune the window-trust thresholds on half the UBFC subjects and check them on the other half.

Reads the face rows that `rppg_eval --windows-csv` writes (one CSV per encoding, e.g. the
original and the CRF re-encodes) and sweeps:
  - max region deviation (kMaxRegionDeviationBpm)
  - min window SNR (kMinWindowSnrDb)
  - min fraction of trusted windows for a clip to be decided (kMinTrustedWindowFraction)

Window rule: among settings where at most 5% of trusted windows are wrong (> 5 bpm off),
keep the most correct windows. Clip rule: the smallest fraction at which no decided clip in
the tuning half is wrong. Tuned on odd-numbered subjects, reported on even-numbered ones.

Usage:
    .venv/bin/python scripts/tune_thresholds.py results/ubfc/windows*.csv
"""

import argparse
import csv
import itertools
import re
from collections import defaultdict

import numpy as np

GOOD_BPM = 5.0
MIN_REGIONS = 2  # must match limits::kMinRegionsPerWindow
MAX_WRONG_SHARE = 0.05
DEVIATIONS = [0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0]
SNRS = [-8.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0, 0.0]
FRACTIONS = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]


def load_clips(paths):
    """Returns a list of clips; each is (subject number, array of face windows)."""
    clips = []
    for path in paths:
        rows = defaultdict(list)
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                if r["region"] == "face":
                    rows[r["subject"]].append((float(r["estimate_bpm"]), float(r["truth_bpm"]),
                                               float(r["snr_db"]), float(r["deviation_bpm"]),
                                               int(r["regions"])))
        for subject, windows in rows.items():
            clips.append((int(re.search(r"\d+$", subject).group()), path, np.array(windows)))
    return clips


def trusted_mask(w, max_dev, min_snr):
    return (w[:, 4] >= MIN_REGIONS) & (w[:, 3] <= max_dev) & (w[:, 2] >= min_snr)


def window_scores(clips, max_dev, min_snr):
    """(correct windows kept, share of kept windows that are wrong)."""
    kept_good = kept = 0
    for _, _, w in clips:
        m = trusted_mask(w, max_dev, min_snr)
        good = np.abs(w[:, 0] - w[:, 1]) <= GOOD_BPM
        kept += m.sum()
        kept_good += (m & good).sum()
    wrong = (kept - kept_good) / kept if kept else 1.0
    return kept_good, wrong


def clip_results(clips, max_dev, min_snr, min_fraction):
    """(decided, wrong among decided, errors of decided clips)."""
    errors = []
    for _, _, w in clips:
        m = trusted_mask(w, max_dev, min_snr)
        if m.any() and m.sum() >= min_fraction * len(w):
            errors.append(np.median(w[m, 0]) - np.median(w[:, 1]))
    errors = np.array(errors)
    return len(errors), int(np.sum(np.abs(errors) > GOOD_BPM)), errors


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", nargs="+", help="files written by rppg_eval --windows-csv")
    args = parser.parse_args()

    clips = load_clips(args.csv)
    tune = [c for c in clips if c[0] % 2 == 1]
    check = [c for c in clips if c[0] % 2 == 0]
    print(f"{len(clips)} clips: tuning on {len(tune)} (odd subjects), checking on {len(check)} (even)")

    best = None
    for max_dev, min_snr in itertools.product(DEVIATIONS, SNRS):
        kept_good, wrong = window_scores(tune, max_dev, min_snr)
        if wrong <= MAX_WRONG_SHARE and (best is None or kept_good > best[0]):
            best = (kept_good, max_dev, min_snr, wrong)
    _, max_dev, min_snr, wrong = best
    print(f"\nwindow thresholds: deviation <= {max_dev} bpm, SNR >= {min_snr} dB "
          f"({wrong:.1%} of trusted tuning windows wrong)")

    min_fraction = next((f for f in FRACTIONS if clip_results(tune, max_dev, min_snr, f)[1] == 0),
                        FRACTIONS[-1])
    print(f"clip threshold: at least {min_fraction:.0%} of windows trusted")

    for label, part in (("tuning half", tune), ("checking half", check)):
        kept_good, wrong = window_scores(part, max_dev, min_snr)
        decided, bad, errors = clip_results(part, max_dev, min_snr, min_fraction)
        mae = np.mean(np.abs(errors)) if decided else float("nan")
        print(f"  {label:14s} windows: {kept_good} correct kept, {wrong:.1%} of kept wrong | "
              f"clips: {decided}/{len(part)} decided, {bad} wrong, MAE {mae:.2f}")

    print("\nper encoding (checking half):")
    for path in args.csv:
        part = [c for c in check if c[1] == path]
        decided, bad, errors = clip_results(part, max_dev, min_snr, min_fraction)
        print(f"  {path:40s} {decided}/{len(part)} decided, {bad} wrong")


if __name__ == "__main__":
    main()
