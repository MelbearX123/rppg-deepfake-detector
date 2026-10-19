"""Plot each region's POS pulse signal from `rppg_detect --pulse-csv`, with its spectrum.

Left: the pulse signal over time. Right: how strong each frequency is, in beats
per minute, with the strongest one in the 42-240 bpm pulse band (0.7-4 Hz)
marked. If every region is measuring the same heartbeat, the peaks should line up.
This is a visual check only; the real heart-rate estimate is Stage 6.

Usage:
    .venv/bin/python scripts/plot_pulse.py pulse.csv [--out plot.png]
"""

import argparse
import csv

import matplotlib.pyplot as plt
import numpy as np

MIN_BPM = 42
MAX_BPM = 240


def load_pulses(path):
    """Returns (times, {region: values}), skipping regions with no pulse."""
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    times = np.array([float(row["time_sec"]) for row in rows])
    regions = [name for name in rows[0] if name != "time_sec"] if rows else []
    pulses = {}
    for region in regions:
        if rows[0][region]:
            pulses[region] = np.array([float(row[region]) for row in rows])
    return times, pulses


def spectrum_bpm(times, values):
    """Returns (bpm, power) for the pulse band, using a Hann window to reduce leakage."""
    rate = (len(times) - 1) / (times[-1] - times[0])
    centered = (values - values.mean()) * np.hanning(len(values))
    power = np.abs(np.fft.rfft(centered)) ** 2
    bpm = np.fft.rfftfreq(len(values), d=1.0 / rate) * 60.0
    band = (bpm >= MIN_BPM) & (bpm <= MAX_BPM)
    return bpm[band], power[band]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", help="file written by rppg_detect --pulse-csv")
    parser.add_argument("--out", help="save to this image instead of opening a window")
    args = parser.parse_args()

    times, pulses = load_pulses(args.csv)
    if not pulses:
        raise SystemExit("no region has a pulse signal")

    fig, axes = plt.subplots(len(pulses), 2, figsize=(14, 2 * len(pulses)),
                             gridspec_kw={"width_ratios": [3, 1]}, squeeze=False)
    for (signal_axis, spectrum_axis), (region, values) in zip(axes, pulses.items()):
        signal_axis.plot(times, values, color="tab:red", linewidth=0.9)
        signal_axis.set_ylabel(region)
        signal_axis.grid(alpha=0.3)

        bpm, power = spectrum_bpm(times, values)
        peak = bpm[np.argmax(power)]
        spectrum_axis.plot(bpm, power, color="tab:blue", linewidth=0.9)
        spectrum_axis.axvline(peak, color="black", linestyle="--", linewidth=0.8)
        spectrum_axis.set_title(f"peak {peak:.0f} bpm", fontsize=9)
        spectrum_axis.set_yticks([])
        print(f"{region:14s} peak {peak:.0f} bpm")

    axes[-1][0].set_xlabel("time (s)")
    axes[-1][1].set_xlabel("bpm")
    fig.suptitle("POS pulse per region, bandpass 0.7-4 Hz")
    fig.tight_layout()

    if args.out:
        fig.savefig(args.out, dpi=120)
    else:
        plt.show()


if __name__ == "__main__":
    main()
