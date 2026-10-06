#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy", "scipy"]
# ///
"""suzu_fit.py — fit Suzu synth patches to instrument acoustic fingerprints.

Part of fitting Suzu patches to VCSL instruments (Phase 10, HANDOFF_SUZU_VCSL.md).
Reads a JSON fingerprint produced by suzu_fingerprint.py and generates a fitted
Suzu patch block inside a preset JSON conforming to presets/SCHEMA.md.

Supported voice kinds:
  - modal (voice_kind 1):
      preset 0: harmonic string
      preset 1: stiff bar (glockenspiel, vibraphone, marimba, xylophone)
      preset 2: bell (tubular bells, hand bells)
      preset 3: glass (crotales, glass)
      preset 4: plucked string (harp, guitars, zither / Dan Tranh)
  - bow (voice_kind 1 with bow_position / bow_onset_s):
      bowed sustains (violin, viola, cello)
  - flute (voice_kind 6): edge-blown jet on an acoustic bore
  - sax (voice_kind 7): inward reed on a conical bore
  - trumpet (voice_kind 8): outward lips on a flared bore

Usage:
  uv run tools/suzu_fit.py --fingerprint <fingerprint.json>
                          [--voice-kind <name_or_int>]
                          [--modal-preset <0..4>]
                          [--preset-name <name>]
                          [--out <preset.json>]
                          [--print-c]
"""
import argparse
import json
import math
import os
import sys

import numpy as np


VOICE_KINDS = {
    "cell": 0,
    "modal": 1,
    "lattice": 1,
    "verlet": 2,
    "hybrid": 3,
    "duffing": 4,
    "rotor": 5,
    "flute": 6,
    "sax": 7,
    "saxophone": 7,
    "trumpet": 8,
    "brass": 8
}

MODAL_PRESETS = {
    "harmonic": 0,
    "bar": 1,
    "stiff_bar": 1,
    "bell": 2,
    "tubular_bell": 2,
    "glass": 3,
    "crotales": 3,
    "plucked": 4,
    "plucked_string": 4,
    "harp": 4,
    "guitar": 4,
    "zither": 4
}


def fit_modal_patch(fp, modal_preset=None, is_bowed=False):
    """Fit parameters for voice_kind 1 (modal lattice)."""
    # 1. Determine modal preset if not specified
    mean_ratios = fp.get("mean_ratios", [1.0])
    mean_weights = fp.get("mean_weights", [1.0])
    mean_t60 = fp.get("mean_t60_s", 3.0)
    mean_b = fp.get("mean_inharmonicity_b", 0.0)
    decay_bright = fp.get("decay_bright", 0.3)
    pluck_fit = fp.get("pluck_fit", 0.28)
    attack_s = fp.get("mean_attack_s", 0.005)

    if modal_preset is None:
        inst_lower = fp.get("instrument", "").lower()
        if "bell" in inst_lower or "chime" in inst_lower:
            modal_preset = 2  # BELL
        elif any(k in inst_lower for k in ["glock", "marimba", "xylo", "vibe", "bar"]):
            modal_preset = 1  # BAR
        elif any(k in inst_lower for k in ["glass", "crotale"]):
            modal_preset = 3  # GLASS
        elif any(k in inst_lower for k in ["harp", "guitar", "tranh", "zither", "pluck"]):
            modal_preset = 4  # PLUCKED
        elif is_bowed:
            modal_preset = 0  # HARMONIC BOWED
        else:
            modal_preset = 0  # HARMONIC

    # Number of modes: count partials with weight >= 0.03 (approx -30 dB)
    modes = 1
    for i, w in enumerate(mean_weights[:16]):
        if w >= 0.03:
            modes = i + 1
    modes = max(3, min(16, modes))

    # Stiffness (inharmonicity B)
    stiffness = max(0.0, min(0.05, float(mean_b)))

    # Decay T60
    decay_s = max(0.2, min(30.0, float(mean_t60)))

    # High frequency decay brightness beta
    decay_b = max(0.0, min(2.0, float(decay_bright)))

    # Pluck position
    pluck = max(0.05, min(0.5, float(pluck_fit)))

    # Bow parameters
    if is_bowed:
        bow_onset_s = max(0.02, min(0.5, float(attack_s * 2.0)))
        # Bow position from relative even/odd harmonic balance
        h2 = mean_weights[1] if len(mean_weights) > 1 else 0.5
        h3 = mean_weights[2] if len(mean_weights) > 2 else 0.3
        bow_position = 0.25 if h2 > h3 else 0.35
    else:
        bow_onset_s = 0.0
        bow_position = 0.3

    # Output level and release
    level = 0.28
    release_s = max(0.1, min(1.5, decay_s * 0.3))

    patch = {
        "source": 1,
        "voice_kind": 1,
        "modal_preset": int(modal_preset),
        "modes": int(modes),
        "coupling": 0.05,
        "decay_s": float(round(decay_s, 3)),
        "decay_bright": float(round(decay_b, 4)),
        "stiffness": float(round(stiffness, 6)),
        "pluck": float(round(pluck, 3)),
        "bow_onset_s": float(round(bow_onset_s, 3)),
        "bow_position": float(round(bow_position, 3)),
        "breath_cc": 2,
        "level": float(level),
        "attack_s": float(round(attack_s, 4)),
        "release_s": float(round(release_s, 3)),
        "cutoff_hz": 20000.0,
        "resonance": 0.0,
        "shear": 0.0,
        "shear_kind": 0
    }
    return patch


