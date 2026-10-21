"""Inspects the quality of reference PPG signals (per-window HR and spectral SNR of the reference).

Usage: python gt_quality.py ppg_sync.txt fps [window_s]
"""
import sys

import numpy as np
import scipy.signal as ss


def load(path):
    vals = []
    with open(path) as f:
        for line in f:
            p = line.split()
            if p:
                vals.append(float(p[0]))
    return np.array(vals)


def main(path, fps, win=10.0):
    x = load(path)
    fs = float(fps)
    b, a = ss.butter(2, [0.67, 3.33], btype="band", fs=fs)
    y = ss.filtfilt(b, a, x - x.mean())
    n = int(win * fs)
    print(f"{path}: {len(x)} samples ({len(x) / fs:.1f} s), raw range {x.min():.0f}..{x.max():.0f}")
    for k0 in range(0, len(y) - n + 1, int(5 * fs)):
        seg = y[k0:k0 + n]
        f, p = ss.periodogram(seg, fs=fs, window="hann", nfft=1 << 14)
        band = (f >= 0.67) & (f <= 3.33)
        i = np.argmax(p * band)
        f0 = f[i]
        sig = p[(np.abs(f - f0) <= 0.1) | (np.abs(f - 2 * f0) <= 0.1)].sum()
        rest = p[band].sum() - p[band & (np.abs(f - f0) <= 0.1)].sum()
        flat = np.mean(np.abs(np.diff(x[k0:k0 + n])) == 0) * 100
        print(f"  t={k0 / fs:6.1f}s  HR={f0 * 60:6.1f}  SNR={10 * np.log10(sig / max(rest, 1e-12)):6.1f} dB  flat={flat:5.1f}%")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 10.0)
