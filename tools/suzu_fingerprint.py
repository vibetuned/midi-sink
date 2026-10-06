#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "matplotlib", "scipy"]
# ///
"""suzu_fingerprint.py — measure instrument samples from SFZ / WAV collections
into a JSON fingerprint and a 3-panel sound profile figure.

Part of fitting Suzu patches to VCSL instruments (Phase 10, HANDOFF_SUZU_VCSL.md).
Extracts:
  - per note f0, partial ratios r_k and strike weights w_k
  - inharmonicity coefficient B (string dispersion)
  - per-partial T60 decay (log-envelope slope)
  - attack time and noise floor
  - sustained spectrum across velocity layers
  - aggregate modal parameters (decay_s, decay_bright, pluck position)

Usage:
  uv run tools/suzu_fingerprint.py --sfz <path_or_url> [--samples-dir <dir>]
                                  [--fetch] [--out <fingerprint.json>]
                                  [--plot <fingerprint.png>] [--title <title>]
"""
import argparse
import json
import math
import os
import re
import sys
import urllib.parse
import urllib.request
import ssl

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from scipy import signal
from scipy.io import wavfile


import xml.etree.ElementTree as ET


def parse_sfz(text):
    """Parse SFZ subset: groups and regions with opcode assignments."""
    group, regions, cur = {}, [], None
    for line in text.splitlines():
        line = line.split("//")[0].strip()
        if not line:
            continue
        if line == "<group>":
            cur = group
            continue
        if line == "<region>":
            cur = dict(group)
            regions.append(cur)
            continue
        m = re.match(r"([a-z_0-9]+)=(.*)", line)
        if m and cur is not None:
            cur[m.group(1)] = m.group(2).strip()
    return regions


def parse_dspreset(text):
    """Parse DecentSampler .dspreset XML format into regions."""
    regions = []
    try:
        root = ET.fromstring(text)
    except Exception as e:
        print(f"Error parsing XML: {e}", file=sys.stderr)
        return []
    for s in root.iter("sample"):
        path = s.attrib.get("path", "")
        root_note = s.attrib.get("rootNote", "60")
        r = {
            "sample": path,
            "pitch_keycenter": root_note,
            "lokey": s.attrib.get("loNote", root_note),
            "hikey": s.attrib.get("hiNote", root_note),
            "lovel": s.attrib.get("loVel", "0"),
            "hivel": s.attrib.get("hiVel", "127"),
            "offset": s.attrib.get("start", "0")
        }
        regions.append(r)
    return regions


def read_audio(path):
    """Read WAV file into normalized float64 mono array and sample rate."""
    rate, data = wavfile.read(path)
    if data.dtype.kind == "i":
        data = data.astype(np.float64) / float(2 ** (8 * data.dtype.itemsize - 1))
    elif data.dtype.kind == "u":
        data = (data.astype(np.float64) - 128.0) / 128.0
    data = np.asarray(data, dtype=np.float64)
    if data.ndim == 2:
        data = np.mean(data, axis=1)
    return rate, data


def estimate_f0(x, rate, nominal_hz):
    """Estimate fundamental f0 near nominal_hz via parabolic peak interpolation on Hann STFT."""
    n_fft = min(8192, 1 << int(math.ceil(math.log2(len(x)))))
    if n_fft < 2048:
        n_fft = 2048
    win = np.hanning(min(n_fft, len(x)))
    segment = x[:len(win)] * win
    spec = np.abs(np.fft.rfft(segment, n=n_fft))
    freqs = np.fft.rfftfreq(n_fft, d=1.0 / rate)

    # Search window around nominal_hz (+- 150 cents)
    lo_hz = nominal_hz * (2.0 ** (-150.0 / 1200.0))
    hi_hz = nominal_hz * (2.0 ** (150.0 / 1200.0))
    mask = (freqs >= lo_hz) & (freqs <= hi_hz)
    indices = np.where(mask)[0]
    if len(indices) == 0:
        return nominal_hz

    peak_idx = indices[np.argmax(spec[indices])]
    if 0 < peak_idx < len(spec) - 1:
        alpha = spec[peak_idx - 1]
        beta = spec[peak_idx]
        gamma = spec[peak_idx + 1]
        denom = alpha - 2 * beta + gamma
        delta = 0.5 * (alpha - gamma) / denom if denom != 0 else 0.0
        f0 = (peak_idx + delta) * (rate / n_fft)
    else:
        f0 = freqs[peak_idx]
    return f0 if f0 > 0 else nominal_hz


