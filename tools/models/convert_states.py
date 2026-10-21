"""Converts the initial recurrent states of the streaming rPPG models to the engine's binary format.

Format (little endian):
    char[8]  magic "RPPGSTAT"
    uint32   version (1)
    uint32   tensor count
    repeated:
        uint32 name length, name bytes (UTF-8)
        uint32 ndim, int64 dims[ndim]
        float32 data[prod(dims)]

Usage:
    python convert_states.py state.json out.state.bin          (ME-rPPG)
    python convert_states.py state.gz   out.state.bin          (FacePhys)
"""
import gzip
import json
import struct
import sys

import numpy as np


def load(path):
    if path.endswith(".gz"):
        with gzip.open(path, "rb") as f:
            return json.loads(f.read().decode())
    with open(path, "r") as f:
        return json.load(f)


def main(src, dst):
    state = load(src)
    with open(dst, "wb") as f:
        f.write(b"RPPGSTAT")
        f.write(struct.pack("<II", 1, len(state)))
        for name, value in state.items():
            a = np.asarray(value, dtype=np.float32)
            nb = name.encode("utf-8")
            f.write(struct.pack("<I", len(nb)))
            f.write(nb)
            f.write(struct.pack("<I", a.ndim))
            f.write(struct.pack("<" + "q" * a.ndim, *a.shape))
            f.write(np.ascontiguousarray(a).tobytes())
    print(f"{dst}: {len(state)} tensors")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
