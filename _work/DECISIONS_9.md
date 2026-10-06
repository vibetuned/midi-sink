# Part IX — Phase 10: Publish (steps 67–70) — in flight (`DECISIONS_9 #n`)

Opened at step 67 (the documentation), 2026-10-06. Merged into
`docs/DECISIONS.md` as Part IX at the phase's end. The spec is
`docs/PROJECT_SPEC.md` §9.6 (the books) and the `[ITERATE]` lists of the
four Phase 6–9 specs; the roadmap is `_work/ROADMAP_5.md`.

1. **Every `[ITERATE]` of the four specs is resolved by a decision entry or
   documented as a limit on the page that owns it — the table.** (The
   roadmap's DONE clause for step 67.) Where an item is "documented as a
   limit", the page says so in a sentence a reader finds where they would
   look for the feature; nothing is resolved here that an earlier entry did
   not already resolve.

   | Spec | Item | Resolution |
   |---|---|---|
   | INSTRUMENT §1 | the valve CC numbers | `DECISIONS_8 #1`: CC 110/111/112 (≥ 64 pressed), the slide CC 113, the master channel — the chart's new rows |
   | INSTRUMENT §1 | the slide: a 14-bit pair or 7-bit + smoothing | `#1`: 7-bit, smoothed 10 ms in the engine (4.7 cents a step) |
   | INSTRUMENT §1 | should pressed valves render? | `#9`, `#13`–`#17`: the strip's widgets and the fingering panel show it; the canvas stays pure ink; the web's HUD is an overlay (`#25`) — the instruments guide |
   | INSTRUMENT §2 | the partials: a column or an arc | `#2` (`params.trumpet_arc`), the ring re-cut by the author's eye `#15`–`#17`; both documented, the arc the author's choice on the iPad |
   | INSTRUMENT §2 | the lip bend's scaling | `#2`: one semitone per cell radius, the cell's +x axis |
   | INSTRUMENT §4 | the fretboard's tuning table: params or fixed | `#6`: three fixed presets (`params.string_tuning`), documented with their tunings |
   | INSTRUMENT §5 | the theremin's "continuous": a radius sentinel or a flag | `#7`: a flags field, `SUMI_CELL_CONTINUOUS` |
   | INSTRUMENT §7 | microtonal / Scala tunings pulled forward? | NOT built — documented as a limit on the instruments guide ("Not in 2.0") |
   | QOL §2 | a panic button on the strip | `#12`: built (the strip's Panic pad; the desktop's Panic beside its inputs) — the control-strip guide |
   | QOL §2 | the quick-switch: a user-chosen subset | `#12`: a subset in the order chosen, the Next pad — the control-strip and instruments guides |
   | QOL §2 | a preset-next button on the strip | NOT built — documented as a limit on the presets guide |
   | QOL §3 | expose the fibre angle-drift amount? | NOT exposed, a constant — documented on the medium guide's substrate table |
   | QOL §3 | TIFF-16 for print workflows | NOT built, PNG only — documented on the paper-and-prints guide |
   | MEDIUM (TO_PROJECT_SPEC §11) | a long-exposure / strain accumulation buffer | carried with its question — documented on the Anod page ("Not in 2.0") |
   | SOUND (§12) | the AIFF reader; the DS MPE map | answered in Part VI (`DECISIONS_6 #14`, the preset's own map) — the sound guide and the chart's Voxo section |
   | SYNTH (§13) | the fold's ×4 oversampling budget | carried — documented on the cells chapter |
   | SYNTH (§13) | the lattice's coupling topology | resolved in `DECISIONS_7 #10` (a nearest-neighbour chain) — the modal chapter |
   | SYNTH (§13) | "sustained small drive into damped modes" | NOT built — documented in the Suzu index's MPE table (the bow is the press consumer where there is breath) |

2. **The Suzu lab deploys INSIDE the marble tree, at `/marble/suzu/`, built
   from the same tag as `/marble/`; the docs embed it through `<Lab>`, and
   the docs' new demos resolve only once the marble at that address knows
   them.** The drafts and the lab's `DOCS_ROOT` had assumed `/suzu/` beside
   `/marble/`; the docs' own Suzu book wants `/suzu/` for its chapters, and
   the lab is engine code (a wasm and an AudioWorklet), which the drift check
   forbids inside the docs build. So: `pages.yml` copies each tag's
   `build-web/suzu-dist` to `marble/<tag>/suzu/` when the tag has one (the
   cache key bumped so cached trees gain it), `compose-marble.mjs` does the
   same for the preview from `SUZU_DIST` (default `../build-web/suzu-dist`),
   `web/suzu/site/lab-core.js` sets `DOCS_ROOT = '../../'`, and `check.mjs`
   admits exactly one other iframe target than a scene — `<marble>suzu/<page>.html`
   — requiring every page the web build serves (`web/suzu/site/*.html`,
   index aside) to be embedded once, as it requires every scene. The
   seventeen scenes (the roadmap counted sixteen: the eleven of 1.0 and
   the six of Phase 6) are all required now. THE TIMING, flagged for the
   author (the roadmap's "build against the RC; merge with the stable
   tag", `DECISIONS_4 #82`): the site deploys from `main` and `/marble/`
   is rebuilt from the newest STABLE tag, so between this step's merge and
   the `v2.0.0` tag the live site's new demos and lab panels would point at
   a 1.0 marble that knows neither. Two ways, the author's call: land this
   step with the stable tag, or let `pages.yml` build the docs with
   `PUBLIC_MARBLE_URL=/marble/rc/` while an RC is newer than the stable
   (every demo then runs on the RC — a change to #82's policy, not made
   here). THE AUTHOR'S CALL (2026-10-06, on seeing the book): the tag is
   **the first v2 release candidate**, cut so the testers can proofread
   the docs live — so the second way is BUILT: `pages.yml` builds the docs
   with `PUBLIC_MARBLE_URL=/marble/rc/` and `SITE_VERSION` = the RC while
   an RC is newer than the stable, and with `/marble/` and the stable
   otherwise; `/marble/` itself stays the stable (#82's discipline holds;
   only the docs' embeds move, and the 1.0 scenes run on the RC meanwhile
   — the RC is a superset). The lab's `DOCS_ROOT` is derived from the
   page's own address (whatever precedes its `/marble/` segment —
   `/marble/suzu/`, `/marble/rc/suzu/`, a project-site base alike; two
   levels up when served alone). The tag must be cut AFTER this step's
   commit: the workflow builds the marble tree from the tag's own sources,
   and the bumped cache key then builds the tag's `suzu-dist` into
   `/marble/rc/suzu/`. The gallery's 2.0 cards render as "coming soon"
   (the replay link appears only once a card has its video; a pending card
   says its recording will replay), and six more pending cards name the
   captures the author will record — the theremin, Wicki–Hayden, the
   trombone with the S Pen, the flute's overblow, the Chladni vibrato, a
   replay across three devices — their captions placeholders, as #4's.

3. **The chart gained what Phase 7–9 made it owe, verified: two output
   sections from the step-63 iPad logs and the sound engine's own input
   section.** `tests/fixtures/bytelogs/trumpet_byte_log.csv`,
   `trombone_byte_log.csv` and `theremin_byte_log.csv` are the step-63 lab
   logs (`git show f2916db:docs/evidence/step63/ipad/…`); the chart's
   "Output · The valves (src 3)" (CC 110–112 present, the announce's nine
   conditional) and "Output · The slide (src 3)" (CC 113 present, 42
   messages) verify against them in `tools/chart_check.py` — 52 checks
   green, the strip section's rows unchanged. The input side: CC 110–113,
   CC 120/123 and CC 122 rows in the MPE section; a new "Input · the sound
   engine (Voxo)" section (verified by the Voxo and Suzu suites, not a
   log) naming every consumer — CC 74, CC 1, CC 2/11, CC 64, 120/123, 122,
   the bus targets 1000–1005; the CC-map row names the Phase-6 dimensions
   and the desktop's 104–109. The theremin log is kept as a fixture for
   the next analyser (its bends and pressures are the finger section's
   rows already).

4. **The gallery's three new cards are PENDING with a `replay` field each —
   the recordings and the captions are the author's.** The roadmap names
   the three performances (an Anod piece, a trumpet piece, a bowed Suzu
   piece with the orbit trace); the cards are in `gallery.json` with
   `replay` paths under `/gallery/` (`anod-discharge`, `trumpet-arc`,
   `bowed-trace` `.sumireplay`), the device, synth and modes written as
   PLACEHOLDERS for the author to correct when the pieces are recorded —
   "after I show the book", the author's word. `Gallery.astro` renders the
   field as "watch it again" (`<marble>?replay=<file>`); `check.mjs`
   checks the field's form and reports a missing file as pending rather
   than failing. The burst page's author note is the roadmap's text,
   signed "— the author": the name is the author's to put.

5. **The settings reference is rewritten from the desktop window's sections
   in their order; the citations gained the electric medium's and the
   synth's papers; the architecture book gained the replay page.** The
   reference lists every section the desktop window has (Canvas, Layout &
   look, Prints, Presets, Replay, Substrate, Palette, Medium, Expression
   routing, the five operator sections, CC map, MIDI inputs, Sound, Suzu
   trace, Window, the bench, About) with each row's params field or CC,
   and the tablets' pages folded in; the 1.0 rows that still hold were
   kept word for word. Citations: Chladni, Taylor–Green, Aref and Ottino,
   Chirikov, Greene, Moser, Meiss, the viscous multipoles (Voropayev &
   Afanasyev, Chan & Chwang, Gallay & Wayne) with the literature-check
   sentence; Benade, Fletcher & Rossing, Karplus–Strong, Chamberlin,
   Gordon–Smith, Verlet; the Decent Sampler format's author; the thanks
   with Professor Jaffer first and Ichisuke Fujioka honoured. The Suzu
   book names the cell the Gordon–Smith oscillator and carries the archived
   charts as figures (mode splitting, the rotor's sweep, the Duffing
   clang and drive, the flute's and the sax's ramps, the trumpet's lips),
   the rotor's sweep also on the Chirikov page — one theorem, two senses.

6. **Every scene carries the medium toggle, and it is permanent: the
   medium is set before the fresh sheet's first drop, never switched at
   the end.** The author, on seeing the book: "the anod toggle should
   exist for all the operators and it should be permanent — not start with
   Sumi and toggle to Anod at the end of the replay; it should start with
   Anod if the toggle is 1." AS BUILT (`web/site/scenes.js`, a page
   change, no core touch): one `MEDIUM` slider (`M`: 0 Sumi, 1 Anod,
   default 0) spliced before `⏱` into every scene's params, and every
   scene's `setup` wrapped so the medium is set first — the runner dips,
   the medium lands, then the clusters — so `?medium=1` on any scene's
   embed runs the whole script as a discharge. The Anod scene defaults to
   1, sets its look (palette, glow, grid) before the script and no longer
   switches at the end; its caption says so. The docs' embeds keep their
   defaults (ink for the operators' pages, Anod for the Anod page); the
   scene sweep and three captures (Anod, torsion and the spark under
   Anod) are in the evidence.

7. **The Windows lane of the first v2 RC was red: MSVC has no
   `clock_gettime`; the deviceless Voxo backend reads the performance
   counter there.** `voxo/src/backend_none.cpp` — compiled on every
   platform into `voxo_nofma`, the no-FMA reference the web gate compares
   against — took the monotonic clock through `clock_gettime(CLOCK_MONOTONIC)`,
   which the macOS, Linux, Android and wasm toolchains have and MSVC does
   not (`C2065 'CLOCK_MONOTONIC'`, `C3861 'clock_gettime'`, the RC's
   `release.yml` log). Fixed with a `_WIN32` branch on
   `QueryPerformanceCounter` / `QueryPerformanceFrequency` (the same
   monotonic clock miniaudio's backend uses on Windows), the other
   platforms untouched. Not verifiable on this Mac: `build.yml` runs the
   Windows lane on every push to `main`, so the push carrying this fix
   proves it before the next RC is cut; the MSVC `C4996` warnings on the
   replay library's `sscanf` are warnings and stay. The RC tag that failed
   is the author's to replace. THE LANE FAILED AGAIN on the next run (the
   log not seen by this session): the work moves to the Windows box —
   `_work/HANDOFF_WINDOWS.md` (reproduce, read the first error, fix within
   the rules, ctest, the release configure's version check, then the D3D11
   gates: field at the second tier, composite, replay with #7's dip caveat,
   the storm); its record is `#8`, the Windows session's. A second hand-off
   written at the author's ask, for a later session:
   `_work/HANDOFF_SUZU_VCSL.md` — fit Suzu's patches to the Versilian
   Community Sample Library's instruments (CC0), with the measuring and
   fitting tools, the side-by-side profiles and the questions the author
   settles first (which instruments, presets in code or editable tables,
   defaults or beside them, where in the roadmap). The install page's
   "makes no sound of its own" — a 1.0 sentence — is corrected: the app
   sounds since 2.0; the browser is the picture alone.

8. **The Windows lane's second red, read on the Windows box: three MSVC
   stops after the clock — an else-if chain past the compiler's nesting
   limit, a GCC attribute on the web reference's exports, `M_PI` in the
   Suzu suite — fixed portably; the lane's steps and the D3D11 gates
   green.** The step-67 tree (`d7d9126`, with #7's performance-counter
   clock) built on the box with MSVC 19.44 (the lane's is 19.51), Release,
   `ninja -k 0` to see every stop at once (`docs/evidence/step67/windows/
   build_log_tail.txt`). Three, none in `core/`: (1) `desktop/src/
   app_settings.cpp(491): fatal error C1061: compiler limit: blocks nested
   too deeply` — the INI loader's one `else if (k == …)` chain had grown to
   134 keys with Suzu's knobs, and MSVC counts each `else if` as a nested
   block against its limit of 128; the chain now restarts as a second
   `if` at the first `suzu_*` key (the keys are distinct, so two chains
   read as one and an unknown key still falls through both). (2) `web/
   suzu/suzu_web.c(101)`: `SW_EXPORT` was `__attribute__((used,
   visibility("default")))` unconditionally — emscripten's and GCC's
   keep-alive for the worklet's imports — and MSVC has no `__attribute__`;
   the macro is the attribute under `__GNUC__`/`__clang__` and empty
   elsewhere (the native reference `suzu_web_reference` is linked
   statically, so the plain declaration is the export). (3) `tests/
   voxo_suzu_tests.cpp(164)`: `M_PI`, which standard `<cmath>` does not
   define and MSVC provides only under `_USE_MATH_DEFINES`; the test
   defines it when absent. After the three the build is clean; the
   warnings are MSVC's deprecation notes (`C4996` on `strcpy`/`sscanf` in
   the replay and preset tests and `sumi_replay.c`), two `C4127` in the
   ABI test, and libremidi's — nothing builds with `/WX`. **ctest 12 of
   12** (the ABI tests at 1.5.0); the first pass had `voxo_tests` die with
   a SegFault at 2.75 s while the release configure compiled beside it —
   eight direct reruns, `--repeat until-fail:6` and a second full pass on
   the idle box all passed (0.12 s each), so it is recorded as a one-off
   under load, not reproduced. **The release lane's configure**
   (`-DBUILD_TESTING=OFF -DSUMI_APP_VERSION=2.0.0-rc.2`) builds 99 steps
   and `midi-sink.exe --version` prints `midi-sink 2.0.0-rc.2 (commit
   d7d9126, libsumi 1.5.0)` — About reads the tag. **The gates on D3D11**
   (NVIDIA RTX 5090, driver 616.64): the §4.6 field gate green at the
   reference tier, not only the second — dx 2.44e-4, dy 3.66e-4, ink
   3.91e-3, aux 0, mean 2.26e-6, the step-55b numbers to the digit
   (DECISIONS_7 #7); the composite gate within its tier of one step
   (25 617 samples differ); the replay gate GREEN — the bench's canonical
   demo (480 frames, 794 events) recorded through the real recorder on
   this box replays **bitwise** (every channel 0, mean 0) and the 60 Hz
   wall-time re-bucketing diverges (ink max 1.41, mean 3.0e-3) — #7's
   variable dip did not reach it, since the recording and the replay
   both start from the same dip on the same box; the storm with the Dan
   Tranh: 0 XRuns, 0 dropped, 164.6 fps, render max 0.463 ms, 480 frames
   per block (DECISIONS_6 #39). The live round: `--record-live 5` saved
   824 frames / 5.0 s of loopMIDI phrases, `--replay-live` played them
   back with the banner; in the settings window the Replay section's
   Record and Stop recording were pressed by hand (a 97.9 s recording of
   16 123 frames saved and listed in the picker); the Play button and the
   iPad recording were not exercised — the author was working on the box
   and the agent stopped injecting input (the player itself ran through
   `--replay-live`); no `.sumireplay` from the iPad is on the box. The
   warnings stay as they are. Evidence `docs/evidence/step67/windows/`.

9. **Suzu patches fitted to the Versilian Community Sample Library (CC0);
   fingerprinting, fitting and comparison tools in tree.**
   (`_work/HANDOFF_SUZU_VCSL.md` delivered.)
   * **Measurement:** `tools/suzu_fingerprint.py` measures instrument
     samples (SFZ and `.dspreset`), extracting f₀, partial ratios r_k,
     strike weights w_k, inharmonicity B, per-partial T60, attack times,
     noise floor, and sustained spectra; renders 3-panel profile figures.
   * **Fitting:** `tools/suzu_fit.py` maps fingerprints to Suzu presets
     (`presets/SCHEMA.md`), fitting modal ratio presets 0–4 (Dan Tranh,
     Glockenspiel, Tubular Bells, Concert Harp), breath bow sustains, and
     acoustic bore winds (Tenor Saxophone, Baroque Recorder).
   * **Comparison & Verification:** `desktop/src/dev_tools.cpp`'s
     `--voxo-profile` accepts `--preset <file.json>` with Suzu blocks;
     `tools/suzu_compare.py` renders side-by-side profile overlays with
     quantitative metrics (harmonic spectral distance in dB, peak error,
     T60 tracking error).
   * **Presets & Gates:** Saved under `presets/*_vcsl.json`; factory
     defaults preserved so `suzu_web_gate.mjs` bit-for-bit reference is
     unaffected; all fitted patches verified through `tests/voxo_suzu_tests.cpp`
     (Gate 27, all gates green); Suzu book's preset table updated with
     VCSL references; evidence under `docs/evidence/step67/suzu_vcsl/`.
   * **Suzu Lab Audition:** The six VCSL presets are exposed in the Suzu Lab
     web interface (`web/suzu/site/`): `modal.html` carries the 4 modal/plucked
     instruments with responsive sliders, `flute.html` gains the Baroque Recorder
     preset, `winds.html` gains the Tenor Saxophone preset, `strings.html` gains
     Dan Tranh and Concert Harp under its modal pluck kind, and `index.html`
     carries a VCSL showcase with direct deep links (`?preset=`). All headless
     browser gates (`suzu_lab_gate.mjs`) and the web reference gate (`suzu_web_gate.mjs`)
     remain 100% green.