def fit_wind_patch(fp, voice_kind):
    """Fit parameters for acoustic bore winds (flute 6, sax 7, trumpet 8)."""
    mean_weights = fp.get("mean_weights", [1.0])
    mean_attack = fp.get("mean_attack_s", 0.03)
    mean_t60 = fp.get("mean_t60_s", 1.0)

    # General wind defaults
    patch = {
        "source": 1,
        "voice_kind": int(voice_kind),
        "level": 0.25,
        "attack_s": float(round(max(0.005, min(0.1, mean_attack)), 3)),
        "release_s": 0.25,
        "cutoff_hz": 20000.0,
        "resonance": 0.0,
        "press_blows": 1
    }

    if voice_kind == 6:
        # Flute (open-open cylinder with labium jet)
        # Even harmonic strength relative to fundamental determines jet_offset
        h1 = mean_weights[0] if len(mean_weights) > 0 else 1.0
        h2 = mean_weights[1] if len(mean_weights) > 1 else 0.2
        even_ratio = min(1.0, h2 / max(1e-6, h1))
        jet_offset = 0.15 + 0.35 * even_ratio  # 0.15..0.50

        patch.update({
            "bore_nodes": 128,
            "bore_loss": 0.28,
            "bore_corner_hz": 1600.0,
            "jet_gain": 560.0,
            "jet_drive": 1.0,
            "jet_tau": 0.5,
            "jet_q": 1.0,
            "jet_noise": 0.025,
            "jet_area": 0.05,
            "jet_offset": float(round(jet_offset, 3)),
            "breath_ref": 0.44,
            "breath_range": 12.0,
            "bore_wall_s": float(round(max(0.5, min(2.0, mean_t60)), 2))
        })

    elif voice_kind == 7:
        # Saxophone (inward reed on a cone)
        patch.update({
            "reed_hz": 11500.0,
            "reed_q": 0.72,
            "reed_open": 0.50,
            "reed_close": 3.0,
            "reed_area": 0.14,
            "reed_noise": 0.02,
            "cone_apex": 0.25
        })

    elif voice_kind == 8:
        # Trumpet (outward lips on a flared bore)
        patch.update({
            "lip_ratio": 0.95,
            "lip_q": 3.0,
            "lip_open": 0.05,
            "lip_close": 1.0,
            "lip_area": 0.5,
            "lip_range": 1.0,
            "partial": 3,
            "bell_start": 0.60,
            "bell_gamma": 0.70,
            "brass": 0.50
        })

    return patch


