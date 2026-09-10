# Screenshot plan — Google Play (author input from real sessions)

Play wants, per form factor, 2–8 screenshots, JPEG or 24-bit PNG, each side
320–3840 px, aspect between 16:9 and 9:16 (Play Console rejects anything
squarer). Provide the **7-inch and 10-inch tablet** sets (the app targets
tablets) and a **phone** set (the listing shows it to phones too):

* **10-inch tablet** — the Galaxy Tab S8 Ultra is 2960×1848 (16:10): use its
  native screenshots (`adb exec-out screencap -p > shot.png`).
* **7-inch tablet** — same captures are accepted (the ratio is what matters).
* **Phone** — crop the 16:10 captures to 16:9 (2960×1665) or capture on a
  phone; both modes work on any Android 10+ device with OpenGL ES 3.

Six slots, in this order (the first two are what the listing shows first):

1. **Marble mode, mid-performance** — a ROLI or Osmose playing: rings, combs
   and a vortex on the paper. The hero shot.
2. **Play mode, chromatic grid** — several fingers down, joystick rings and
   thumbs visible, the floating strip top-left, drops under the fingers.
3. **S-Pen legato** — a glissando trail of drops across the piano grid, the
   pen's hover ghost visible.
4. **Piano grid** — the two-row keyboard lattice with the black-key corridor,
   one hand playing.
5. **Settings sheet** — Mode, Note bend, Slide, Outbound MIDI with the USB
   status line reading *active*, About showing the tag.
6. **A finished sheet** — after a paper dip, the print alone.

Also needed: the **feature graphic** (1024×500 PNG/JPEG, no alpha) — a crop
of a real print with the wordmark; and the **app icon** (512×512 PNG) from
`tools/gen_icons.py` output (`packaging/` / the launcher mipmaps' source).

Capture from a tagged build so About in slot 5 reads the release. Do not
retouch the marbling; do crop the status bar if it carries personal
information (time and battery are fine).

---

## What is captured (v1.0.0, 2026-09-10)

Captured from the tagged `v1.0.0` build (`versionCode 56`) installed on the
author's **Galaxy Tab S8 Ultra (SM-X906B, Android 16)**, driven through the
real UI with `adb` — every sheet is a real session, nothing is mocked or
retouched.

| File | Shows | Palette · layout |
|---|---|---|
| `tablet/01-marble-sumi.png` | combed suminagashi, the hero | Sumi black · Circle of fifths |
| `tablet/02-play-chromatic-indigo.png` | Play mode: lattice, control strip, a finger down with its joystick ring | Indigo · Chromatic grid |
| `tablet/03-controls-cc-map.png` | Vortex / Stylus wake / Ripple controls and the CC map, marbling behind | Ochre |
| `tablet/04-marble-ochre.png` | a second sheet, the third palette | Ochre · Circle of fifths |

**Geometry.** The Tab S8 Ultra is 2960×1848 — **16:10**, which Play's
large-screen slots reject. Each file is cropped to **2960×1665**, exactly
16:9, taking the band from y=110 (below the status bar) so the system chrome
and the gesture pill are out of frame. `tools/`-worthy script:
`crop169.py` in the task evidence folder.

**Counts.** Play needs a minimum of 2 phone screenshots and wants 4 for the
large-screen slots. These four satisfy the tablet slots; the same four are
16:9 and can be reused for the phone slots.

**Still to capture (optional).** A 7-inch tablet set is not needed — the same
16:9 files serve both tablet slots. A promo video is a **YouTube URL**, not an
upload: an existing gallery performance can be pasted straight in.
