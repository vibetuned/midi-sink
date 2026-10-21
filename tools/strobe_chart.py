#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "matplotlib"]
# ///
"""THE STROBE CHARTS (Phase 11 step 75, KESSHO §2.5): draws what the Suzu suite
wrote under SUZU_STROBE_DUMP=<dir> — the Poincaré strobe's returns —

  strobe_1_1.csv            the undamped cell at A2 strobed at the bass (itself): one point
  strobe_3_2_just.csv       E3 at the just fifth over A2: two antipodal points
  strobe_3_2_et.csv         E3 equal-tempered: the two points precessing (1.955 cents)
  strobe_tritone.csv        D#3 (√2 over A2): the dotted ring
  strobe_duffing_drive.csv  the Duffing cell strobed by its own drive
  strobe_rotor_K<k>.csv     the kicked rotor strobed by its own kicks — Chirikov's section
                            (θ = atan2(y, x), p = u) for K 0.3 … 2.5
  strobe_membrane.csv       step 79: the membrane at C3 strobed at the bass (itself) — the
                            pickup's (s, ṡ/ω) at each return, scattered by the inharmonic partials
  strobe_toda.csv           step 83: the Toda ring at A3 strobed at the bass (itself), the
                            press full — the soliton train's section: the pulses pass
                            the pickup faster than the note, the points walk the ring
  strobe_boris.csv          step 84: the Boris gyroscope at A3 under a 20 Hz field strobed by its
                            own cyclotron clock — (x₁, x₂) at each full turn of the field: the
                            rosette's lobes as a closed figure

  uv run tools/strobe_chart.py <dir> [--out <png>]

Writes strobe_sections.png beside the CSVs (or --out).
"""
import csv
import math
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return np.zeros((0, 4))
    return np.array([[float(r["x"]), float(r["y"]), float(r["u"]), float(r["w"])] for r in rows])


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    d = sys.argv[1]
    out = os.path.join(d, "strobe_sections.png")
    if "--out" in sys.argv:
        out = sys.argv[sys.argv.index("--out") + 1]

    ratios = [("strobe_1_1.csv", "1:1 — A2 at the bass: one point"),
              ("strobe_3_2_just.csv", "3:2 — E3 at the just fifth: two points"),
              ("strobe_3_2_et.csv", "3:2 — E3 equal-tempered: precessing"),
              ("strobe_tritone.csv", "√2 — D#3 over A2: the dotted ring"),
              ("strobe_duffing_drive.csv", "the Duffing cell by its own drive"),
              ("strobe_membrane.csv", "the membrane at C3 at the bass (step 79)"),
              ("strobe_toda.csv", "the Toda ring at A3 at the bass (step 83)"),
              ("strobe_boris.csv", "the gyroscope by its cyclotron clock (step 84)")]
    rotor = sorted([f for f in os.listdir(d) if f.startswith("strobe_rotor_K") and f.endswith(".csv")],
                   key=lambda f: float(f[len("strobe_rotor_K"):-4]))
    ncol = max(len(ratios), len(rotor), 1)
    fig, axes = plt.subplots(2, ncol, figsize=(3.2 * ncol, 6.6))
    axes = np.atleast_2d(axes)
    for ax in axes.flat:
        ax.set_visible(False)

    for i, (name, title) in enumerate(ratios):
        ax = axes[0, i]; ax.set_visible(True)
        path = os.path.join(d, name)
        if not os.path.exists(path):
            ax.set_title(title + " (missing)", fontsize=8); continue
        pts = read(path)
        if len(pts):
            r = np.hypot(pts[:, 0], pts[:, 1]).max()
            t = np.linspace(0, 2 * math.pi, 200)
            ax.plot(r * np.cos(t), r * np.sin(t), color="0.85", lw=0.8)
            c = np.linspace(0.2, 1.0, len(pts))
            ax.scatter(pts[:, 0], pts[:, 1], s=6, c=c, cmap="viridis", lw=0)
            ax.set_xlim(-1.15 * r, 1.15 * r); ax.set_ylim(-1.15 * r, 1.15 * r)
        ax.set_aspect("equal"); ax.set_title(f"{title}\n{len(pts)} returns", fontsize=8)
        ax.tick_params(labelsize=6)

    for i, name in enumerate(rotor):
        ax = axes[1, i]; ax.set_visible(True)
        pts = read(os.path.join(d, name))
        k = name[len("strobe_rotor_K"):-4]
        if len(pts):
            th = np.arctan2(pts[:, 1], pts[:, 0]); p = pts[:, 2]
            ax.scatter(th, p, s=2, color="k", lw=0)
        ax.set_xlim(-math.pi, math.pi); ax.set_ylim(-math.pi, math.pi)
        ax.set_title(f"the rotor by its kicks, K = {k}\n(θ, p) at each kick: Chirikov's section", fontsize=8)
        ax.set_xlabel("θ", fontsize=7); ax.set_ylabel("p", fontsize=7); ax.tick_params(labelsize=6)

    fig.suptitle("The Poincaré strobe (step 75): the returns of the undamped cell at the bass phasor, and the rotor's section by its own kicks", fontsize=9)
    fig.tight_layout()
    fig.savefig(out, dpi=140)
    print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
