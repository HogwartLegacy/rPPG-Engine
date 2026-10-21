"""Aggregates rppg_bench CSV results into metric tables (Markdown + CSV) and plots.

Usage:
    python report.py results.csv [more.csv ...] --out docs/results
"""
import argparse
import os

import numpy as np
import pandas as pd


def metrics(df):
    err = df.hr_pred - df.hr_gt
    out = {
        "n": len(df),
        "MAE": np.mean(np.abs(err)),
        "RMSE": np.sqrt(np.mean(err ** 2)),
        "MAPE": np.mean(np.abs(err) / df.hr_gt) * 100,
        "r": np.corrcoef(df.hr_pred, df.hr_gt)[0, 1] if len(df) > 2 and df.hr_pred.std() > 0 else np.nan,
        "acc3": np.mean(np.abs(err) <= 3) * 100,
        "acc5": np.mean(np.abs(err) <= 5) * 100,
    }
    return pd.Series(out)


def add_groups(df):
    df = df.copy()
    df["camera"] = ""
    df["state"] = ""
    m = df.dataset.str.startswith("mcd")
    parts = df.loc[m, "video"].str.split("_", expand=True)
    if len(parts):
        df.loc[m, "camera"] = parts[1]
        df.loc[m, "state"] = parts[2]
    return df


def table(df, keys):
    g = df.groupby(keys, sort=False).apply(metrics, include_groups=False).reset_index()
    return g


def fmt(df):
    cols = [c for c in df.columns]
    lines = ["| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
    for _, r in df.iterrows():
        cells = []
        for c in cols:
            v = r[c]
            if isinstance(v, float):
                cells.append(f"{v:.3f}" if c == "r" else (f"{v:.1f}" if c in ("acc3", "acc5") else f"{v:.2f}"))
            else:
                cells.append(str(v))
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines)


def sqi_curve(df, taus=(0.0, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7)):
    rows = []
    for t in taus:
        d = df[df.sqi >= t]
        if len(d) < 10:
            continue
        m = metrics(d)
        rows.append({"sqi>=": t, "coverage%": 100 * len(d) / len(df), "MAE": m.MAE, "RMSE": m.RMSE, "acc5": m.acc5})
    return pd.DataFrame(rows)


def plots(df, out_dir, tag):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception:
        return
    d = df
    if d.empty:
        return
    mean = (d.hr_pred + d.hr_gt) / 2
    diff = d.hr_pred - d.hr_gt
    md, sd = diff.mean(), diff.std()
    fig, ax = plt.subplots(1, 2, figsize=(10, 4.2))
    ax[0].scatter(d.hr_gt, d.hr_pred, s=4, alpha=0.4)
    lim = [min(d.hr_gt.min(), d.hr_pred.min()) - 5, max(d.hr_gt.max(), d.hr_pred.max()) + 5]
    ax[0].plot(lim, lim, "k--", lw=1)
    ax[0].set_xlabel("reference HR (bpm)")
    ax[0].set_ylabel("rPPG HR (bpm)")
    ax[0].set_title(tag)
    ax[1].scatter(mean, diff, s=4, alpha=0.4)
    for y, ls in ((md, "-"), (md + 1.96 * sd, "--"), (md - 1.96 * sd, "--")):
        ax[1].axhline(y, color="r", ls=ls, lw=1)
    ax[1].set_xlabel("mean HR (bpm)")
    ax[1].set_ylabel("rPPG - reference (bpm)")
    ax[1].set_title(f"Bland-Altman: bias {md:.2f}, LoA +-{1.96 * sd:.2f}")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, f"ba_{tag}.png"), dpi=110)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("--out", default="results")
    ap.add_argument("--plots", default="")  # comma list of config:candidate:protocol to plot
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    df = pd.concat([pd.read_csv(p) for p in a.csv], ignore_index=True)
    df = df[(df.hr_gt > 0) & (df.hr_pred > 0)]
    df = add_groups(df)
    md = []
    h = df[df.protocol == "hrv"].copy()
    if not h.empty:
        h["metric"] = h.candidate.str.split("@").str[0]
        h["source"] = h.candidate.str.split("@").str[1]
        rows = []
        for (ds, cfg, m, s), g in h.groupby(["dataset", "config", "metric", "source"]):
            rows.append({"dataset": ds, "config": cfg, "metric": m, "source": s, "n": len(g),
                         "rPPG mean": g.hr_pred.mean(), "reference mean": g.hr_gt.mean(),
                         "MAE": (g.hr_pred - g.hr_gt).abs().mean(),
                         "r": np.corrcoef(g.hr_pred, g.hr_gt)[0, 1] if len(g) > 2 else np.nan})
        t = pd.DataFrame(rows)
        t.to_csv(os.path.join(a.out, "hrv.csv"), index=False)
        md.append("## HRV agreement with contact PPG\n\n" + fmt(t[t.source == "engine"]) + "\n")
        df = df[df.protocol != "hrv"]
    for proto in ("whole", "window", "stream"):
        d = df[df.protocol == proto]
        if d.empty:
            continue
        t = table(d, ["dataset", "config", "candidate"])
        t = t.sort_values(["dataset", "MAE"])
        t.to_csv(os.path.join(a.out, f"{proto}.csv"), index=False)
        md.append(f"## Protocol: {proto}\n\n" + fmt(t) + "\n")
        mcd = d[d.dataset.str.startswith("mcd")]
        if not mcd.empty:
            tc = table(mcd, ["config", "candidate", "camera"])
            tc.to_csv(os.path.join(a.out, f"{proto}_mcd_camera.csv"), index=False)
            ts = table(mcd, ["config", "candidate", "state"])
            ts.to_csv(os.path.join(a.out, f"{proto}_mcd_state.csv"), index=False)
    s = df[(df.protocol == "stream") & (df.candidate == "FUSED")]
    for (ds, cfg), d in s.groupby(["dataset", "config"]):
        c = sqi_curve(d)
        if not c.empty:
            md.append(f"### SQI gating ({ds}, {cfg}, stream)\n\n" + fmt(c) + "\n")
    with open(os.path.join(a.out, "tables.md"), "w") as f:
        f.write("\n".join(md))
    for spec in filter(None, a.plots.split(",")):
        cfg, cand, proto = spec.split(":")
        for ds, d in df[(df.config == cfg) & (df.candidate == cand) & (df.protocol == proto)].groupby("dataset"):
            plots(d, a.out, f"{ds}_{cfg}_{cand}_{proto}")
    print("\n".join(md))


if __name__ == "__main__":
    main()
