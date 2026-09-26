#!/usr/bin/env python3
"""The demo instrument (Phase 7 step 53, SOUND §3 — DECISIONS_6 #22): the
Dan Tranh (a Vietnamese zither) from the Versilian Community Sample Library,
CC0 1.0 — the author's pick. Two outputs:

  1. THE TEST LIBRARY (not committed): the full "Normal" articulation — 48
     WAVs, 16 notes x 3 velocity layers, ~34 MB — downloaded from the VCSL
     repository into --library-dir (default ~/Music/midi-sink/Dan Tranh
     (VCSL)/) with "Dan Tranh.dspreset" converted from the SFZ beside it:
     a real multi-layer instrument for the tests and the hands-on.
  2. THE BUNDLED DEMO (committed, voxo/demo/): the "f" layer alone — 16
     samples — as 32 kHz 16-bit mono, the SFZ offset applied, trimmed to 3 s
     with a fade, each normalised to -3 dBFS (ffmpeg), plus demo.dspreset
     and LICENSE.txt. About 3 MB: what every shell bundles so a first launch
     makes a sound.

  python3 tools/fetch_dan_tranh.py [--library-dir DIR] [--no-demo]

Needs ffmpeg on PATH for the demo's conversion; the download alone needs
nothing but the network."""
import argparse, os, re, shutil, subprocess, sys, urllib.request

RAW = "https://raw.githubusercontent.com/sgossner/VCSL/sfz/Chordophones/Zithers/"
SFZ = "Dan Tranh - Normal.sfz"

def fetch(url, dest):
    if os.path.exists(dest) and os.path.getsize(dest) > 0:
        return
    with urllib.request.urlopen(url) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)

def parse_sfz(text):
    """The SFZ subset VCSL writes: <group> defaults, <region> opcodes, one per line."""
    group, regions, cur = {}, [], None
    for line in text.splitlines():
        line = line.split("//")[0].strip()
        if not line:
            continue
        if line == "<group>":
            cur = group; continue
        if line == "<region>":
            cur = dict(group); regions.append(cur); continue
        m = re.match(r"([a-z_0-9]+)=(.*)", line)
        if m and cur is not None:
            cur[m.group(1)] = m.group(2).strip()
    return regions

