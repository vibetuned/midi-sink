# Handoff — Linux box: Step 45b (the Android shell for Phase 6)

Written on the macOS machine, 2026-09-23. You are the agent on the author's
Linux box, Android side (Android Studio + NDK, the Galaxy Tab S8 Ultra over
adb). **Do step 45a first** (`_work/LINUX_HANDOFF.md`: the Linux desktop
proves the Phase-6 core on GL) — this step then brings the Android shell to
where the iPad shell is after step 44a. Do not touch the macOS, iOS,
Windows or web lanes; the core is shared — a core fix must keep Metal, GL
and the web bitwise where they were.

> Step numbering: `_work/ROADMAP_5.md` calls this "Step 45 — Android shell";
> the author's numbering is **45b**. Evidence in `docs/evidence/step45b/`.
> Roadmap text: "First line of the step: the step-41 JNI update compiles
> and the app runs. DONE when: as 44 on the Galaxy Tab, plus the 16 KB
> page-size build still passes Play's check." Step 44's DONE: every new
> setting is reachable on the tablet; a preset made on the desktop imports
> and renders the same (screenshot compare); the byte path is untouched (the
> Phase-4 tests still pass).

## Read first, in this order

1. `CLAUDE.md` — **agents never commit, stage or push**; **agents never edit
   specs or roadmaps**; temporary verification hooks are removed from the
   tree; version strings come from the tag.
2. `docs/evidence/step44a/SUMMARY.md` and `_work/DECISIONS_5.md` **#73** —
   the iPad shell's Phase-6 work, which is your blueprint: one session model
   persisted through the preset serializer, the settings pages, the print
   ledger, the dip / clear copy, the play surface following the medium.
   Then **#74** (the web page's JNI-friendly preset shim — the closest model
   for Android) and **#75** (the medium-aware gestures).
3. The iOS sources to mirror: `ios/Sources/Session.swift`,
   `PrintLedger.swift`, `SettingsPages.swift`, `SumiApp.swift`,
   `SumiCanvas.swift` (`applySession`, `paperDip` / `clearCanvas`,
   `exportPrint`, the gesture handlers), `PlayOverlayView.swift` and
   `ControlStripView.swift` (`setDarkTheme`). And `web/sumi_web.cpp`
   (`sumi_web_preset_*`, `sumi_web_palette_*`) for a flat C shim that a
   JNI layer can copy.
4. `_work/DECISIONS_5.md` #45 and #49 (the 1.0.0 break: the Android JNI was
   edited on the Mac and **never compiled**), #50–#72 for what the medium
   does, and `android/RELEASING.md` / `android/prepare_release.sh`.
5. New decision entries are numbered after whatever the Linux (45a) agent
   wrote — at the time of writing the next free number is **#76**.

## State of the Android tree

