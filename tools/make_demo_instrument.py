#!/usr/bin/env python3
"""The demo instrument's PLACEHOLDER (Phase 7 step 52, SOUND §3's "one tiny
public-domain demo instrument ships so first launch makes a sound"): a
synthesised music box — an inharmonic bell-like tone with a fast decay —
three zones an octave apart, 22.05 kHz mono 16-bit, about 170 KB in all,
public domain by construction. It fills the slot (voxo/demo/) until the
author records the real one (`[ITERATE: a music-box or kalimba, on brand]`);
the preset beside it is what the shells load on a first launch.

  python3 tools/make_demo_instrument.py     # rewrites voxo/demo/
"""
import math, os, struct, wave

here = os.path.dirname(os.path.abspath(__file__))
out_dir = os.path.join(here, "..", "voxo", "demo")
rate = 22050

def tone(root_hz, seconds):
    n = int(seconds * rate)
    # A music-box tine: the fundamental and a few inharmonic partials, each with its own decay.
    partials = [(1.0, 1.0, 1.2), (2.76, 0.45, 0.5), (5.40, 0.25, 0.25), (8.93, 0.12, 0.12), (13.34, 0.06, 0.08)]
    out = []
    for i in range(n):
        t = i / rate
        x = 0.0
        for ratio, amp, tau in partials:
            f = root_hz * ratio
            if f < rate / 2 * 0.9:
                x += amp * math.exp(-t / tau) * math.sin(2 * math.pi * f * t)
        env = min(1.0, t / 0.002)   # a 2 ms attack against the click
        out.append(0.6 * env * x)
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak * 0.85 for v in out]

def write(path, samples):
    w = wave.open(path, "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples)); w.close()

os.makedirs(os.path.join(out_dir, "Samples"), exist_ok=True)
zones = []
for note in (60, 72, 84):
    hz = 440.0 * 2 ** ((note - 69) / 12)
    name = f"mb_{note}.wav"
    write(os.path.join(out_dir, "Samples", name), tone(hz, 1.3))
    zones.append((note, name))
with open(os.path.join(out_dir, "demo.dspreset"), "w") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
    f.write('<DecentSampler minVersion="1.0">\n')
    f.write('  <!-- The demo instrument\'s placeholder: a synthesised music box (tools/make_demo_instrument.py), public domain. -->\n')
    f.write('  <groups attack="0.0" decay="0" sustain="1.0" release="0.15" ampVelTrack="0.7">\n    <group>\n')
    ranges = [(0, 65), (66, 77), (78, 127)]
    for (note, name), (lo, hi) in zip(zones, ranges):
        f.write(f'      <sample path="Samples/{name}" rootNote="{note}" loNote="{lo}" hiNote="{hi}"/>\n')
    f.write('    </group>\n  </groups>\n')
    f.write('  <effects>\n    <effect type="reverb" roomSize="0.6" damping="0.5" wetLevel="0.2"/>\n  </effects>\n')
    f.write('</DecentSampler>\n')
print("demo instrument written to", os.path.abspath(out_dir))
