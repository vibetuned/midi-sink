#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy"]
# ///
"""The glide's offline spectral check (Phase 7 step 49, SOUND §2 — the reason
for 4-point Hermite interpolation, recorded in DECISIONS_6 #10).

  uv run tools/voxo_glide_check.py --dir <bounce dir> [--pass-db -55] [--margin-db 10]

`midi-sink --dev --voxo-bounce <dir>` writes, through Voxo's real renderer
and once per interpolation: the ±48-semitone sweep (glide_hermite.wav,
glide_linear.wav), two static holds at -45 and +45 semitones
(static_{down,up}_{hermite,linear}.wav), and glide.json with the trajectory
(root, semitone range, duration, hold, the sample's partials, its rate). The
sample is six partials at 1/k rooted at A3, recorded at 12 kHz so its top
partial sits at 0.22 of its own Nyquist — where an acoustic sample keeps its
strong partials. This script frames each file (4096, Blackman-Harris, hop
1024), predicts where the partials sit at the frame's time, and splits every
frame's spectrum into the partials' bins (signal) and everything else
(artefacts: the interpolation's images). It reports the worst and median
artefact-to-signal ratio per file. The static holds are the interpolation's
floor alone; the sweep is judged strictly between its corners.

  PASS when, for each of the three cases, Hermite's worst ratio is at or
  below --pass-db AND linear is worse by at least --margin-db (the
  side-by-side: the comparison is the control).
"""
import argparse, json, os, struct, sys
import numpy as np


def read_wav(path):
    """RIFF walk (the wave module refuses IEEE float): PCM 16 or float 32, channel 0."""
    b = open(path, "rb").read()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise SystemExit(f"{path}: not a WAV")
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(b):
        cid, csize = b[pos:pos + 4], struct.unpack_from("<I", b, pos + 4)[0]
        body = b[pos + 8:pos + 8 + csize]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body, 0)
        elif cid == b"data":
            data = body
            break
        pos += 8 + csize + (csize & 1)
    if fmt is None or data is None:
        raise SystemExit(f"{path}: no fmt/data chunk")
    tag, ch, sr, _, _, bits = fmt
    if tag == 3 and bits == 32:
        x = np.frombuffer(data, dtype="<f4")
    elif tag == 1 and bits == 16:
        x = np.frombuffer(data, dtype="<i2") / 32768.0
    else:
        raise SystemExit(f"{path}: unsupported format tag {tag} / {bits} bits")
    return x.reshape(-1, ch)[:, 0].astype(np.float64), sr


