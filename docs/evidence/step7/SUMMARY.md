# Step 7 DONE evidence — washi paper, palettes, paper dip

Date: 2026-09-01. Same rig. All GUI runs `METAL_DEVICE_WRAPPER_TYPE=1`; zero
validation output in every log.

New: §4.5 composite (simplex mulberry-fiber strands + sizing mottle +
absorption grain scaled by paper_roughness; sumi/indigo/ochre palettes with
palette-morph blending and aux-channel per-drop hue drift; linear math with
manual sRGB encode), live param keys (1/2 viscosity, 3/4 expansion, 5/6
roughness, 7 palette, 8 layout, 9 dip, S save PNG), full paper dip
(snapshot -> async blit on sokol's queue -> poll -> sumi_read_print; "lift
the paper" fade; identity reset), harness PNG export via stb_image_write on a
background thread. README documents the keys and CC defaults.

| DONE check | Result | Evidence |
|---|---|---|
| Dip during heavy MPE exports a correct full-res PNG with no hitch > 1 frame @60 fps | **PASS** — osmose stress + `--dip-at`: 2560×1440 PNG exported; dip-window worst frame 14.09 ms (< 16.7 ms), and across 3 trials 14–20 ms — always below the same runs' ambient scheduler jitter (31–37 ms maxima outside the window); 0 dropped messages | `run_dip.log`, `dip_print.png` |
| All palettes and both layouts switch live without artifacts | **PASS** — `--cycle-visuals` cycles palette 0/1/2 + layout + roughness live while painting: sumi, indigo, ochre captures all sharp, fibers scale with roughness, no artifacts at switch | `palette_sumi.png`, `palette_indigo.png`, `palette_ochre.png`, `run_palettes.log` |
| Exported print matches the on-screen frame at dip time | **PASS** — screen captured ~0.6 s before the dip vs the exported print: identical structure/tones (differences are only the strokes painted in that 0.6 s); both share the same linear→sRGB path | `screen_at_dip.png` vs `dip_print.png`, plus `screen_after_dip.png` (fresh bath + fade) |

**Found & fixed by the hitch measurement:** inline PNG encoding stalled the
loop ~1080 ms — moved to a detached thread (DECISIONS.md #42); print CPU
buffer and staging MTLBuffer are preallocated so the dip frame does no
allocation (#41).

Regressions: ctest green (abi + 468 normalizer checks), `leaks --atExit`
across a dip = unchanged OS baseline (288/18 816 B, no core frames),
~99 fps sustained in all runs.
