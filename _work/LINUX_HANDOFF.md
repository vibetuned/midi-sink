# Handoff — Linux box: Step 33 verification (Linux desktop + the Android build)

Written on the macOS machine. You are the agent on the author's Linux box
(Ubuntu 25.10, GNOME on Wayland, NVIDIA RTX 5090, Android Studio + NDK, the
Galaxy Tab S8 Ultra over adb, the DAW end of the tablet transports). Your own
steps, **Step 30 (Linux release lane) and Step 31 (Android release
procedure), are DONE** — `docs/evidence/step30/`, `step31/`, DECISIONS_4
#44–#48. What is left of them is the author's (the tag, publishing the draft,
the Play internal track, the signed bundle) — leave those alone.

This handoff is **Step 33 on this box**: ten fix batches landed on the Mac.
The desktop ones need verifying on Linux (X11 and Wayland). The **Android
ones were written on the Mac and never compiled** — that is the main job:
build the app, run it on the Tab, fix what does not compile or behave, record
it. Do not touch the macOS, Windows or web lanes.

## Read first, in this order

1. `CLAUDE.md` — the working rules. **Agents never commit** (prepare
   evidence, report "tree ready"); **agents never edit specs or roadmaps** —
   new decisions go to `_work/DECISIONS_4.md` as numbered entries (**next
   number: 73**; the Windows machine is done — #67–#69 are its), and where a
   spec line and reality disagree
   you flag it in the entry.
2. `docs/evidence/step33/SUMMARY.md` — batches 1–11 plus the Windows
   verification: what each changed, how it was verified, which Android files
   are uncompiled.
3. `_work/DECISIONS_4.md` #49–#72 — the record of what shipped. Android is
   named in #49, #53, #54, #56, #60, #64, #65, #69, #71; the desktop in
   #57–#58, #67, #70, #72.
4. `android/RELEASING.md` and `android/prepare_release.sh` (yours from Step
   31) — the build entry point; `docs/evidence/step28/` for the iOS twin of
   every Android change (the iOS code compiled and ran; when in doubt, mirror
   `ios/Sources/SumiCanvas.swift` / `SumiApp.swift`).

## What changed since Steps 30–31 (all of it on the Mac)

* **Core ABI 0.5 → 0.9.0** — rebuild everything from scratch, desktop and
  the JNI library alike (the JNI lib builds the repo-root CMake through
  Gradle's `externalNativeBuild`). `sumi_params_t` grew (`wake_profile`,
  `wake_spread`, #53); `sumi_layout_t` grew two rolls (#64); new drop layer
  and vortex profile enums (#49); `sumi_ctl_t` grew the swirl trio and two
  pinches (#69, from the Windows box). Desktop: `cmake -B build -G Ninja &&
  cmake --build build && ctest --test-dir build` — four suites, 18 455 mapper
  checks; the §4.6 field fixture must still hold **bitwise** on the NVIDIA GL
  (`tools/field_gate.py` defaults).
* **Behaviour every platform shares:** MPE mode gives a channel-1 keyboard
  per-note voices (#60); the palette-morph CC travels the ring (#61);
  **CC 64 never dips the paper in any mode** (#62); wind mode is MPE plus a
  wake between notes, breath unbounded, the wandering brush is gone (#63);
  the bend-driven ripple is 4× more sensitive (#66); the Airwave map is
  symmetric hands — Raise / Glide / Slide per hand, left the vortex, right a
  Lamb-Oseen swirl, Grasp the pinches, Tilt the ripple (#69) — and a
  persisted map that is an older stock map upgrades itself on load (#71).

## Checklist A — Linux desktop

1. **Build + suites + field gate** (above), then `--dev --pressure-test`
   (6/6), `--stokeslet-test` (4/4), `--ripple-group-test`,
   `--ripple-permanence-test`.
2. **Fullscreen (#58).** Settings › Window › Fullscreen and F11 on both
   sessions: fills the monitor, comes back to the same windowed geometry;
   `--fullscreen` writes `fullscreen=1` to `~/.config/midi-sink/settings.ini`;
   `--window 1920x1080` (#57) opens exactly that.
3. **Input mode (#60/#62/#63)** with the ROLI over ALSA and any plain
   keyboard: MPE (default) — a channel-1 chord paints one drop per note,
   CC 64 does nothing; Classic — per-note voices, bend = global shear; Wind —
   one voice, legato drags the sounding drop to the next note with a wake and
   CC 2 grows it. Persisted as `input_mode=` in the INI.
4. **Layouts (#64).** Eight in the combo; the four Piano rolls (left / right /
   top / bottom) scroll away from their now-line; tempo and roll rows show for
   all four.
5. **Gestures.** Shift + right drag (press: hold/push up = feed, pull back =
   swirl), middle-drag wake with Stylus wake = Viscous stroke, right drag with
   Vortex profile = Rankine.
6. **Packaging unaffected?** `cmake/LinuxPackaging.cmake` did not change; one
   local CPack DEB, install into a clean container, `--version`, launch — to
   be sure nothing in the new flags upsets the `.desktop` launch. The canvas
   keeps its title bar on Linux as everywhere (#70) — nothing to judge there.

## Checklist B — Android (build first; this code has never compiled)

Files written on the Mac, by batch. Build in Android Studio (or
`./gradlew assembleDebug`), fix compile errors in place, then verify on the
Tab. Every behaviour has an iOS twin that runs — mirror it when the intent
is unclear.

| Batch | Files | What to verify on the Tab |
|---|---|---|
| #49 pressure gesture | `MainActivity.kt` (`SumiSurfaceView`: 250 ms long press, `Choreographer` tick), `NativeBridge.kt` + `sumi_jni.cpp` (`nativeAddFeed`, `nativeAddSwirl`) | Long press lays a drop; hold / push up grows it, pull back swirls; no drop on lift after a press; tines and two-finger gestures unchanged; `ACTION_CANCEL` clears the press |
| #53 stylus wake fluid | `nativeSetWakeProfile` (JNI), the **Stylus wake** rows in the sheet (`wakeViscous`, `wakeSpread` prefs) | Flipping to Viscous stroke changes the pen wake's look; the spread row moves it; persists |
| #54 S-Pen in Marble mode | `SumiSurfaceView.handleStylus` / `stylusLast` | A pen stroke in Marble mode draws the wake (tip from pressure), no tine, no drop on lift, no long-press; eraser end = tip; fingers unchanged |
| #56 settings parity | `sumi_jni.cpp` (`nativeSetLook`, `nativeSetVortexProfile`, `nativeSetRippleAngle`, `nativeSetCcMap`, `nativeSendCC`, `apply_cc_map`, the wider params seed/apply, the `cc_replay` at create), `sumi_play.cpp` + `shell.h` (`play_send_cc`), `NativeBridge.kt`, `MainActivity.kt` (prefs, `SettingsDialog` rows, `StepRow` / `CycleRow`, the `CcMap` object) | Palette switches the ink colour at once; viscosity / ink feed / roughness step rows move the look; tempo / roll rows appear on the four rolls; Vortex row flips the two-finger twist diffuse ↔ rigid; Ripple rows move the shimmer and ride CC 102/103 (`midi_log.csv` shows them as source 2); removing the CC 102 route hides the Amount row; Restore default map brings the Airwave routes back; kill + relaunch — everything persists and the ripple values are re-sent |
| #60 input mode | `nativeSetInputMode` (JNI, applied at create and on change), the **INPUT** rows, `inputMode` pref | With a channel-1 keyboard over USB: MPE (default) plays chords; Classic per-note voices; Wind with a breath controller: one voice, a wake from note to note, CC 2 grows the drop; CC 64 does nothing in any mode (#62) |
| #64/#65 layouts | The eight-entry layout list, `if (id in 0..7)`, JNI range `0..7`, the three lattices labelled "(playable)" | Piano roll (right) and (bottom) scroll from their edge; tempo rows show for all four rolls; the labels read "(playable)" |
| #69/#71 CC map | `CcMap` in `MainActivity.kt`: the 14 dimension names, the #69 defaults, `olderDefaults` in `decode` | With the Airwave over USB: Raise L stirs, Raise R swirls, Grasp folds, Tilt ripples, Flex does nothing; the CC map rows show the new names; an install that ran before #69 (stored old stock map) comes up on the new routes without "Restore default map" |

Then the on-device suites (`--es hostmpeTests 1`, unchanged by these batches)
and the Play-mode byte-log asserts (`tools/midi_asserts.py`) once, to be sure
the merge point still holds with the new `play_send_cc` path. USB-MIDI to this
box: `amidi -l`, `build/tests/midi_capture_alsa --match SAMSUNG`.

**Android Studio / Gradle:** the versioning from Step 31 (#47) is untouched;
`prepare_release.sh` still prints the tag values. No Android CI job, by the
author's decision (#47).

## Reporting

Append **"Linux verification (Step 33)"** and **"Android verification (Step
33)"** to `docs/evidence/step33/SUMMARY.md`: a table per checklist (PASS /
FAIL / YOURS with the file that proves each), raw outputs beside it
(`ctest.log`, `field_gate_gl.txt`, screenshots — X11 at least, the Wayland
capture problem from Step 30 stands —, the INI, `midi_log.csv` from the Tab,
the Gradle build log). Fixes go in the shells (Kotlin, JNI, the GLFW harness),
never the core, each with a DECISIONS_4 entry from #67; if a core fix is
unavoidable, describe it and stop — the author decides. The author commits;
end with "tree ready" and the evidence path.