def analyse(path, traj, n=4096, hop=1024, fixed_semis=None):
    x, sr = read_wav(path)
    x = x[int(0.3 * sr):]   # the attack and the first blocks settle
    # 4-term Blackman-Harris: sidelobes at -92 dB, so the window's own leakage
    # sits far below the interpolation images this check is after (a Hann
    # window's -31 dB sidelobes would set the floor at ~-37 dB by themselves).
    i = np.arange(n)
    win = (0.35875 - 0.48829 * np.cos(2 * np.pi * i / n) + 0.14128 * np.cos(4 * np.pi * i / n)
           - 0.01168 * np.cos(6 * np.pi * i / n))
    root_hz = 440.0 * 2 ** ((traj["root_note"] - 69) / 12)
    partials, dur, hold = traj["partials"], traj["duration_s"], traj.get("hold_s", 0.0)
    s0, s1 = traj["semitones_from"], traj["semitones_to"]
    def semis_at(tc):
        if fixed_semis is not None:
            return fixed_semis
        tc += 0.3
        u = 0.0 if tc < hold else min(1.0, max(0.0, (tc - hold) / dur))
        return s0 + (s1 - s0) * u
    rows = []
    for t in range(0, len(x) - n, hop):
        tc = (t + n / 2) / sr
        if fixed_semis is None:
            # The sweep's corners — where the chirp starts and stops — splatter
            # for a frame on either side whatever the interpolation (measured:
            # -40 dB for both at the top corner); the static holds judge the
            # endpoints, the sweep is judged strictly between its corners.
            if not (hold + n / sr < tc + 0.3 < hold + dur - n / sr):
                continue
        semis = semis_at(tc)
        # The partials sweep across the frame: the signal window spans each
        # partial's excursion from the frame's start to its end, plus the
        # Hann main lobe — the glide's own smear is signal, not artefact.
        fa, fb = root_hz * 2 ** (semis_at(t / sr) / 12), root_hz * 2 ** (semis_at((t + n) / sr) / 12)
        mag = np.abs(np.fft.rfft(x[t:t + n] * win)) ** 2
        total = mag.sum()
        if total < 1e-9:
            continue
        sig = np.zeros(len(mag), dtype=bool)
        for k in range(1, partials + 1):
            lo, hi = min(fa, fb) * k, max(fa, fb) * k
            if lo >= sr / 2 * 0.98:
                break
            b0, b1 = int(lo * n / sr) - 5, int(hi * n / sr) + 6   # the Blackman-Harris main lobe is ±4 bins
            sig[max(0, b0):min(len(mag), b1)] = True
        sig[:6] = True                         # DC, not an artefact
        signal = mag[sig].sum()
        artefact = total - signal
        if signal > 0:
            rows.append((tc, semis, 10 * np.log10(max(artefact, 1e-30) / signal)))
    r = np.array(rows)
    worst = r[np.argmax(r[:, 2])]
    return {"frames": len(r), "worst_db": float(worst[2]), "worst_t": float(worst[0]), "worst_semis": float(worst[1]),
            "median_db": float(np.median(r[:, 2])),
            "down_worst_db": float(r[r[:, 1] < 0, 2].max()) if (r[:, 1] < 0).any() else float("nan"),
            "up_worst_db": float(r[r[:, 1] >= 0, 2].max()) if (r[:, 1] >= 0).any() else float("nan")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--pass-db", type=float, default=-55.0)
    ap.add_argument("--margin-db", type=float, default=10.0)
    a = ap.parse_args()
    traj = json.load(open(os.path.join(a.dir, "glide.json")))
    verdicts = []
    # The static holds first: the interpolation's own image floor, no sweep.
    for tag, semis in (("down", -45.0), ("up", 45.0)):
        r = {}
        for name in ("hermite", "linear"):
            r[name] = analyse(os.path.join(a.dir, f"static_{tag}_{name}.wav"), traj, fixed_semis=semis)
            print(f"static {tag:4s} {semis:+.0f} st {name:8s}: artefact/signal worst {r[name]['worst_db']:.1f} dB, median {r[name]['median_db']:.1f} dB")
        verdicts.append((f"static {tag}", r["hermite"]["worst_db"], r["linear"]["worst_db"]))
    res = {}
    for name in ("hermite", "linear"):
        res[name] = r = analyse(os.path.join(a.dir, f"glide_{name}.wav"), traj)
        print(f"sweep {name:8s}: {r['frames']} frames; artefact/signal worst {r['worst_db']:.1f} dB at t={r['worst_t'] + 0.3:.2f} s "
              f"({r['worst_semis']:+.1f} st), median {r['median_db']:.1f} dB; down-glide worst {r['down_worst_db']:.1f}, up-glide worst {r['up_worst_db']:.1f}")
    verdicts.append(("sweep", res["hermite"]["worst_db"], res["linear"]["worst_db"]))
    ok = True
    for what, h, l in verdicts:
        ok_h = h <= a.pass_db
        ok_m = l - h >= a.margin_db
        ok = ok and ok_h and ok_m
        print(f"{what:12s}: Hermite worst {h:.1f} dB {'<=' if ok_h else '>'} {a.pass_db:.0f} dB; linear {l - h:.1f} dB worse "
              f"({'>=' if ok_m else '<'} the {a.margin_db:.0f} dB margin)")
    print("glide check PASS" if ok else "glide check FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
