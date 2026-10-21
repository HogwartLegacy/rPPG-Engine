"""Quick summary of a sweep CSV: FUSED MAE per config (window by camera / whole / stream)."""
import sys

import pandas as pd

d = pd.read_csv(sys.argv[1])
cand = sys.argv[2] if len(sys.argv) > 2 else "FUSED"
d = d[(d.hr_gt > 0) & (d.hr_pred > 0)]
d["cam"] = d.video.str.split("_").str[1].fillna("-")
d.loc[~d.dataset.str.startswith("mcd"), "cam"] = d.dataset
d["ae"] = (d.hr_pred - d.hr_gt).abs()
for proto in ("window", "stream", "whole"):
    w = d[(d.protocol == proto) & (d.candidate == cand)]
    if w.empty:
        continue
    t = w.pivot_table(index="config", columns="cam", values="ae", aggfunc="mean")
    t["all"] = w.groupby("config").ae.mean()
    t["acc5"] = w.groupby("config").ae.apply(lambda x: (x <= 5).mean() * 100)
    print(f"== {proto} ({cand})")
    print(t.round(2).sort_values("all").to_string())
