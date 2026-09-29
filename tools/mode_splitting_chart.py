#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "matplotlib"]
# ///
"""MODE SPLITTING, charted once (Phase 8 step 57, SYNTH §2.5's "feature,
named"): two magic-circle cells at the same pitch, coupled by κ through the
shared-potential kick, split into a close pair — the normal modes at ε² and
ε²(1 + 2κ') (κ' = κ·ε₀²/ε², 1 at the fundamental) — and the sum beats at
their difference. `voxo_suzu_tests` writes `mode_splitting.csv` (κ, the
analytic split in Hz, the measured beat) and `mode_splitting_spectrum.csv`
(the pair's spectrum at one κ); this draws both as one figure.

  uv run tools/mode_splitting_chart.py <dir> [--out <png>]
"""
import argparse
import csv
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    rows = list(csv.DictReader(open(os.path.join(a.dir, "mode_splitting.csv"))))
    kappa = np.array([float(r["kappa"]) for r in rows])
    analytic = np.array([float(r["split_hz_analytic"]) for r in rows])
    measured = np.array([float(r["split_hz_measured"]) for r in rows])
    spec = list(csv.DictReader(open(os.path.join(a.dir, "mode_splitting_spectrum.csv"))))
    f = np.array([float(r["hz"]) for r in spec]); m = np.array([float(r["db"]) for r in spec])
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.6), constrained_layout=True)
    ax = axes[0]
    ax.plot(kappa, analytic, "k--", lw=1.2, label="analytic: f₊ − f₋ of the joint map")
    ax.plot(kappa, measured, "o", color="#d62728", ms=5, label="measured: the beat of the sum")
    ax.set_xlabel("coupling κ (the swirl adds up to 0.5)"); ax.set_ylabel("split, Hz"); ax.grid(True, alpha=0.3); ax.legend()
    ax.set_title("two cells at A3 (220 Hz), 96 kHz: the split against κ", fontsize=10)
    ax = axes[1]
    ax.plot(f, m, color="#1f77b4", lw=1.2)
    ax.set_xlim(f.min(), f.max()); ax.set_ylim(max(m.max() - 90, m.min()), m.max() + 3)
    ax.set_xlabel("Hz"); ax.set_ylabel("dB"); ax.grid(True, alpha=0.3)
    ax.set_title(f"the pair's spectrum at κ = {spec[0]['kappa']}: one placed partial, split in two (4 s Hann)", fontsize=10)
    fig.suptitle("Suzu — mode splitting (SYNTH §2.5): coupling as a timbre control", fontsize=13)
    out = a.out or os.path.join(a.dir, "mode_splitting.png")
    fig.savefig(out, dpi=110)
    print(f"{out}: {len(kappa)} κ points, split {measured.min():.2f}..{measured.max():.2f} Hz")
    return 0


if __name__ == "__main__":
    sys.exit(main())
