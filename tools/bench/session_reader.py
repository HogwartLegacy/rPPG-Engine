"""Reads .rppgsess files (engine::FrameRecord array, MSVC/GCC x64 layout) into numpy."""
import struct
import sys

import numpy as np

NREG = 29
RECORD = np.dtype({
    "names": ["t", "face", "rgb", "n", "corr", "bg", "bg_valid", "pose", "luma", "saturated", "nn", "nn_valid"],
    "formats": ["<f8", "u1", ("<f4", (NREG, 3)), ("<u4", NREG), ("<f4", 6), ("<f4", 3), "u1", ("<f4", 6), "<f4",
                "<f4", ("<f4", 2), ("u1", 2)],
    "offsets": [0, 8, 12, 360, 476, 500, 512, 516, 540, 544, 548, 556],
    "itemsize": 560,
})


def read(path):
    with open(path, "rb") as f:
        magic = f.read(8)
        size, n = struct.unpack("<QQ", f.read(16))
        t0 = struct.unpack("<d", f.read(8))[0]
        if magic != b"RPPGSES1" or size != RECORD.itemsize:
            raise ValueError(f"{path}: unexpected format (record size {size})")
        return np.frombuffer(f.read(n * size), dtype=RECORD, count=n), t0


def cheek_asymmetry(rec):
    """|log| ratio of left vs right cheek skin pixels: ~0 frontal, >1 for profile views."""
    left = rec["n"][:, 21].astype(float) + rec["n"][:, 23]
    right = rec["n"][:, 22].astype(float) + rec["n"][:, 24]
    ok = rec["face"] > 0
    return np.abs(np.log((left[ok] + 1) / (right[ok] + 1)))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        r, _ = read(p)
        a = cheek_asymmetry(r)
        print(f"{p}: frames={len(r)} asym median={np.median(a):.2f} p90={np.percentile(a, 90):.2f}")
