# ROLI Airwave — raw capture of the twelve dimensions (2026-09-06)

Capture: `airwave_raw.log` (`SUMI_MIDI_LOG=1`, harness raw log with
timestamp + source port — the port tag was added for this capture).
Session: user exercised all twelve Airwave dimensions, one at a time,
t ≈ 22 s → 51 s. 72 messages total.

## What the Airwave sends

Every message is a plain **CC on channel 1** through the **"ROLI Airwave
Expression"** port (nothing on the Pedal or Piano ports, no bend, no
aftertouch, no notes). Twelve distinct CCs. **The user played the pairs in
a stated order — left hand then right hand, for Air Glide, Air Raise, Air
Tilt, Air Flex, Air Slide, Air Grasp — which names every CC:**

| # | t (s) | CC | Airwave dimension | midi-sink default route (README table) |
|---|-------|----|-------------------|------------------------------------------|
| 1 | 22.6 | **CC 24** | **Air Glide — left** | Paper roughness |
| 2 | 26.2 | **CC 25** | **Air Glide — right** | Palette morph |
| 3 | 29.6 | **CC 26** | **Air Raise — left** | — none — |
| 4 | 31.7 | **CC 27** | **Air Raise — right** | — none — |
| 5 | 34.1 | **CC 28** | **Air Tilt — left** | — none — |
| 6 | 36.3 | **CC 29** | **Air Tilt — right** | — none — |
| 7 | 38.9 | **CC 30** | **Air Flex — left** | — none — |
| 8 | 41.3 | **CC 31** | **Air Flex — right** | — none — |
| 9 | 43.3 | **CC 22** | **Air Slide — left** | Vortex center Y |
| 10 | 45.3 | **CC 23** | **Air Slide — right** | Viscosity |
| 11 | 47.7 | **CC 20** | **Air Grasp — left** | Vortex strength |
| 12 | 49.7 | **CC 21** | **Air Grasp — right** | Vortex center X |

So the real Dashboard assignment is, per gesture (left, right):
**Grasp 20/21 · Slide 22/23 · Glide 24/25 · Raise 26/27 · Tilt 28/29 ·
Flex 30/31** — the contiguous CC 20–31 block, paired L/R.

midi-sink's default map only routes **20–25** — **six of the twelve
dimensions (Raise, Tilt and Flex, both hands: CC 26–31) land on unmapped
CCs and do nothing**. Worse, the README table's intent labels do not match
the device: it imagined CC 20 = "L-Raise (wind over the water)", CC 21/22
= "Glide X/Y", CC 23 = "R-Tilt", CC 24 = "Flex" — but on the real Airwave
CC 20/21 are **Grasp**, 22/23 **Slide**, 24/25 **Glide**, and Raise / Tilt
/ Flex are the six unmapped CCs. So even the six dimensions that DO reach
a dimension drive the "wrong" one relative to the table's intent. That is
the whole "mapping issues" report. The ripple dims listen on CC 102/103 by
default, which the Airwave never sends.

## Note on the values in this capture

Every dimension shows only two short bursts of 125/126/127 — that was
DELIBERATE (user confirmed): each dimension was pinged twice near its
maximum purely to identify its CC. The Airwave transmits the full 0–127
range in normal play; nothing is filtered or threshold-configured. Ranges
need no re-verification before the mapping fix.

## For the later fix (not done now — user's call)

* Options: extend `install_default_cc_map` / the README table with the six
  missing CCs (26–31 → e.g. ripple amp/freq and other dims), or leave the
  defaults and publish an Airwave preset through the settings window's CC
  map (the editor + INI already handle it — routes added there persist and
  survive restarts, proven in the Step-29 evidence).
* Any change to the DEFAULT map touches the core's `install_default_cc_map`
  and the README/chart — core is frozen until Step 33's scoped unfreeze;
  the CC-map editor route needs no core change at all.