def note_name(n):
    return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12] + str(n // 12 - 1)

def write_dspreset(path, regions, sample_dir, attack, release, with_volume=True, with_start=True, name="Dan Tranh"):
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<DecentSampler minVersion="1.0">',
             f'  <!-- {name}: Versilian Community Sample Library (VCSL), CC0 1.0 - converted from its SFZ by tools/fetch_dan_tranh.py -->',
             f'  <groups attack="{attack}" decay="0" sustain="1.0" release="{release}" ampVelTrack="0.6">', '    <group>']
    for r in regions:
        attrs = [f'path="{sample_dir}/{os.path.basename(r["sample"])}"',
                 f'rootNote="{r["pitch_keycenter"]}"', f'loNote="{r["lokey"]}"', f'hiNote="{r["hikey"]}"',
                 f'loVel="{r.get("lovel", 0)}"', f'hiVel="{r.get("hivel", 127)}"']
        if with_start and "offset" in r:
            attrs.append(f'start="{r["offset"]}"')
        if with_volume and "volume" in r:
            attrs.append(f'volume="{float(r["volume"]):.2f}dB"')
        lines.append("      <sample " + " ".join(attrs) + "/>")
    lines += ["    </group>", "  </groups>", "  <effects>",
              '    <effect type="reverb" roomSize="0.7" damping="0.4" wetLevel="0.18"/>', "  </effects>", "</DecentSampler>"]
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--library-dir", default=os.path.expanduser("~/Music/midi-sink/Dan Tranh (VCSL)"))
    ap.add_argument("--no-demo", action="store_true")
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    lib = a.library_dir
    os.makedirs(os.path.join(lib, "Samples"), exist_ok=True)
    sfz_path = os.path.join(lib, SFZ)
    fetch(RAW + urllib.parse.quote(SFZ), sfz_path)
    regions = parse_sfz(open(sfz_path).read())
    print(f"{len(regions)} regions in the SFZ")
    for i, r in enumerate(regions):
        name = os.path.basename(r["sample"])
        fetch(RAW + urllib.parse.quote("Dan Tranh/Normal/" + name), os.path.join(lib, "Samples", name))
        print(f"  [{i + 1}/{len(regions)}] {name}", end="\r")
    print()
    attack = float(regions[0].get("ampeg_attack", 0.004)); release = float(regions[0].get("ampeg_release", 0.3))
    write_dspreset(os.path.join(lib, "Dan Tranh.dspreset"), regions, "Samples", attack, release)
    print("the test library:", os.path.join(lib, "Dan Tranh.dspreset"))
    if a.no_demo:
        return 0
    if not shutil.which("ffmpeg"):
        print("ffmpeg is not on PATH: the demo's conversion needs it", file=sys.stderr)
        return 1
    demo = os.path.join(here, "..", "voxo", "demo")
    shutil.rmtree(os.path.join(demo, "Samples"), ignore_errors=True)
    os.makedirs(os.path.join(demo, "Samples"), exist_ok=True)
    for f in os.listdir(demo):
        if f.endswith(".dspreset") or f.endswith(".txt"):
            os.remove(os.path.join(demo, f))
    chosen = [r for r in regions if "_f_" in r["sample"]]
    demo_regions = []
    for r in chosen:
        src = os.path.join(lib, "Samples", os.path.basename(r["sample"]))
        name = note_name(int(r["pitch_keycenter"])).replace("#", "s") + ".wav"
        dst = os.path.join(demo, "Samples", name)
        offset = int(r.get("offset", 0))
        # Pass 1: the peak after the offset and the trim; pass 2: normalise to -3 dBFS.
        probe = subprocess.run(["ffmpeg", "-hide_banner", "-i", src, "-af", f"atrim=start_sample={offset},atrim=duration=3,volumedetect", "-f", "null", "-"],
                               capture_output=True, text=True)
        m = re.search(r"max_volume: (-?[\d.]+) dB", probe.stderr)
        gain = -3.0 - float(m.group(1)) if m else 0.0
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", src,
                        "-af", f"atrim=start_sample={offset},atrim=duration=3,volume={gain:.2f}dB,afade=t=out:st=2.6:d=0.4",
                        "-ac", "1", "-ar", "32000", "-c:a", "pcm_s16le", dst], check=True)
        d = dict(r); d["sample"] = name; d["lovel"] = "0"; d["hivel"] = "127"
        d.pop("offset", None); d.pop("volume", None)
        demo_regions.append(d)
    # The demo covers the whole keyboard: the instrument is pentatonic and its
    # SFZ leaves gaps, so every zone stretches up to the next one and the ends reach 0 and 127.
    demo_regions.sort(key=lambda d: int(d["pitch_keycenter"]))
    for i, d in enumerate(demo_regions):
        d["lokey"] = "0" if i == 0 else str(int(demo_regions[i - 1]["hikey"]) + 1)
        d["hikey"] = "127" if i == len(demo_regions) - 1 else str((int(d["pitch_keycenter"]) + int(demo_regions[i + 1]["pitch_keycenter"])) // 2)
    write_dspreset(os.path.join(demo, "demo.dspreset"), demo_regions, "Samples", attack, release, with_volume=False, with_start=False,
                   name="Dan Tranh (the demo)")
    with open(os.path.join(demo, "LICENSE.txt"), "w") as f:
        f.write("Dan Tranh - Versilian Community Sample Library (VCSL), by Versilian Studios and contributors.\n"
                "CC0 1.0 Universal (public domain dedication): https://github.com/sgossner/VCSL\n"
                "These files are the 'Normal' articulation's f layer, converted to 32 kHz 16-bit mono, trimmed to\n"
                "three seconds and normalised, by tools/fetch_dan_tranh.py; the preset beside them is midi-sink's.\n")
    total = sum(os.path.getsize(os.path.join(demo, "Samples", f)) for f in os.listdir(os.path.join(demo, "Samples")))
    print(f"the demo: {len(demo_regions)} zones, {total / 1024 / 1024:.1f} MB in {os.path.abspath(demo)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
