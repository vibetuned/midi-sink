#!/usr/bin/env python3
"""Turn the 2.0 raw captures into the stores' files (2026-10-14, DECISIONS_9 #18).

  assemble20.py <raw dir> <repo root>

The Galaxy Tab S8 Ultra's 2960x1848 captures are cropped to Play's 16:9 as
crop169.py does (the status bar off the top, 2960x1665); the Pixel 10 Pro
emulator's 2856x1280 landscape captures are centre-cropped to 16:9
(2276x1280); the iPad Pro 13" (2064x2752) and iPhone 17 Pro Max (1320x2868)
simulator captures are App Store Connect's exact portrait sizes and are
written as they are. Every file is 24-bit RGB, no alpha. Slots are numbered
after the files already in each folder."""
import os, re, sys
from PIL import Image

raw, repo = sys.argv[1], sys.argv[2]

def next_index(folder):
    os.makedirs(folder, exist_ok=True)
    n = [int(m.group(1)) for f in os.listdir(folder) for m in [re.match(r"(\d\d)-", f)] if m]
    return (max(n) + 1) if n else 1

def put(src, folder, slug, crop=None, expect=None):
    im = Image.open(os.path.join(raw, src)).convert("RGB")
    if crop == "tab":
        assert im.size == (2960, 1848), (src, im.size); im = im.crop((0, 110, 2960, 110 + 1665))
    elif crop == "phone":
        w, h = im.size; tw = int(round(h * 16 / 9)); x0 = (w - tw) // 2; im = im.crop((x0, 0, x0 + tw, h))
    if expect: assert im.size == expect, (src, im.size, expect)
    i = next_index(folder); dst = os.path.join(folder, f"{i:02d}-{slug}.png"); im.save(dst, optimize=True)
    print(f"{os.path.relpath(dst, repo):58s} {im.size[0]}x{im.size[1]}")

tab = os.path.join(repo, "android/metadata/screenshots/tablet")
for src, slug in [("tab_07_janko_anod_raw.png", "anod-janko-midi"), ("tab_10_strings_midi_raw.png", "play-strings-midi"),
                  ("tab_08_trumpet_raw.png", "play-trumpet-arc"), ("tab_09_sound_raw.png", "settings-sound"),
                  ("tab_11_replay_raw.png", "replay"), ("tab_12_network_raw.png", "outbound-network-session")]:
    put(src, tab, slug, crop="tab", expect=(2960, 1665))

phone = os.path.join(repo, "android/metadata/screenshots/phone")
for src, slug in [("emu_07_anod_raw.png", "anod-janko-midi"), ("emu_11_replay_raw.png", "replay"),
                  ("emu_10_strings_end_raw.png", "play-strings-midi"), ("emu_08_trumpet_raw.png", "play-trumpet-arc")]:
    put(src, phone, slug, crop="phone", expect=(2276, 1280))

ipad = os.path.join(repo, "ios/metadata/screenshots/ipad-13")
for src, slug in [("ipad_07_anod_raw.png", "anod-janko-midi"), ("ipad_11_replay_raw.png", "replay"), ("ipad_10_strings_end_raw.png", "play-strings-midi")]:
    put(src, ipad, slug, expect=(2064, 2752))

iphone = os.path.join(repo, "ios/metadata/screenshots/iphone-6.9")
for src, slug in [("iphone_07_anod_raw.png", "anod-janko-midi"), ("iphone_11_replay_raw.png", "replay"), ("iphone_10_strings_end_raw.png", "play-strings-midi")]:
    put(src, iphone, slug, expect=(1320, 2868))