def extract_partials(x, rate, f0, max_partials=16):
    """Extract partial frequencies, relative ratios, strike weights, and T60 decays."""
    n_fft = 8192 if len(x) >= 8192 else (1 << int(math.floor(math.log2(len(x)))))
    if n_fft < 1024:
        return []

    # Strike window: initial 50-150 ms
    strike_len = int(min(len(x), rate * 0.15))
    strike_win = np.hanning(strike_len)
    strike_spec = np.abs(np.fft.rfft(x[:strike_len] * strike_win, n=n_fft))
    freqs = np.fft.rfftfreq(n_fft, d=1.0 / rate)

    # Global peak in strike
    max_mag = np.max(strike_spec) if len(strike_spec) > 0 else 1.0
    if max_mag <= 0:
        max_mag = 1.0

    # STFT for time-tracking
    hop = n_fft // 4
    f_stft, t_stft, Zxx = signal.stft(x, fs=rate, nperseg=min(2048, len(x)), noverlap=min(1536, len(x) * 3 // 4))
    mag_stft = np.abs(Zxx)

    partials = []
    # Track harmonic or inharmonic peaks up to Nyquist
    nyquist = rate * 0.48
    for k in range(1, max_partials + 1):
        target_f = f0 * k
        if target_f >= nyquist:
            break
        # Search band +- 35 % of fundamental or harmonic separation
        band = max(f0 * 0.35, 20.0)
        idx_band = np.where((freqs >= target_f - band) & (freqs <= target_f + band))[0]
        if len(idx_band) == 0:
            continue

        p_idx = idx_band[np.argmax(strike_spec[idx_band])]
        if 0 < p_idx < len(strike_spec) - 1:
            a = strike_spec[p_idx - 1]
            b = strike_spec[p_idx]
            c = strike_spec[p_idx + 1]
            denom = a - 2 * b + c
            delta = 0.5 * (a - c) / denom if denom != 0 else 0.0
            actual_f = (p_idx + delta) * (rate / n_fft)
            mag = b - 0.25 * (a - c) * delta
        else:
            actual_f = freqs[p_idx]
            mag = strike_spec[p_idx]

        ratio = actual_f / f0
        weight = mag / max_mag

        # Estimate T60 for this partial from STFT time track
        # Find closest STFT frequency bin
        bin_idx = np.argmin(np.abs(f_stft - actual_f))
        time_env = mag_stft[bin_idx, :]
        env_peak = np.max(time_env) if len(time_env) > 0 else 1e-9
        if env_peak <= 1e-9:
            t60 = 0.0
        else:
            env_db = 20.0 * np.log10(np.maximum(time_env / env_peak, 1e-6))
            # Fit slope in window from peak down to -30 dB or before floor
            peak_t_idx = np.argmax(time_env)
            valid = np.where((t_stft >= t_stft[peak_t_idx]) & (env_db >= -30.0))[0]
            if len(valid) >= 4 and (t_stft[valid[-1]] - t_stft[valid[0]]) > 0.05:
                dt = t_stft[valid] - t_stft[valid[0]]
                slope, _ = np.polyfit(dt, env_db[valid], 1)
                t60 = float(-60.0 / slope) if slope < -0.1 else 30.0
            else:
                t60 = 3.0

        partials.append({
            "k": k,
            "freq_hz": float(actual_f),
            "ratio": float(ratio),
            "weight": float(weight),
            "t60_s": float(max(0.01, min(60.0, t60)))
        })

    return partials


def fit_inharmonicity(partials):
    """Fit string inharmonicity B where r_k ~ k * sqrt(1 + B * k^2)."""
    if len(partials) < 4:
        return 0.0
    ks = np.array([p["k"] for p in partials], dtype=np.float64)
    ratios = np.array([p["ratio"] for p in partials], dtype=np.float64)
    # (r_k / k)^2 - 1 = B * k^2
    y = (ratios / ks) ** 2 - 1.0
    x = ks ** 2
    # Constrain B >= 0
    b_val = float(np.sum(x * y) / np.sum(x * x))
    return max(0.0, b_val)


def measure_note(x, rate, nominal_f0):
    """Measure single note: attack, f0, partials, B, overall T60, noise floor."""
    if len(x) == 0:
        return None
    peak_val = np.max(np.abs(x))
    if peak_val <= 1e-9:
        return None

    # Attack time
    peak_idx = np.argmax(np.abs(x))
    thresh = peak_val * 0.1
    onset_idx = np.where(np.abs(x[:peak_idx]) >= thresh)[0]
    start_idx = onset_idx[0] if len(onset_idx) > 0 else 0
    attack_s = (peak_idx - start_idx) / rate

    # Fundamental f0
    f0 = estimate_f0(x, rate, nominal_f0)
    cents_detune = 1200.0 * math.log2(f0 / nominal_f0) if nominal_f0 > 0 else 0.0

    # Partials
    partials = extract_partials(x, rate, f0)

    # Inharmonicity B
    inharmonicity_b = fit_inharmonicity(partials)

    # Overall T60 (energy envelope)
    frame_sz = 512
    frames = [np.mean(x[i:i+frame_sz] ** 2) for i in range(0, len(x) - frame_sz, frame_sz // 2)]
    frames = np.sqrt(np.maximum(frames, 1e-12))
    if len(frames) > 4:
        max_f = np.max(frames)
        f_db = 20.0 * np.log10(np.maximum(frames / max_f, 1e-6))
        p_idx = np.argmax(frames)
        t_axis = np.arange(len(frames)) * (frame_sz / 2.0 / rate)
        decay_pts = np.where((t_axis >= t_axis[p_idx]) & (f_db >= -30.0))[0]
        if len(decay_pts) >= 4 and (t_axis[decay_pts[-1]] - t_axis[decay_pts[0]]) > 0.05:
            dt = t_axis[decay_pts] - t_axis[decay_pts[0]]
            slope, _ = np.polyfit(dt, f_db[decay_pts], 1)
            t60_overall = float(-60.0 / slope) if slope < -0.1 else 30.0
        else:
            t60_overall = 3.0
    else:
        t60_overall = 3.0

    # Noise floor (last 100 ms)
    tail_len = int(min(len(x), rate * 0.1))
    noise_rms = np.sqrt(np.mean(x[-tail_len:] ** 2)) if tail_len > 0 else 1e-6
    noise_floor_db = 20.0 * math.log10(max(noise_rms, 1e-6))

    # Steady state spectrum (harmonic levels h1..h8 in dB)
    h_levels = []
    for h in range(1, 9):
        found = next((p["weight"] for p in partials if p["k"] == h), 1e-6)
        h_levels.append(float(20.0 * math.log10(max(found, 1e-6))))

    return {
        "f0": float(f0),
        "f0_nominal": float(nominal_f0),
        "cents_detune": float(cents_detune),
        "attack_s": float(max(0.001, attack_s)),
        "t60_s": float(max(0.01, min(60.0, t60_overall))),
        "noise_floor_db": float(noise_floor_db),
        "inharmonicity_b": float(inharmonicity_b),
        "peak_dbfs": float(20.0 * math.log10(max(peak_val, 1e-9))),
        "partials": partials,
        "h_levels_db": h_levels
    }


def fetch_url(url, dest):
    """Fetch URL to destination file."""
    os.makedirs(os.path.dirname(os.path.abspath(dest)), exist_ok=True)
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return
    ctx = ssl._create_unverified_context()
    req = urllib.request.Request(url, headers={"User-Agent": "midi-sink-fingerprint/1.0"})
    with urllib.request.urlopen(req, context=ctx) as r, open(dest, "wb") as f:
        f.write(r.read())


def main():
    ap = argparse.ArgumentParser(description="Extract acoustic fingerprint and profile from instrument samples")
    ap.add_argument("--sfz", default=None, help="SFZ file path or URL")
    ap.add_argument("--preset", default=None, help="DecentSampler .dspreset file path")
    ap.add_argument("--samples-dir", default=None, help="Local directory for samples (cached/fetched)")
    ap.add_argument("--fetch", action="store_true", help="Download missing samples from VCSL repository")
    ap.add_argument("--base-url", default=None, help="Base URL for remote samples if fetching")
    ap.add_argument("--out", default=None, help="Output JSON fingerprint path")
    ap.add_argument("--plot", default=None, help="Output PNG profile figure path")
    ap.add_argument("--title", default=None, help="Plot title")
    args = ap.parse_args()

    input_path = args.sfz or args.preset
    if not input_path:
        print("Error: Specify either --sfz or --preset", file=sys.stderr)
        return 1

    is_dspreset = input_path.endswith(".dspreset") or (args.preset is not None)
    is_remote = input_path.startswith("http://") or input_path.startswith("https://")
    input_name = os.path.basename(urllib.parse.urlparse(input_path).path if is_remote else input_path)
    inst_name = os.path.splitext(input_name)[0]

    samples_dir = args.samples_dir
    if not samples_dir:
        if not is_remote and os.path.exists(os.path.join(os.path.dirname(os.path.abspath(input_path)), "Samples")):
            samples_dir = os.path.join(os.path.dirname(os.path.abspath(input_path)), "Samples")
        else:
            samples_dir = os.path.expanduser(f"~/Music/midi-sink/VCSL/{inst_name}/Samples")
    os.makedirs(samples_dir, exist_ok=True)

    if is_remote:
        local_input = os.path.join(samples_dir, "..", input_name)
        print(f"Fetching remote input: {input_path} -> {local_input}")
        fetch_url(input_path, local_input)
        text = open(local_input, "r", encoding="utf-8", errors="ignore").read()
        base_url = args.base_url or input_path.rsplit("/", 1)[0] + "/"
    else:
        text = open(input_path, "r", encoding="utf-8", errors="ignore").read()
        base_url = args.base_url

    regions = parse_dspreset(text) if is_dspreset else parse_sfz(text)
    if not regions:
        print(f"Error: No regions parsed from {input_path}", file=sys.stderr)
        return 1

    print(f"Parsed {len(regions)} regions from {input_name}")

    # Process regions by note
    note_results = {}
    processed_files = set()

    # Prioritize loudest / primary layer (e.g. lovel >= 80 or highest velocity)
    # Group by keycenter
    by_note = {}
    for r in regions:
        kc = int(r.get("pitch_keycenter", r.get("lokey", 60)))
        by_note.setdefault(kc, []).append(r)

    notes_sorted = sorted(by_note.keys())
    print(f"Notes covered: {len(notes_sorted)} ({min(notes_sorted)}..{max(notes_sorted)})")

    for midi_note in notes_sorted:
        cands = by_note[midi_note]
        # Choose candidate with highest hivel
        cands.sort(key=lambda c: int(c.get("hivel", 127)), reverse=True)
        chosen = cands[0]
        sample_rel = chosen.get("sample", "")
        if not sample_rel:
            continue
        sample_file = os.path.basename(sample_rel)
        local_wav = os.path.join(samples_dir, sample_file)

        if not os.path.exists(local_wav) or os.path.getsize(local_wav) == 0:
            if args.fetch and base_url:
                # Fetch sample
                sample_url = urllib.parse.urljoin(base_url, urllib.parse.quote(sample_rel))
                print(f"  Downloading [{midi_note}] {sample_file}...")
                fetch_url(sample_url, local_wav)
            else:
                # Check directly relative to SFZ
                alt_path = os.path.join(os.path.dirname(args.sfz), sample_rel) if not is_remote_sfz else None
                if alt_path and os.path.exists(alt_path):
                    local_wav = alt_path
                else:
                    print(f"  Missing sample for note {midi_note}: {sample_file} (use --fetch to download)")
                    continue

        rate, data = read_audio(local_wav)
        offset = int(chosen.get("offset", 0))
        if offset > 0 and offset < len(data):
            data = data[offset:]

        nom_hz = 440.0 * (2.0 ** ((midi_note - 69) / 12.0))
        res = measure_note(data, rate, nom_hz)
        if res:
            res["sample"] = sample_file
            note_results[str(midi_note)] = res

    if not note_results:
        print("Error: No note samples could be measured.", file=sys.stderr)
        return 1

    # Aggregate statistics
    all_b = [r["inharmonicity_b"] for r in note_results.values() if r["inharmonicity_b"] > 0]
    mean_b = float(np.mean(all_b)) if all_b else 0.0

    all_t60 = [r["t60_s"] for r in note_results.values()]
    mean_t60 = float(np.median(all_t60)) if all_t60 else 3.0

    all_attack = [r["attack_s"] for r in note_results.values()]
    mean_attack = float(np.median(all_attack)) if all_attack else 0.005

    # Collect partial ratios and weights across notes
    ratios_by_k = {}
    weights_by_k = {}
    t60_by_k = {}
    for r in note_results.values():
        for p in r["partials"]:
            k = p["k"]
            ratios_by_k.setdefault(k, []).append(p["ratio"])
            weights_by_k.setdefault(k, []).append(p["weight"])
            t60_by_k.setdefault(k, []).append(p["t60_s"])

    max_k = max(ratios_by_k.keys()) if ratios_by_k else 1
    mean_ratios = [float(np.median(ratios_by_k.get(k, [float(k)]))) for k in range(1, max_k + 1)]
    mean_weights = [float(np.median(weights_by_k.get(k, [0.0]))) for k in range(1, max_k + 1)]
    # Normalize strike weights
    max_w = max(mean_weights) if mean_weights else 1.0
    if max_w > 0:
        mean_weights = [w / max_w for w in mean_weights]

    # Fit decay brightness beta: gamma_k = alpha + beta * (r_k^2 - 1)
    alpha = 6.907755 / max(0.1, mean_t60)
    beta = 0.0
    if len(mean_ratios) >= 3:
        gammas = [6.907755 / max(0.05, float(np.median(t60_by_k.get(k, [mean_t60])))) for k in range(1, len(mean_ratios) + 1)]
        x_rk = np.array([r * r - 1.0 for r in mean_ratios], dtype=np.float64)
        y_gm = np.array([g - alpha for g in gammas], dtype=np.float64)
        if np.sum(x_rk ** 2) > 1e-6:
            beta = float(max(0.0, np.sum(x_rk * y_gm) / np.sum(x_rk ** 2)))

    # Fit pluck position p for modal plucked string (sin(k*pi*p)/k^2)
    best_p = 0.28
    best_err = 1e9
    for p_trial in np.linspace(0.05, 0.5, 91):
        err = 0.0
        for k in range(1, min(len(mean_weights) + 1, 9)):
            target = abs(math.sin(k * math.pi * p_trial)) / (k * k)
            err += (mean_weights[k - 1] - target) ** 2
        if err < best_err:
            best_err = err
            best_p = float(p_trial)

    fingerprint = {
        "instrument": inst_name,
        "notes_measured": len(note_results),
        "mean_attack_s": mean_attack,
        "mean_t60_s": mean_t60,
        "mean_inharmonicity_b": mean_b,
        "decay_bright": beta,
        "pluck_fit": best_p,
        "mean_ratios": mean_ratios,
        "mean_weights": mean_weights,
        "notes": note_results
    }

    out_json = args.out or f"docs/evidence/step67/suzu_vcsl/{inst_name}_fingerprint.json"
    os.makedirs(os.path.dirname(os.path.abspath(out_json)), exist_ok=True)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(fingerprint, f, indent=2)
    print(f"Fingerprint written: {out_json}")

    # Plot profile figure if requested
    if args.plot:
        plot_path = args.plot
        os.makedirs(os.path.dirname(os.path.abspath(plot_path)), exist_ok=True)
        notes = np.array([int(k) for k in note_results.keys()])
        order = np.argsort(notes)
        notes = notes[order]
        peaks = np.array([note_results[str(n)]["peak_dbfs"] for n in notes])
        t60s = np.array([note_results[str(n)]["t60_s"] for n in notes])
        h1 = np.array([note_results[str(n)]["h_levels_db"][0] for n in notes])
        h2 = np.array([note_results[str(n)]["h_levels_db"][1] for n in notes])
        h3 = np.array([note_results[str(n)]["h_levels_db"][2] for n in notes])

        fig, axes = plt.subplots(3, 1, figsize=(12, 11), constrained_layout=True)
        title = args.title or f"Sound profile — {inst_name} (VCSL)"
        fig.suptitle(title, fontsize=14)

        ax = axes[0]
        ax.plot(notes, peaks, label="peak level", color="#1f77b4", lw=1.6)
        ax.set_xlabel("MIDI note")
        ax.set_ylabel("peak, dBFS")
        ax.grid(True, alpha=0.3)
        ax.legend(loc="lower left")
        ax.set_title(f"Level across keyboard — peak spread {peaks.max() - peaks.min():.2f} dB, mean attack {mean_attack*1000:.1f} ms", fontsize=10)
        for oct in range(24, 109, 12):
            ax.axvline(oct, color="k", alpha=0.08)

        ax = axes[1]
        ax.plot(notes, h1, label="partial 1 (fundamental)", color="#2ca02c", lw=1.4)
        ax.plot(notes, h2, label="partial 2", color="#d62728", lw=1.4)
        ax.plot(notes, h3, label="partial 3", color="#9467bd", lw=1.4)
        ax.set_ylim(-90, 0)
        ax.set_xlabel("MIDI note")
        ax.set_ylabel("relative level, dBFS")
        ax.grid(True, alpha=0.3)
        ax.legend(loc="lower left")
        ax.set_title(f"First three partials (strike spectrum) — inharmonicity B = {mean_b:.6f}", fontsize=10)

        ax = axes[2]
        ax.plot(notes, t60s, label="T60 decay", color="#ff7f0e", lw=1.6)
        ax.set_xlabel("MIDI note")
        ax.set_ylabel("T60, seconds")
        ax.grid(True, alpha=0.3)
        ax.legend(loc="upper right")
        ax.set_title(f"Decay time T60 — median {mean_t60:.2f} s, fitted decay_bright beta = {beta:.3f}", fontsize=10)

        fig.savefig(plot_path, dpi=110)
        plt.close(fig)
        print(f"Profile figure written: {plot_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
