# Evidence — Step 44b: the web marble (medium, palettes, editor, presets)

ROADMAP_5 Step 46 (the author's 44b); MEDIUM §1, QOL §1 and §3, SPEC §9.5.
Decisions: `_work/DECISIONS_5.md` #74, #75 (the gestures). Machine: the author's Mac, headless
Chrome with WebGPU (the gate's own launch).

## What landed

* The one preset serializer in the wasm (`presets/` in the web build) and a
  shim (`web/sumi_web.cpp`): the session captured, written, read over and
  applied through it; the palette flattened for JS.
* The page's session is preset JSON (localStorage, restored on load); named
  presets, export to a file, import of any preset.
* The panel: Medium, Substrate, Palette (library, Load into custom, the
  custom editor), Presets, the 1.1 routing modes, dip / clear copy.
* `tools/web_gate.mjs --preset <file>`: a permanent preset round-trip gate.
* `site/scripts/check.mjs` knows the Phase-6 scenes and cross-checks its list
  against `web/site/scenes.js`.

## The gestures follow the medium (#75)

The author, before the handoff: the marble gestures ignored the medium. Now
five core calls (`sumi_gesture_tap / _pinch / _twist / _press / _press_end`)
play Sumi exactly as before and, in Anod, the author's table — tap = the
strike, pinch = the burst, twist = the torsion vortex, long press = the
torsion feed (pull = the Chladni stir, reversed). Every shell (desktop, iPad,
web) calls them; Android joins at 45b. `gesture_test.log` (`--gesture-test`,
6/6): Sumi bitwise the 1.0 calls; each Anod gesture plays its operator.
`anod_gestures.png`: the four on one sheet (tap upper left, pinch upper
right, twist lower left, press lower right).

## DONE (step 44b)

| Criterion | Result |
|---|---|
| the web gate is green | §4.6 web tier PASS — max 9.8·10⁻⁴, mean 7.9·10⁻⁹ (`web_field_gate.log`) |
| the scene sweep is green | 17/17 scenes, the six Phase-6 ones included (`web_scenes.log`) |
| check.mjs learns the new scene names | built site checked: `ok.` (`site_check.log`) |
| presets in localStorage with export | the panel test below; the author's desktop session through the page's import and export: **byte-identical**, 2040 bytes (`preset_roundtrip.log`) |
| the marble page plays Anod from a ROLI over Web MIDI in Chrome | the author's to play (Web MIDI is unchanged; the medium switch is live) |

`panel_test.log` — the panel driven in headless Chrome through its own DOM:
Anod shows its three palettes and its glass rows (the paper rows hidden);
the library lists the six Anod presets; "Load into custom" makes Custom
active and opens the editor; a hue-drift edit reaches the saved session;
growing the stops inserts the desktop's midway stop (0.498 = the linear
midpoint, at 0.75) and shrinking removes it; the panel's order holds after
rebuilds; a preset saved, the medium switched to Sumi (its three palettes
listed), the preset loaded back (Anod, Custom, drift 0.7); after a reload
the session restores (Anod, drift 0.7). Screens: `web_anod_palette.png`,
`web_after_reload.png`.

The panel test was a scratch DevTools script (not kept in the tree); the
preset gate mode is kept.

## Flagged

* ROADMAP_5 puts the phase end (tag `v2.0.0-alpha.1`, fold DECISIONS_5) on
  this step; in the author's numbering the phase ends after 46 (Windows).
