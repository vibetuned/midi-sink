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

---

## 2.0 — the slots to add (author input, from real sessions)

The 1.0 set stands; 2.0 wants the new things seen. Same rules and sizes.

7. **Anod** — a discharge mid-performance, the glow and the water's grid
   (electric blue): the second hero.
8. **The trumpet** — the fingering panel's three pads at the side, a partial
   lit under a finger (the arc).
9. **The Sound page** — the source row, the Suzu patch picker, the
   instrument list with the demo.
10. **The strings** — a chord shape, every string's echo lit.
11. **A replay** — the banner naming the source device over the water.
12. **Outbound MIDI** — the network session's status line naming the
    connected host.

## What is captured (2.0, 2026-10-14)

Captured from the 2.0 build (`2.0.0-rc.5`, the tree at the Phase-10 fold).
The tablet shots are the **Galaxy Tab S8 Ultra**'s own captures cropped to
16:9 (`crop169.py`'s crop, in `assemble20.py`); the phone shots are the
**Pixel 10 Pro emulator** (Android 16, host GPU) in landscape, centre-cropped
to 16:9. The dense scenes are a scripted MPE performance
(`tools/store_screenshots/perform.py`) played over USB into the Tab — the
Tab is the Mac's "SAMSUNG_Android" MIDI port — on the strings (Sumi, indigo)
and on Jankó (Anod, electric blue), recorded with the lab recorder and
replayed on the emulator (`--es replayFile`), so the phone shows the same
performance.

| File | Shows | Medium · palette · layout |
|---|---|---|
| `tablet/05-anod-janko-midi.png` | the discharge: the performance on Jankó — the second hero | Anod · electric blue · Jankó |
| `tablet/06-play-strings-midi.png` | the strings under the performance, every string's echo | Sumi · indigo · strings |
| `tablet/07-play-trumpet-arc.png` | the trumpet on the arc, the fingering panel's three pads, a partial struck | Sumi · indigo · trumpet |
| `tablet/08-settings-sound.png` | the Sound page: the source, the Suzu patch, the synth trace, the instruments | — |
| `tablet/09-replay.png` | the Jankó performance replaying, the banner naming the source | Anod · electric blue · Jankó |
| `tablet/10-outbound-network-session.png` | Outbound MIDI with the network session connected to a host | — |
| `phone/01-anod-janko-midi.png` | the discharge on a phone | Anod · electric blue · Jankó |
| `phone/02-replay.png` | the replay banner on a phone | Anod · electric blue · Jankó |
| `phone/03-play-strings-midi.png` | the strings on a phone | Sumi · indigo · strings |
| `phone/04-play-trumpet-arc.png` | the trumpet's arc on a phone, the fingering demo's drops | Sumi · black · trumpet |

**Two things the captures taught.** The Tab's Adreno loses most of the
small Anod charges a tap lays (the spark's charge is a third of the drop,
`DECISIONS_5 #88`'s drift): forty spaced taps left fourteen charges on the
Tab while the same recording replayed on the iPad shows them all — so the
Anod hero is MIDI-driven, where the notes' charges are larger. And the
lab recorder (`--es recordLab`) draws garbage on the Tab's screen while
its 640×400 lab surface is up — the recording and the field dump are
fine; it is the lab's display, carried (`DECISIONS_9 #18`).
