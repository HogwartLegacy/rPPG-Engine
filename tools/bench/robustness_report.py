"""Robustness tables / chart from run_robustness.ps1 output (dataset column = perturbation)."""
import os
import sys

import numpy as np
import pandas as pd

ORDER = ["clean", "h264_crf18", "h264_crf23", "h264_crf28", "h264_crf35", "lowlight_0.25", "melanin_1.0", "noise_8", "flicker_3pct",
         "shake_15px", "shake_30px", "lowres_160"]


def main(csv, out_dir):
    d = pd.read_csv(csv)
    d = d[(d.hr_gt > 0) & (d.hr_pred > 0) & (d.protocol != "hrv")]
    d["ae"] = (d.hr_pred - d.hr_gt).abs()
    conds = [c for c in ORDER if c in set(d.dataset)]
    fused = d[d.candidate == "FUSED"]
    t1 = fused[fused.protocol == "stream"].pivot_table(index="dataset", columns="config", values="ae", aggfunc="mean")
    t1 = t1.reindex(conds)
    meth = d[(d.config == "methods") & (d.protocol == "window") & (d.candidate != "FUSED_RAW")]
    t2 = meth.pivot_table(index="dataset", columns="candidate", values="ae", aggfunc="mean").reindex(conds)
    acc = fused[(fused.protocol == "stream") & (fused.config == "engine")].groupby("dataset").ae.apply(
        lambda x: (x <= 5).mean() * 100).reindex(conds)
    t1["engine acc5 %"] = acc
    os.makedirs(out_dir, exist_ok=True)
    t1.to_csv(os.path.join(out_dir, "robustness_stream.csv"))
    t2.to_csv(os.path.join(out_dir, "robustness_methods_window.csv"))
    print("Real-time (stream) MAE, FUSED output:\n", t1.round(2).to_string())
    print("\nPer-method 10 s window MAE:\n", t2.round(2).to_string())
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        cols = [c for c in ["engine", "engine-lite", "classical-only"] if c in t1.columns]
        x = np.arange(len(conds))
        fig, ax = plt.subplots(figsize=(10, 4))
        for i, c in enumerate(cols):
            ax.bar(x + (i - 1) * 0.27, t1[c].values, width=0.27, label=c)
        ax.set_xticks(x)
        ax.set_xticklabels(conds, rotation=30, ha="right")
        ax.set_ylabel("MAE (bpm), real-time output")
        ax.set_title("Robustness on UBFC-rPPG (10 subjects, 60 s) under perturbations")
        ax.legend()
        fig.tight_layout()
        fig.savefig(os.path.join(out_dir, "robustness.png"), dpi=110)
    except Exception as e:
        print("plot skipped:", e)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
