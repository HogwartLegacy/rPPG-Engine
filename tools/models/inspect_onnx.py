"""Prints the input/output signature of ONNX models and runs a dummy inference."""
import sys
import time

import numpy as np
import onnx
import onnxruntime as ort


def describe(path):
    m = onnx.load(path)
    print(f"== {path}")
    print(f"   opset: {[(o.domain or 'ai.onnx', o.version) for o in m.opset_import]}  ir={m.ir_version}")
    so = ort.SessionOptions()
    so.intra_op_num_threads = 1
    sess = ort.InferenceSession(path, so, providers=["CPUExecutionProvider"])
    ins, outs = sess.get_inputs(), sess.get_outputs()
    print(f"   inputs ({len(ins)}):")
    for i in ins[:6]:
        print(f"     {i.name:24s} {i.type:16s} {i.shape}")
    if len(ins) > 6:
        print(f"     ... {len(ins) - 6} more, e.g. {ins[-1].name} {ins[-1].shape}")
    print(f"   outputs ({len(outs)}):")
    for o in outs[:6]:
        print(f"     {o.name:24s} {o.type:16s} {o.shape}")
    if len(outs) > 6:
        print(f"     ... {len(outs) - 6} more, e.g. {outs[-1].name} {outs[-1].shape}")
    feeds = {}
    for i in ins:
        shape = [d if isinstance(d, int) and d > 0 else 1 for d in i.shape]
        dtype = np.float32 if "float" in i.type else np.int64
        feeds[i.name] = np.zeros(shape, dtype=dtype) if len(shape) else np.array(1 / 30, dtype=dtype)
    t = time.perf_counter()
    n = 50
    for _ in range(n):
        res = sess.run(None, feeds)
    dt = (time.perf_counter() - t) / n * 1000
    print(f"   dummy run ok: out0 shape={np.asarray(res[0]).shape}, {dt:.2f} ms/run (1 thread)")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        describe(p)
