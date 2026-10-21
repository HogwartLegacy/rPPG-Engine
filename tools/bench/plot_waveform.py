"""Plots the engine's pulse waveform against the contact PPG of UBFC-rPPG.

Usage: python plot_waveform.py bvp.csv ground_truth.txt out.png [t_start]
(bvp.csv from `rppg_cli --bvp-csv`, sampled at 30 Hz on the video clock)
"""
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import scipy.signal as ss


def norm(x, fs):
    b, a = ss.butter(2, [0.67, 3.33], btype="band", fs=fs)
    y = ss.filtfilt(b, a, x - np.mean(x))
    return (y - y.mean()) / y.std()


def main(bvp_csv, gt_path, out, t0=20.0, dur=10.0):
    d = pd.read_csv(bvp_csv)
    fs = 1.0 / np.median(np.diff(d.t))
    with open(gt_path) as f:
        ppg = np.array([float(v) for v in f.readline().split()])
    t_ref = np.arange(len(ppg)) / 30.0
    ref = np.interp(d.t, t_ref, ppg)
    k = (d.t >= t0) & (d.t < t0 + dur)
    r = norm(ref, fs)[k]
    fig, ax = plt.subplots(figsize=(10, 3.2))
    ax.plot(d.t[k], r, color="k", lw=1.6, label="contact PPG (CMS50E)")
    for name, color in (("FUSED", "tab:red"), ("FacePhys", "tab:blue"), ("POS-MR", "tab:green")):
        if name not in d:
            continue
        y = norm(d[name].values, fs)[k]
        lag = np.argmax(ss.correlate(r, y, mode="full")) - (len(y) - 1)  # display alignment only
        ax.plot(d.t[k], np.roll(y, lag), color=color, lw=1.0, alpha=0.85,
                label=f"{name} (r={np.corrcoef(r, np.roll(y, lag))[0, 1]:.2f})")
    ax.set_xlabel("time (s)")
    ax.set_yticks([])
    ax.legend(loc="upper right", fontsize=8, ncol=4)
    ax.set_title("rPPG pulse vs. contact PPG (z-scored, 10 s)")
    fig.tight_layout()
    fig.savefig(out, dpi=110)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]) if len(sys.argv) > 4 else 20.0)
