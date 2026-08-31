# Step 6 DONE evidence — wind mode & Airwave CC routing

Date: 2026-09-01. Same rig. All GUI runs `METAL_DEVICE_WRAPPER_TYPE=1`, zero
validation output.

New: wind mode (single wandering-brush voice: breath CC2/CC11/chanAT feeds a
breath-proportional line width, legato note change = VoiceMigrate + wake
tine), CC routing table with README-documented Airwave defaults
(sumi_map_cc / sumi_clear_cc_map live), global controls as smoothed dt-scaled
state (vortex strength/center, viscosity damping; roughness/palette tracked
for the later composite). README.md added.

| DONE check | Result | Evidence |
|---|---|---|
| Scripted mono breath stream → one continuous wandering ink line, breath-modulated thickness | **PASS** — tests/wind_breath.swift (2 225 msgs / 20 s, ~150 Hz CC2 + legato melody): a single calligraphic line wanders the canvas, thick where breath swelled, thin flicks where it dropped; `input mode -> wind` auto-detected; 0 dropped, 99.7 fps | `wind_wandering_line.png`, `run_wind.log` |
| Runtime sumi_map_cc of an arbitrary CC to vortex strength visibly modulates the swirl | **PASS** — A/B with the same CC30-sweep script: `--map-cc 30:0` produces a deep nautilus swirl; the unmapped control leaves the rings untouched | `remap_mapped.png` vs `remap_control.png`, `run_remap_*.log` |
| Classic and MPE unaffected (step 4/5 scripts) | **PASS** — step-4 midisend: stays classic, 99.7 fps, 0 dropped; step-5 osmose_stress: MPE auto-engaged, 99.7 fps, 0 dropped; all 460 headless checks green (incl. unchanged step-4/5 suites) | `run_regression_classic.log`, `run_regression_mpe.log`, ctest |

Also proven by unit tests: legato migrate (single drop counter, wake tine,
feeds continue at the new position with the same ink band), stale note-off
ignore, chanAT breath alias, CC map defaults / runtime remap / per-channel
precedence / clear-removes-defaults, per-frame vortex at the routed center,
viscosity damping (§2.2 R-Tilt).

**Design finding:** literal unbounded breath integration paints a blob, not a
line — wind now targets a breath-proportional brush width (DECISIONS.md #34;
before/after in the git history of this file's captures).

## Addendum: live mode handover (user-reported)

Playing the MPE piano permanently latched the mode, blocking later wind lines.
Detection is now activity-windowed (DECISIONS.md #37) and breath aliases
include CC7 (#38). End-to-end proof in one session
(`run_handover.log`, `handover_wind_after_mpe.png`): osmose stress → MPE;
~7 s of silence; wind_breath stream → wind, brush lines drawn over the MPE
marbling; exactly two mode transitions, 0 dropped; piano resumption flips
back to MPE (unit-tested, 468 checks green). A mode switch also synthesizes
VoiceEnd for the old dialect's voices — no stuck feeds.