* `android/cpp/sumi_jni.cpp`, `sumi_play.cpp`: the probe calls pass
  `nullptr` for the 1.0 state argument; otherwise the JNI is the 0.x shell —
  per-field setters (`nativeSetLook` clamps the palette to 0..2, `bend_mode`
  0/1 only, the press gesture's feed / swirl math inline), no medium, no
  palette, no presets, no ledger, and the marble gestures call
  `sumi_add_drop / _pinch / _vortex` directly.
* `android/cpp/CMakeLists.txt` links `sumi_static`, `hostmpe`, EGL, android,
  amidi, log — **not** `sumi_presets` (the root CMake already builds
  `presets/` on Android; add it to `target_link_libraries`).
* Kotlin: `MainActivity.kt` (the Compose settings dialog: CANVAS, LAYOUT &
  LOOK, MODE, CONTROL STRIP, INPUT, NOTE BEND, PRESSURE, SLIDE, VORTEX,
  STYLUS WAKE, RIPPLE, CC MAP, MIDI, OUTBOUND, SESSION, ABOUT),
  `NativeBridge.kt`, `PlayOverlayView.kt`, `ControlStripView.kt`, the MIDI
  and BLE files. The debug intents (`--es fieldDump 1`, `--es hostmpeTests
  1`, `--ei stressMinutes N`, `--ei layout N`) are in `handleDebugIntent`.

## The work

1. **First line: it compiles and runs.** `android/prepare_release.sh` (or
   Gradle) against the 1.1.0 core; fix the mechanical fallout; the app runs
   on the Tab; the on-device suites via the debug intent (`hostmpeTests`)
   pass; the §4.6 field dump (`fieldDump`) against the Metal fixture at the
   GLES tier (`build/tests/field_dump_compare` on the desktop). The Phase-6
   shaders are compiled to `glsl300es` and have **never run on GLES**: the
   bloom's RGBA16F half-res targets, the Chladni RGBA16F index map, the
   export target — watch the log for sokol validation errors (binding-name
   collisions across shaders were the Mac's trap, #69).
2. **One session (as #73).** Replace the per-field setters with one session:
   a `sumi_params_t`, the custom palette, the CC map mirror, the input
   dialect, the six routed controls (ripple 7/8, Chladni 16/17, spark 18,
   Chirikov 19 — sent as their CCs through the sole MIDI producer), the
   strip's two wheel assignments. Persist it through `presets/`
   (`last_session.json` in `filesDir`, restored at launch; named presets in
   a `Presets/` folder); take the core's defaults right after
   `sumi_create`; migrate the 0.x SharedPreferences rows once (only rows
   that exist; an untouched bend lands on "Medium default"). The web shim's
   shape (`sumi_web_preset_capture / _add_cc / _add_control / _write /
   _read / _apply` and the getters, the palette as 45 floats) is a good
   JNI surface — copy it into `sumi_jni.cpp`.
3. **The settings (mirror the iPad's pages and names):** Canvas ("Dip the
   paper — keep the print" / "Clear the canvas — discard", Prints), Medium
   & look (the medium; Palette — built-ins, library, "Load into custom", the
   stop editor, depth curve / floor, drift and its colour, clear water;
   Substrate — tint and paper presets, roughness, fiber scale in Sumi;
   glass darkness, grain, bloom, reach, glow scale, grid lines, strike
   charge in Anod; Presets — save, load, delete, share, import / export via
   the Storage Access Framework; Operators — Chladni, burst, spark,
   Chirikov), Expression routing (the three modes with "Medium default"
   first and every 1.1 value, the vortex's three profiles, the torsion
   sweep), the CC map's targets 14–19 with the desktop's names and default
   handles CC 104–109 (a stored 0.x default map reads as today's).
4. **The print ledger (as the iPad's):** each dip keeps its field
   (`sumi_read_field`), params and palette; thumbnails when the print
   lands; re-export at Screen / 2K / 4K / 8K and Anod over alpha through
   `sumi_export_begin / _poll`, written as PNG and offered through a share
   intent / MediaStore; a clear's print is read and dropped on arrival;
   six entries or 256 MB.
5. **Gestures (#75):** the marble tap, pinch, twist, long press (and its
   first touch) and the stylus pinch go through `sumi_gesture_tap / _pinch
   (span = finger distance in canvas heights) / _twist / _press (returns R)
   / _press_end`; remove the press's feed / swirl constants from the shell.
6. **The play surface on the glass (#73 addendum):** in Anod the joysticks,
   lattice, echo highlight and hover ghost draw white over a dark halo, and
   the control strip turns to translucent dark with white marks.
7. **16 KB pages:** the release build still passes Play's 16 KB page-size
   check (the procedure in `android/RELEASING.md`).

## Evidence (as 44a did it)

* A **preset made on the desktop**: `docs/evidence/step44a/desktop-anod.json`
  pushed into the app's Presets folder with adb, loaded, written back —
  **byte-identical** (the iPad and the web page are).
* **Screenshot compare:** the same preset + the same six strikes (notes 60,
  66, 62, 68, 64, 70 on channels 2–7, velocity 100, 0.15 s apart, 1.25 s,
  note-offs, dip) printed by the desktop harness at the Tab's canvas size
  and by the Tab; compare glass level, glow colour and where the strikes lie
  (44a used a scratch Python PNG reader; strokes differ by the frame clock,
  the look must not). 44a did this with temporary hooks on both sides — an
  app launch argument and a desktop `--preset-render`; recreate them, take
  the evidence, remove them.
* The reachability table (every Phase-6 setting → where it lives), the
  on-device suites, the 16 KB check, and your screenshots.

`docs/evidence/step45b/SUMMARY.md`, decision entries for what you resolved,
"tree ready" with a commit reference. Installing on the Tab is fine; store
uploads and track promotions are the author's.
