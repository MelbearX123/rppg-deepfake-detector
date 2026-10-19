"""Plot each region's heart rate and SNR over time from `rppg_detect --heart-rate-csv`.

Left: heart rate per window (dots: straight from the spectrum, line: after the
median filter). Right: SNR per window, with the threshold below which a region
is too weak to trust. A trustworthy region has a flat heart-rate line and SNR
above the threshold.

Usage:
    .venv/bin/python scripts/plot_heart_rate.py heart_rate.csv [--min-snr 0.0] [--out plot.png]
"""

import argparse
import csv
from collections import defaultdict

import matplotlib.pyplot as plt


def load_windows(path):
    """Returns {region: list of window rows}, in file order."""
    regions = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            regions[row["region"]].append({k: float(v) for k, v in row.items() if k != "region"})
    return regions


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="file written by rppg_detect --heart-rate-csv")
    parser.add_argument("--min-snr", type=float, default=0.0,
                        help="SNR threshold in dB (limits::kMinSnrDb)")
    parser.add_argument("--out", help="save to this image instead of opening a window")
    args = parser.parse_args()

    regions = load_windows(args.csv)
    if not regions:
        raise SystemExit("no region has heart-rate windows")

    fig, axes = plt.subplots(len(regions), 2, figsize=(14, 2 * len(regions)), sharex=True,
                             squeeze=False)
    for (bpm_axis, snr_axis), (region, windows) in zip(axes, regions.items()):
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

    axes[0][0].legend(loc="upper right", fontsize=8)
    axes[-1][0].set_xlabel("window center (s)")
    axes[-1][1].set_xlabel("window center (s)")
    fig.suptitle("Heart rate and SNR per region (dashed: SNR threshold)")
    fig.tight_layout()

    if args.out:
        fig.savefig(args.out, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
