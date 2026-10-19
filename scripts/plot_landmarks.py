"""Plot raw vs smoothed face positions from `rppg_detect --landmarks-csv`.

Shows the nose tip's x and y position over time. Use it to choose the
smoothing window: the smoothed line should run through the middle of the raw
wobble (jitter removed) without trailing behind real head movement (no lag).
Frames filled in by interpolation are marked with x.

Usage:
    .venv/bin/python scripts/plot_landmarks.py track.csv [--out plot.png]
"""

import argparse
import csv
from collections import defaultdict

import matplotlib.pyplot as plt


def load_track(path):
    """Returns {kind: {column: [values...]}} for kind in "raw" and "smoothed"."""
    columns = defaultdict(lambda: defaultdict(list))
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            kind = row.pop("kind")
            for name, value in row.items():
                columns[kind][name].append(float(value))
    return columns


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="file written by rppg_detect --landmarks-csv")
    parser.add_argument("--out", help="save to this image instead of opening a window")
    args = parser.parse_args()

    track = load_track(args.csv)
    raw, smoothed = track["raw"], track["smoothed"]
    filled = [i for i, detected in enumerate(raw["detected"]) if detected == 0]

    fig, axes = plt.subplots(2, 1, sharex=True, figsize=(12, 6))
    for axis, coord in zip(axes, ("x", "y")):
        column = f"nose_tip_{coord}"
        axis.plot(raw["time_sec"], raw[column], color="tab:red", linewidth=0.8, label="raw")
        axis.plot(smoothed["time_sec"], smoothed[column], color="tab:green", label="smoothed")
        axis.scatter([raw["time_sec"][i] for i in filled], [raw[column][i] for i in filled],
                     marker="x", color="black", label="filled in", zorder=3)
        axis.set_ylabel(f"nose tip {coord} (px)")
        axis.grid(alpha=0.3)
    axes[0].legend(loc="upper right")
    axes[-1].set_xlabel("time (s)")
    fig.suptitle("Face position: raw vs smoothed")
    fig.tight_layout()

    if args.out:
        fig.savefig(args.out, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
