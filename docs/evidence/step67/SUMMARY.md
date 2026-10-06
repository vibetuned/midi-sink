# Step 67 — Documentation (`DECISIONS_9 #1–#5`)

The book caught up with Phases 6–9, complete except the gallery's three
recordings, which the author provides after seeing the book.

## What shipped

* **The operator book**: `torsion`, `chladni`, `burst`, `spark`, `chirikov`,
  `anod`, `palettes` under `site/src/content/docs/operators/`, from the
  Phase-6 drafts checked against Part V (the spark as the Anod strike on its
  charge, #88; the Chladni keys as eddies with the Taylor–Green flow kept as
  the gesture, #59–#62; the Chirikov ceiling 1.25, #42; the burst back in
  the strike with `burst_order`); the index tables fourteen operators by
  class. The burst page carries the author's note, signed "— the author".
* **The Suzu book**: `site/src/content/docs/suzu/` — index (the thesis, the
  DSP class table, the voices, the MPE mapping, the gates), cells,
  modal-voice, strings-and-chaos, winds, orbit-trace, sound-profile,
  patches, lab; 25 figures in `site/src/assets/suzu/`; every voice family's
  live panel through the new `site/src/components/Lab.astro`.
* **The lab's address**: `/marble/suzu/` (DECISIONS_9 #2) — `pages.yml`
  and `site/scripts/compose-marble.mjs` copy `suzu-dist` beside the marble
  app; `web/suzu/site/lab-core.js` `DOCS_ROOT = '../../'`.
* **The user guide**: new `instruments`, `sound`, `medium`, `presets`,
  `replay`; `layouts`, `control-strip`, `play-mode`, `stylus`,
  `paper-and-prints`, `desktop`, `web`, `marble-mode`, `devices` updated.
* **Reference**: `settings.md` rewritten from the desktop window's twenty
  sections; `citations.md` extended (22 entries; the thanks with Professor
  Jaffer first); `licensing.md` from the Voxo draft; the chart's new
  sections and rows (`src/data/midi-chart.json`) with the step-63 logs in
  `tests/fixtures/bytelogs/`.
* **Architecture**: `replay.md` new; `hostmpe.md` and `index.md` extended.
* **Gallery**: three pending cards with `replay` links (the captions are
  placeholders for the author, #4); `Gallery.astro` renders "watch it
  again"; `public/gallery/README.md` documents the field.
* **Mechanics**: `check.mjs` requires all seventeen scenes and the six lab
  pages and admits no other iframe; a prose link into the lab must follow
  the embed root (`LabLink.astro`); `pages.yml` embeds `/marble/rc/` and
  names the RC in the footer while an RC is newer than the stable (#2);
  `build-notes.mjs` publishes `_work/DECISIONS_9.md` as Part IX; the
  sidebar; the site README. The home and support pages link the App Store
  and Google Play listings; the support page's known limits are current.

## Gates

| Gate | Result | File |
|---|---|---|
| `npm run build` + `check.mjs`, against `/marble/` and against `/marble/rc/` | 66 pages from 65 sources; 17/17 scenes, 6/6 lab pages embedded; 0 problems either way (the pending-replay notes) | `check_mjs.txt`, `check_mjs_rc.txt` |
| `tools/chart_check.py` | 52 ok, 0 failed — CHART MATCHES THE BYTE LOGS | `chart_check.txt` |
| captures (`web_gate.mjs --fullshots` on the composed `site/dist`) | 14 pages | `captures/` |

## The author's two fixes on the book (#6, #7)

* The medium toggle on every scene, permanent from the first drop
  (`web/site/scenes.js`): the sweep of all seventeen scenes and three
  captures in `scenes/` (Anod; torsion and the spark under Anod).
* The RC's Windows lane: `voxo/src/backend_none.cpp` reads
  `QueryPerformanceCounter` under `_WIN32` — unverified here, proven by
  `build.yml`'s Windows lane on the push.

## `[ITERATE]` resolution

The table is `DECISIONS_9 #1`: every item of INSTRUMENT, QOL, the medium
and the synth sections is resolved by an earlier entry or documented as a
limit on its page (microtonal tunings, the preset-next pad, the fibre angle
drift, TIFF-16, the long-exposure buffer, the fold's ×4 budget, the
sustained small drive).

## Open for the author

* The nine "coming soon" gallery cards: the captures and their captions
  (placeholders now), and the `.sumireplay` files the cards name.
* The tag: the first v2 RC, cut after this commit — `pages.yml` then
  embeds `/marble/rc/` in the docs and names the RC in the footer (#2).
* The burst note's signature (the name is the author's to put) and any
  roughening of its text.
