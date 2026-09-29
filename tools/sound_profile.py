#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "matplotlib", "scipy"]
# ///
"""The SOUND PROFILE of a Voxo source (Phase 8 step 56, the author's ask):
draws what `midi-sink --dev --voxo-profile <dir> [--voxo-source suzu]
[--voxo-preset <p>]` wrote — every MIDI note 21..108 struck at velocity 100,
held 0.5 s, released 0.3 s, offline at 48 k — as one figure:

  1. the level across the keyboard: the held window's peak and RMS in dBFS
     per note (a flat line is an instrument whose notes are equally loud in
     amplitude; the ear's equal-loudness curve is not applied);
  2. the first three harmonics' levels per note (the timbre across the
     keyboard; a pure sine has only the first);
  3. a spectrogram of the whole run, log-frequency, so the strikes, the
     releases and any modulation are seen at once.

  uv run tools/sound_profile.py <dir> [--title "Suzu, the defaults"] [--out <png>]
"""
import argparse
import csv
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal


def read_wav(path):
    # the bench writes 32-bit FLOAT WAV, which the standard library's wave module refuses (format 3)
    from scipy.io import wavfile
    rate, data = wavfile.read(path)
    if data.dtype.kind == "i":
        data = data.astype(np.float64) / float(2 ** (8 * data.dtype.itemsize - 1))
    data = np.asarray(data, dtype=np.float64)
    if data.ndim == 2:
        data = data[:, 0]
    return rate, data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--title", default=None)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    rows = list(csv.DictReader(open(os.path.join(a.dir, "profile.csv"))))
    notes = np.array([int(r["note"]) for r in rows])
    peak = np.array([float(r["peak_dbfs"]) for r in rows]); rms = np.array([float(r["rms_dbfs"]) for r in rows])
    h = [np.array([float(r[f"h{k}_db"]) for r in rows]) for k in (1, 2, 3)]
    rate, x = read_wav(os.path.join(a.dir, "profile.wav"))
    title = a.title or os.path.basename(os.path.abspath(a.dir))

    fig, axes = plt.subplots(3, 1, figsize=(12, 11), constrained_layout=True)
    fig.suptitle(f"Sound profile — {title}", fontsize=14)
    ax = axes[0]
    ax.plot(notes, peak, label="peak", color="#1f77b4", lw=1.6)
    ax.plot(notes, rms, label="RMS", color="#ff7f0e", lw=1.6)
    ax.set_xlabel("MIDI note (21 = A0 … 108 = C8), velocity 100, held 0.1–0.5 s")
    ax.set_ylabel("level, dBFS"); ax.grid(True, alpha=0.3); ax.legend(loc="lower left")
    ax.set_title(f"level across the keyboard — peak spread {peak.max() - peak.min():.2f} dB, RMS spread {rms.max() - rms.min():.2f} dB", fontsize=10)
    for octave in range(24, 109, 12): ax.axvline(octave, color="k", alpha=0.08)
    ax = axes[1]
    for k, hh, c in zip((1, 2, 3), h, ("#2ca02c", "#d62728", "#9467bd")):
        ax.plot(notes, hh, label=f"harmonic {k}", color=c, lw=1.4)
    ax.set_ylim(max(-120, min(hh.min() for hh in h) - 5), 0)
    ax.set_xlabel("MIDI note"); ax.set_ylabel("level, dBFS"); ax.grid(True, alpha=0.3); ax.legend(loc="lower left")
    ax.set_title("the first three harmonics (a Goertzel at f, 2f, 3f over the held window)", fontsize=10)
    ax = axes[2]
    nper = 2048
    f, t, S = signal.spectrogram(x, fs=rate, nperseg=nper, noverlap=nper // 2, window="hann", scaling="spectrum", mode="magnitude")
    S_db = 20 * np.log10(np.maximum(S, 1e-7))
    keep = f > 20
    ax.pcolormesh(t, f[keep], S_db[keep], shading="auto", cmap="magma", vmin=-100, vmax=0)
    ax.set_yscale("log"); ax.set_ylim(20, rate / 2)
    ax.set_xlabel("time, s (each note 0.8 s: 0.5 held, 0.3 released)"); ax.set_ylabel("Hz")
    ax.set_title("the run as a spectrogram (magnitude, dBFS; Hann 2048)", fontsize=10)
    out = a.out or os.path.join(a.dir, "profile.png")
    fig.savefig(out, dpi=110)
    print(f"{out}: {len(notes)} notes; peak {peak.min():.1f}..{peak.max():.1f} dBFS (spread {peak.max() - peak.min():.2f}), "
          f"RMS {rms.min():.1f}..{rms.max():.1f} (spread {rms.max() - rms.min():.2f}); "
          f"h2 max {h[1].max():.1f} dBFS, h3 max {h[2].max():.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