def build_full_preset(preset_name, suzu_patch):
    """Build a complete preset object matching presets/SCHEMA.md."""
    vk = suzu_patch.get("voice_kind", 1)
    # Wind instruments use wind input mode 3, others MPE mode 1
    input_mode = 3 if vk in (6, 7, 8) else 1

    preset = {
        "midi_sink_preset": 1,
        "sumi_version": [1, 5, 0],
        "name": preset_name,
        "input_mode": input_mode,
        "params": {
            "fluid_viscosity": 0.5,
            "expansion_rate": 0.5,
            "paper_roughness": 0.2,
            "active_palette_id": 1,
            "medium": 0,
            "pitch_layout": 0
        },
        "suzu": suzu_patch
    }
    return preset


def main():
    ap = argparse.ArgumentParser(description="Fit Suzu patch to acoustic fingerprint")
    ap.add_argument("--fingerprint", required=True, help="Input fingerprint JSON from suzu_fingerprint.py")
    ap.add_argument("--voice-kind", default=None, help="Voice kind name (modal, flute, sax, trumpet) or integer 0..8")
    ap.add_argument("--modal-preset", default=None, help="Modal preset name (harmonic, bar, bell, glass, plucked) or int 0..4")
    ap.add_argument("--preset-name", default=None, help="Preset name string")
    ap.add_argument("--bowed", action="store_true", help="Fit as bowed string sustain")
    ap.add_argument("--out", default=None, help="Output preset JSON path")
    ap.add_argument("--print-c", action="store_true", help="Print C++ patch initialization code")
    args = ap.parse_args()

    with open(args.fingerprint, "r", encoding="utf-8") as f:
        fp = json.load(f)

    inst_name = fp.get("instrument", "Custom")

    # Determine voice kind
    vk = 1  # default modal
    if args.voice_kind is not None:
        if args.voice_kind.isdigit():
            vk = int(args.voice_kind)
        else:
            vk = VOICE_KINDS.get(args.voice_kind.lower(), 1)
    else:
        inst_lower = inst_name.lower()
        if any(w in inst_lower for w in ["flute", "recorder", "ocarina"]):
            vk = 6
        elif any(w in inst_lower for w in ["sax", "clarinet", "oboe", "reed"]):
            vk = 7
        elif any(w in inst_lower for w in ["trumpet", "brass", "horn", "trombone"]):
            vk = 8
        else:
            vk = 1

    # Determine modal preset
    mp = None
    if args.modal_preset is not None:
        if args.modal_preset.isdigit():
            mp = int(args.modal_preset)
        else:
            mp = MODAL_PRESETS.get(args.modal_preset.lower(), 0)

    # Fit patch
    if vk == 1:
        patch = fit_modal_patch(fp, modal_preset=mp, is_bowed=args.bowed)
    elif vk in (6, 7, 8):
        patch = fit_wind_patch(fp, voice_kind=vk)
    else:
        patch = fit_modal_patch(fp, modal_preset=0, is_bowed=False)
        patch["voice_kind"] = vk

    preset_name = args.preset_name or f"{inst_name} (fitted)"
    full_preset = build_full_preset(preset_name, patch)

    # Output JSON preset
    out_path = args.out or f"presets/{inst_name.lower().replace(' ', '_')}_fitted.json"
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(full_preset, f, indent=2)
    print(f"Preset written: {out_path}")

    # Print summary
    print("\nFitted Suzu patch:")
    print(json.dumps(patch, indent=2))

    if args.print_c:
        print("\nC++ voxo_suzu_params_t snippet:")
        print(f"// Fitted from VCSL {inst_name}")
        for k, v in patch.items():
            if isinstance(v, float):
                print(f"sp.{k} = {v}f;")
            elif isinstance(v, int):
                print(f"sp.{k} = {v};")

    return 0


if __name__ == "__main__":
    sys.exit(main())
