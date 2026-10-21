"""Generates scipy reference outputs used by tests/test_dsp.cpp.

Each line of the output file is: <name> <count> <v0> <v1> ...
"""
import sys

import numpy as np
import scipy.signal as ss
from scipy.sparse import spdiags


def tarvainen(x, lam):
    n = len(x)
    H = np.identity(n)
    D = spdiags(np.array([np.ones(n), -2 * np.ones(n), np.ones(n)]), np.array([0, 1, 2]), n - 2, n).toarray()
    return (H - np.linalg.inv(H + lam ** 2 * D.T @ D)) @ x


def main(out_path):
    rng = np.random.default_rng(7)
    fs = 30.0
    t = np.arange(300) / fs
    x = np.sin(2 * np.pi * 1.2 * t) + 0.5 * np.sin(2 * np.pi * 0.2 * t) + 0.3 * rng.standard_normal(t.size) + 0.02 * t
    rows = {"x": x}

    sos = ss.butter(2, [0.7, 3.0], btype="band", fs=fs, output="sos")
    rows["butter_bp2_sos"] = sos.ravel()
    rows["sosfiltfilt_bp2"] = ss.sosfiltfilt(sos, x)
    sos3 = ss.butter(3, [0.65, 3.5], btype="band", fs=fs, output="sos")
    rows["butter_bp3_sos"] = sos3.ravel()
    rows["sosfiltfilt_bp3"] = ss.sosfiltfilt(sos3, x)
    sos_lp = ss.butter(4, 2.0, btype="low", fs=fs, output="sos")
    rows["butter_lp4_sos"] = sos_lp.ravel()
    rows["sosfiltfilt_lp4"] = ss.sosfiltfilt(sos_lp, x)
    sos_hp = ss.butter(3, 0.5, btype="high", fs=fs, output="sos")
    rows["butter_hp3_sos"] = sos_hp.ravel()
    rows["sosfilt_hp3"] = ss.sosfilt(sos_hp, x)

    rows["detrend100"] = tarvainen(x, 100.0)
    rows["detrend10"] = tarvainen(x, 10.0)

    f, p = ss.periodogram(x, fs=fs, window="hann", nfft=1024, detrend="constant")
    rows["periodogram_hann_1024"] = p
    f, p = ss.welch(x, fs=fs, window="hann", nperseg=128, noverlap=64, nfft=512, detrend="constant")
    rows["welch_hann_128_64_512"] = p

    peaks, _ = ss.find_peaks(x, distance=10)
    rows["find_peaks_d10"] = peaks.astype(float)
    peaks, _ = ss.find_peaks(x, distance=5, prominence=0.8)
    rows["find_peaks_d5_p08"] = peaks.astype(float)

    with open(out_path, "w") as fh:
        for k, v in rows.items():
            v = np.asarray(v, dtype=float).ravel()
            fh.write(k + " " + str(v.size) + " " + " ".join(repr(float(a)) for a in v) + "\n")
    print("wrote", out_path, len(rows), "arrays")


if __name__ == "__main__":
    main(sys.argv[1])
