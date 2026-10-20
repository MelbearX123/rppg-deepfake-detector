"""Plot the face and each region's heart rate over time from `rppg_detect --heart-rate-csv`.

Top row, the face (regions combined, nasal dorsum left out): heart rate per window,
filled dots where the window is trusted; and how far the regions are from each
other, with the agreement threshold. Other rows, each region: heart rate (dots:
straight from the spectrum, line: after the median filter) and SNR, with the
SNR floor.

Usage:
    .venv/bin/python scripts/plot_heart_rate.py heart_rate.csv [--out plot.png]
"""

import argparse
import csv
from collections import defaultdict

import matplotlib.pyplot as plt


def load_windows(path):
    """Returns {region: list of window rows}, in file order. Empty cells become None."""
    regions = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            regions[row["region"]].append(
                {k: float(v) if v else None for k, v in row.items() if k != "region"})
    return regions


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="file written by rppg_detect --heart-rate-csv")
    parser.add_argument("--min-snr", type=float, default=-3.0,
                        help="SNR floor in dB (limits::kMinWindowSnrDb)")
    parser.add_argument("--max-deviation", type=float, default=3.0,
                        help="agreement threshold in bpm (limits::kMaxRegionDeviationBpm)")
    parser.add_argument("--out", help="save to this image instead of opening a window")
    args = parser.parse_args()

    regions = load_windows(args.csv)
    face = regions.pop("face", [])
    if not regions:
        raise SystemExit("no region has heart-rate windows")

    fig, axes = plt.subplots(len(regions) + 1, 2, figsize=(14, 2 * (len(regions) + 1)),
                             sharex=True, squeeze=False)
    bpm_axis, deviation_axis = axes[0]
    times = [w["center_sec"] for w in face]
    bpm_axis.plot(times, [w["smoothed_bpm"] for w in face], color="black", linewidth=0.8)
    for trusted, style in ((1.0, "o"), (0.0, "x")):
        chosen = [w for w in face if w["trusted"] == trusted]
        bpm_axis.plot([w["center_sec"] for w in chosen], [w["smoothed_bpm"] for w in chosen],
                      style, color="black", markersize=4,
                      label="trusted" if trusted else "not trusted")
    bpm_axis.set_ylabel("face\n(bpm)")
    bpm_axis.legend(loc="upper right", fontsize=8)
    bpm_axis.grid(alpha=0.3)
    deviation_axis.plot(times, [w["deviation_bpm"] for w in face], color="tab:purple")
    deviation_axis.axhline(args.max_deviation, color="black", linestyle="--", linewidth=0.8)
    deviation_axis.set_ylabel("region spread\n(bpm)")
    deviation_axis.grid(alpha=0.3)

    for (bpm_axis, snr_axis), (region, windows) in zip(axes[1:], regions.items()):
        times = [w["center_sec"] for w in windows]
        bpm_axis.plot(times, [w["peak_bpm"] for w in windows], "o", color="tab:gray",
                      markersize=3, label="peak")
        bpm_axis.plot(times, [w["smoothed_bpm"] for w in windows], color="tab:red",
                      label="smoothed")
        bpm_axis.set_ylabel(f"{region}\n(bpm)")
        bpm_axis.grid(alpha=0.3)

        snr_axis.plot(times, [w["snr_db"] for w in windows], color="tab:blue")
        snr_axis.axhline(args.min_snr, color="black", linestyle="--", linewidth=0.8)
        snr_axis.set_ylabel("SNR (dB)")
        snr_axis.grid(alpha=0.3)

    axes[1][0].legend(loc="upper right", fontsize=8)
    axes[-1][0].set_xlabel("window center (s)")
    axes[-1][1].set_xlabel("window center (s)")
    fig.suptitle("Heart rate: face and each region (dashed: trust thresholds)")
    fig.tight_layout()

    if args.out:
        fig.savefig(args.out, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
