#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["matplotlib"]
# ///
"""THE ZEEMAN DOUBLET, charted (Phase 14 step 84, TO_PROJECT_SPEC §13.13): the Boris gyroscope's two lines against
ω_B/2 ± sqrt(ω₀² + ω_B²/4), in cents, over the keyboard for each field — prewarped (the gate's 0.1 cent) and raw
(the red: the small-angle ε and the Boris angle, a cent sharp at 2 kHz). `voxo_suzu_tests` writes
`boris_doublet.csv` under SUZU_EVIDENCE; this draws it.

  uv run tools/boris_chart.py <dir> [--out <png>]
"""
import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    d = sys.argv[1]
    out = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else os.path.join(d, "boris_doublet.png")
    rows = list(csv.DictReader(open(os.path.join(d, "boris_doublet.csv"))))
    fields = sorted({float(r["f_B"]) for r in rows})
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharex=True)
    for ax, pw, title in ((axes[0], "1", "prewarped: ε = 2 sin(ω₀dt/2), θ = ω_B dt (the gate: 0.1 cent)"), (axes[1], "0", "raw (the red): ε = ω₀dt, θ = 2 atan(ω_B dt/2)")):
        for fb in fields:
            sel = [r for r in rows if r["prewarp"] == pw and float(r["f_B"]) == fb]
            sel.sort(key=lambda r: float(r["note_hz"]))
            ax.plot([float(r["note_hz"]) for r in sel], [float(r["cents_plus"]) for r in sel], "-o", ms=3, label=f"f_B {fb:.0f} Hz, the + line")
            ax.plot([float(r["note_hz"]) for r in sel], [float(r["cents_minus"]) for r in sel], "--", lw=0.8, color=ax.lines[-1].get_color())
        ax.set_xscale("log"); ax.set_xlabel("the note, Hz"); ax.set_ylabel("cents from ω_B/2 ± sqrt(ω₀² + ω_B²/4)"); ax.set_title(title, fontsize=9); ax.grid(alpha=0.3)
    axes[0].axhspan(-0.1, 0.1, color="#cfe8cf", alpha=0.5, label="the gate ±0.1 cent")
    axes[0].legend(fontsize=7, loc="upper left")
    fig.suptitle("The Boris gyroscope's Zeeman doublet: the scheme's lines against the continuous ones (dashed: the − line)")
    fig.tight_layout(); fig.savefig(out, dpi=130); print("wrote", out)


if __name__ == "__main__":
    main()
