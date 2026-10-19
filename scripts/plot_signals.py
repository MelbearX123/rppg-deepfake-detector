"""Plot the green channel of each skin region from `rppg_detect --signals-csv`.

Each region's green value is shown as a percent change from its own average,
with slow drift (lighting, head movement) removed by subtracting a 1-second
moving average. On a still face, a faint wobble at heart-rate speed
(about 1 per second) should show up in the usable regions.

Usage:
    .venv/bin/python scripts/plot_signals.py signals.csv [--out plot.png]
"""

import argparse
import csv
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np

REGIONS = ["glabella", "forehead", "nasal_dorsum", "right_malar", "left_malar"]


def load_green(path):
    """Returns {region: (times, green values)}, skipping frames with no skin pixels."""
    rows = defaultdict(list)
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if row["g"]:
                rows[row["region"]].append((float(row["time_sec"]), float(row["g"])))
    return {region: np.array(values).T for region, values in rows.items()}


def remove_drift(times, values):
    """Percent change from the average, minus a ~1 s moving average."""
    percent = (values / values.mean() - 1.0) * 100.0
    fps = (len(times) - 1) / (times[-1] - times[0])
    window = max(1, round(fps))
    drift = np.convolve(percent, np.ones(window) / window, mode="same")
    return percent - drift


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="file written by rppg_detect --signals-csv")
    parser.add_argument("--out", help="save to this image instead of opening a window")
    args = parser.parse_args()

    green = load_green(args.csv)
    regions = [r for r in REGIONS if r in green]
    fig, axes = plt.subplots(len(regions), 1, sharex=True, figsize=(12, 2 * len(regions)))
    for axis, region in zip(np.atleast_1d(axes), regions):
        times, values = green[region]
        axis.plot(times, remove_drift(times, values), color="tab:green", linewidth=0.9)
        axis.set_ylabel(f"{region}\n(% change)")
        axis.grid(alpha=0.3)
    np.atleast_1d(axes)[-1].set_xlabel("time (s)")
    fig.suptitle("Green channel per region, drift removed")
    fig.tight_layout()

    if args.out:
        fig.savefig(args.out, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
