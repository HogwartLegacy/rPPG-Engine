"""Builds aria2c input files for the public benchmark datasets.

MCD-rPPG (CC-BY-4.0, https://huggingface.co/datasets/kyegorov/mcd_rppg):
    a reproducible random subset of subjects, all 3 cameras x 2 states.
UBFC-rPPG DATASET_2 (https://sites.google.com/view/ybenezeth/ubfcrppg),
    via the public Hugging Face mirror thachha901/UBFC (42 subjects).

Usage:
    python make_download_lists.py --db db.csv --out D:/rppg_data --mcd-subjects 30
"""
import argparse
import csv
import json
import os
import random
import urllib.request

MCD = "https://huggingface.co/datasets/kyegorov/mcd_rppg/resolve/main/"
UBFC_API = "https://huggingface.co/api/datasets/thachha901/UBFC/tree/main/UBFC"
UBFC = "https://huggingface.co/datasets/thachha901/UBFC/resolve/main/"


def aria_entry(url, out_dir, name):
    return f"{url}\n  dir={out_dir}\n  out={name}\n"


def mcd_lists(db_path, root, n_subjects, seed):
    rows = list(csv.DictReader(open(db_path, newline="", encoding="utf-8")))
    by_subject = {}
    for r in rows:
        by_subject.setdefault(r["patient_id"], []).append(r)
    # keep subjects with all 6 recordings (3 cameras x before/after)
    complete = sorted(s for s, rs in by_subject.items() if len(rs) == 6)
    rng = random.Random(seed)
    chosen = sorted(rng.sample(complete, min(n_subjects, len(complete))), key=int)
    small, videos = [], []
    for s in chosen:
        for r in by_subject[s]:
            for key in ("ppg_sync", "meta"):
                sub, name = r[key].split("/")
                small.append(aria_entry(MCD + r[key], os.path.join(root, "mcd_rppg", sub), name))
            sub, name = r["video"].split("/")
            videos.append(aria_entry(MCD + r["video"], os.path.join(root, "mcd_rppg", sub), name))
    return chosen, small, videos


def ubfc_lists(root):
    with urllib.request.urlopen(UBFC_API) as f:
        subjects = [e["path"] for e in json.load(f) if e["type"] == "directory"]
    subjects.sort(key=lambda p: int(p.rsplit("subject", 1)[1]))
    small, videos = [], []
    for p in subjects:
        sub = p.split("/")[-1]
        d = os.path.join(root, "ubfc_rppg", sub)
        small.append(aria_entry(UBFC + p + "/ground_truth.txt", d, "ground_truth.txt"))
        videos.append(aria_entry(UBFC + p + "/vid.avi", d, "vid.avi"))
    return subjects, small, videos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mcd-subjects", type=int, default=30)
    ap.add_argument("--seed", type=int, default=2026)
    a = ap.parse_args()

    chosen, mcd_small, mcd_vid = mcd_lists(a.db, a.out, a.mcd_subjects, a.seed)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "mcd_subjects.txt"), "w") as f:
        f.write("\n".join(chosen) + "\n")
    subjects, ubfc_small, ubfc_vid = ubfc_lists(a.out)
    # Ground-truth files first, then MCD videos (small), then UBFC videos (1.4-1.8 GB each).
    with open(os.path.join(a.out, "aria_small.txt"), "w") as f:
        f.writelines(mcd_small + ubfc_small)
    with open(os.path.join(a.out, "aria_mcd_videos.txt"), "w") as f:
        f.writelines(mcd_vid)
    with open(os.path.join(a.out, "aria_ubfc_videos.txt"), "w") as f:
        f.writelines(ubfc_vid)
    print(f"MCD subjects: {len(chosen)} -> {len(mcd_vid)} videos; UBFC subjects: {len(subjects)}")


if __name__ == "__main__":
    main()
