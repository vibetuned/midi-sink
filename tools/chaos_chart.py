#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "matplotlib", "scipy"]
# ///
"""The CHAOS CHARTS (Phase 8 step 58, SYNTH §2.3, §2.10, §5): draws what
`midi-sink --dev --voxo-chart <dir>` wrote —

  rotor_sweep.wav/.csv    the kicked rotor on A3, K swept 0 → 2.5 by the mod wheel
                          over 24 s: the spectrogram with K on the top axis — the KAM
                          transition, audible (the visual Chirikov's sibling);
  duffing_clang.wav       the Duffing cell, C4 struck at velocity 127: the pitch against
                          time (the clang-and-settle) over the amplitude;
  duffing_drive.wav/.csv  the Duffing cell driven at the note, the press (the drive's
                          amplitude) swept 0 → 1 over 24 s: the spectrogram — order to chaos.

  uv run tools/chaos_chart.py <dir>
"""
import csv
import os
import sys

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal
from scipy.io import wavfile


def read_wav(path):
    rate, data = wavfile.read(path)
    data = np.asarray(data, dtype=np.float64)
    if data.ndim == 2:
        data = data[:, 0]
    return rate, data


def sweep_figure(d, name, title, param, out):
    rate, x = read_wav(os.path.join(d, name + ".wav"))
    rows = list(csv.DictReader(open(os.path.join(d, name + ".csv"))))
    t_par = np.array([float(r["time_s"]) for r in rows]); p_par = np.array([float(r[param]) for r in rows])
    fig, ax = plt.subplots(figsize=(12, 5.5), constrained_layout=True)
    nper = 4096
    f, t, S = signal.spectrogram(x, fs=rate, nperseg=nper, noverlap=nper * 3 // 4, window="hann", scaling="spectrum", mode="magnitude")
    S_db = 20 * np.log10(np.maximum(S, 1e-7))
    keep = (f > 40) & (f < 8000)
    ax.pcolormesh(t, f[keep], S_db[keep], shading="auto", cmap="magma", vmin=-100, vmax=-10)
    ax.set_yscale("log"); ax.set_ylim(40, 8000)
    ax.set_xlabel("time, s"); ax.set_ylabel("Hz")
    top = ax.secondary_xaxis("top", functions=(lambda s: np.interp(s, t_par, p_par), lambda k: np.interp(k, p_par, t_par)))
    top.set_xlabel(param)
    ax.set_title(title, fontsize=11)
    fig.savefig(out, dpi=110)
    print(f"{out}: {len(x) / rate:.1f} s, {param} {p_par.min():.2f}..{p_par.max():.2f}")


def clang_figure(d, out):
    rate, x = read_wav(os.path.join(d, "duffing_clang.wav"))
    win = int(0.02 * rate)
    times, pitch, amp = [], [], []
    for at in range(0, len(x) - win, win // 2):
        seg = x[at:at + win]
        cross = np.where((seg[:-1] < 0) & (seg[1:] >= 0))[0]
        if len(cross) > 2:
            frac = -seg[cross] / (seg[cross + 1] - seg[cross])
            inst = cross + frac
            f = (len(inst) - 1) * rate / (inst[-1] - inst[0])
            times.append(at / rate); pitch.append(1200 * np.log2(f / 261.6256)); amp.append(np.max(np.abs(seg)))
    times, pitch, amp = np.array(times), np.array(pitch), np.array(amp)
    fig, ax = plt.subplots(figsize=(10, 4.6), constrained_layout=True)
    ax.plot(times, pitch, color="#d62728", lw=1.6, label="pitch, cents from C4")
    ax.set_xlabel("time, s"); ax.set_ylabel("cents from C4", color="#d62728"); ax.grid(True, alpha=0.3)
    ax2 = ax.twinx(); ax2.plot(times, 20 * np.log10(np.maximum(amp, 1e-6)), color="#1f77b4", lw=1.2, label="amplitude, dBFS")
    ax2.set_ylabel("peak, dBFS", color="#1f77b4")
    ax.set_title(f"Suzu — the Duffing cell (β 8), C4 at velocity 127: clangs {pitch.max():+.0f} cents sharp, settles to {pitch[-5:].mean():+.1f} as it decays (2 s T60)", fontsize=10)
    fig.savefig(out, dpi=110)
    print(f"{out}: clang {pitch.max():+.1f} cent, settled {pitch[-5:].mean():+.2f}")


def main():
    d = sys.argv[1]
    if os.path.exists(os.path.join(d, "sax_ramp.wav")):
        sweep_figure(d, "sax_ramp", "Suzu — the saxophone (SYNTH §2.12): A3, the breath ramped 0 → 1 over 16 s — silence under the threshold, the tone, the reed's closing", "breath", os.path.join(d, "sax_ramp.png"))
    if os.path.exists(os.path.join(d, "trumpet_lips.wav")):
        sweep_figure(d, "trumpet_lips", "Suzu — the trumpet (SYNTH §2.12): A3, the embouchure (CC 74) swept 0 → 127 over 16 s — the registers, the bend within each", "cc74", os.path.join(d, "trumpet_lips.png"))
    if os.path.exists(os.path.join(d, "flute_ramp.wav")):
        sweep_figure(d, "flute_ramp", "Suzu — the flute (SYNTH §2.13): A4, the breath ramped 0 → 1 over 16 s — silence, the tone flat and rising, the octave by itself", "breath", os.path.join(d, "flute_ramp.png"))
    sweep_figure(d, "rotor_sweep", "Suzu — the kicked rotor (SYNTH §2.3): A3, K swept 0 → 2.5 by the mod wheel — order, shimmer, island chains past K_c ≈ 0.97, storm", "K", os.path.join(d, "rotor_sweep.png"))
    clang_figure(d, os.path.join(d, "duffing_clang.png"))
    sweep_figure(d, "duffing_drive", "Suzu — the driven Duffing cell (SYNTH §2.10): A3, β 8, the drive's amplitude (the press) swept 0 → 1 — order to chaos", "drive", os.path.join(d, "duffing_drive.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
