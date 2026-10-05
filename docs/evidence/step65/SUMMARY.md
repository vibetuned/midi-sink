# Step 65 — Session replay: the byte stream is the recording — the evidence

`DECISIONS_8 #22` (the file and the recorder), `#23` (the player), `#24` (the
gates and the measurement). Spec: QOL §1 (the undone spec's §1, "session
replay"), SOUND §5 (`TO_PROJECT_SPEC.md` §12.5, replay re-sounds); the
roadmap's step 65. Machine: the author's Mac (Mac16,5, Metal), the iPad
(iPad16,8, iOS, Metal, 60 Hz) over devicectl. The Linux box is out of
commission (the author's word); the Tab was not connected at the close. No
core change: libsumi stays 1.5.0; the Metal fixture untouched.

## What changed in the tree

* `replay/` — NEW, the sibling library beside `presets/`: `include/sumi_replay.h`
  (the API and the thinking), `src/sumi_replay.c` (the recorder with its
  wait-free stage, the writer, the reader, the player, the wall-time
  re-bucketing), `src/sumi_replay_apply.c` (the one unit that links the core
  and the presets: a frame onto an instance), `include/module.modulemap`
  (`import SumiReplay`), `FORMAT.md` (the file), `CMakeLists.txt`; the root
  CMake adds it on every native platform; `tests/replay_tests.c` (47 checks)
  in ctest.
* Desktop: `replay_host.{h,cpp}` NEW (the recorder and player behind the
  settings and the loop: the gesture wrappers, the staged harness, the
  playback pacing, Metal's display sync, the banner, the files); `main.cpp`
  (the gestures through it, the loop's record/playback branches, the
  settings apply during playback, the lab flags), `midi_harness.{h,cpp}`
  (`set_stage`, `set_muted`), `print_ledger.{h,cpp}` (the dip hook),
  `orbit_trace.{h,cpp}` (the gesture hook), `metal_layer_glue.{h,mm}`
  (display sync), `sys_info.{h,cpp}` (the machine name), `app_settings.{h,cpp}`
  (`app_settings_session_json`, `app_replays_dir`), `settings_ui.{h,cpp}`
  (the Replay section), `dev_tools.{h,cpp}` (`--record-demo`, `--replay`,
  `--replay-wall`, `--replay-wav`, `--replay-warmup`, `--record-live`,
  `--replay-live`).
* iOS: `Replay.swift` NEW (the status the sheet and the banner watch, the
  files under Documents/Replays, the field dump, the Replay page);
  `SumiCanvas.swift` (the stage in `push`, the frame in `tick`, the gesture
  wrappers, record/stop/play/stop, the lab recording and the lab replay),
  `Session.swift` (`text(of:)`), `SumiApp.swift` (the banner, the Replay
  row, `--record-lab`, `--replay-file`, `--replay-dump`), `project.yml`.
* Android: `sumi_jni.cpp` (the stage in `push_midi`, the frame, the gesture
  posts, the state event in `apply_session`, the ledger dip, the six JNI
  entry points, the lab size), `sumi_play.cpp` / `shell.h`
  (`play_post_resync`), `NativeBridge.kt`, `SettingsSheet.kt` (the Replay
  page and the host interface), `MainActivity.kt` (the host, the banner, the
  export/import pickers, `--es recordLab`, `--es replayFile`).
* `tools/replay_gate.py` NEW; `tools/README.md`; `docs/CHANGELOG.md`;
  `CLAUDE.md`; `_work/DECISIONS_8.md` #22–#24.

## The Mac's gates (`mac/`, `gates/mac_self_report.txt`)

`midi-sink --dev --record-demo mac_demo.sumireplay` — the canonical
performance through the real recorder (512×320 @1, sim_scale 1, 120 Hz, 4 s,
480 frames, 794 events; the file is `mac/mac_demo.sumireplay`, its field dump
3.5 MB stays out of the tree — the gate re-makes it in one command). Then
`tools/replay_gate.py`:

| run | result |
|---|---|
| `--replay` → the field after vs the recorded dump | max 0, mean 0 — BITWISE |
| `--replay --replay-wall 60` (241 frames for 480) vs the recorded dump | max 1.41 (ink), mean 3.0e-3 — diverged, as required |
| Debug bench (`build/`) vs Release bench (`build-universal/`) | bitwise |
| `--replay-warmup 600` (the clock at 5 s before the replay) vs none | bitwise |
| `--replay --replay-wav mac_demo_suzu.wav --voxo-source suzu` | 192 000 frames (4.00 s) rendered offline, peak 0.089 — `mac/mac_demo_suzu.wav` |
| `--record-live 3 --voxo-storm 2 --exit-after 5` | a 3-s live recording: 344 frames, 4 748 staged bytes, 6 481 orbit-trace segments, one dip, 0 dropped |
| `--replay-live <that file> --exit-after 6` | played in the interactive loop, the harness muted, 81 fps with display sync off |

The first cut of the demo ended with gestures only after its dip, and the
re-bucketed replay came back bitwise: gesture passes replay as the same
sequence under any frame grouping; the mapper's per-update smoothing and
feeds are what the frame field protects. The demo's tail became a held note
bending, pressing and sliding every frame (#24).

## The iPad (`ipad/`, `gates/ipad_series_report.txt`, `gates/ipad_to_mac_debug_report.txt`)

`--layout 8 --play --fingering-demo --record-lab 20`: the lab's small field
(640×445 @1, sim_scale 1), the recording for 20 s with the fingering demo's
phrase inside (`ipad/lab.sumireplay`: 1 223 frames, 132 bytes, one dip,
20.45 s), the field after the last frame beside it (4.3 MB, out of the tree).

| run | result |
|---|---|
| the iPad replaying its own recording (`--replay-file lab.sumireplay --replay-dump`) vs its recorded field | max 0, mean 0 — BITWISE |
| the iPad's recording replayed on the Mac vs the iPad's field | max 5.5e-2 (dy), mean 8.2e-3 — outside the tier |
| the same cut at 180 / 420 / 800 frames (the iPad's own replay vs the Mac's) | mean 9.5e-3 / 8.2e-3 / 8.2e-3 — accrued while passes ran, frozen after |
| the Mac's demo replayed on the iPad vs the Mac's field | max 1.9e-2, mean 4.8e-4 |
| the iPad's recording re-bucketed at 60 Hz on the Mac | max 0.16, mean 8.6e-3 — diverged |

Per channel (the full recording): ink max 2.9e-3 on 6 % of texels (the phase
bands are piecewise constant; a shift shows only at their edges), aux 0, dx
max 2.5e-2, dy max 5.5e-2 on 99 % of texels, a smooth few percent of the
displacement's amplitude. Ruled out: the clock origin, the Debug bench, the
dump path (one reader), the wrap modes (clamp-to-edge on every sampler),
the shader text (the same MSL on both OSes, compiled at run time with
sokol's default options). What stands: the two Metal stacks run the same
passes to displacements a thousandth apart per active second, in RGBA16F,
resampled every pass. The author, watching both: "visually almost
imperceptible".

## For the author

* The step's premise (QOL §1: "determinism holds because the field math is
  identical across backends within the documented tiers") is met per device
  — bit for bit on the Mac and on the iPad, the frame field load-bearing —
  and NOT across the two Metal stacks for a passage of a thousand frames:
  the §4.6 tier bounds seven passes on one GPU. The picture is the same to
  the eye. Flagged in #24; narrowing it is a core change (precise math in
  the passes, or a float32 field) that this phase's fixture invariant
  forbids — your call.
* The Linux box: out of commission. When it is back:
  `midi-sink --dev --replay docs/evidence/step65/mac/mac_demo.sumireplay --field-dump out.bin`
  on the box, then `field_dump_compare` against the Mac's dump re-made by
  `--record-demo` (the GL tier 2.5e-2 / 1e-3 for llvmpipe).
* The Tab: built (`./gradlew :app:assembleDebug`, green), not run — it was
  not connected. The run: `adb install -r …/app-debug.apk`, then
  `adb shell am start -n com.vibetuned.midisink/.MainActivity --ei layout 8 --es playMode 1 --es fingeringDemo 1 --es recordLab 20`,
  `adb shell run-as com.vibetuned.midisink cat files/Replays/lab.sumireplay`
  and `lab.field.bin`, then `tools/replay_gate.py` with the GLES tier.
* Your own recordings: the desktop's Settings → Replay (Record / Stop, the
  list, Play; a `.sumireplay` from the iPad through the path box), the
  iPad's Settings → Replay (Record; the list plays; share through Files).
  A replay keeps your window size and palette; dip and print it after.

| tool | what it did |
|---|---|
| `cmake --build build`, `ctest` | 11/11 (replay_tests new) |
| `build-universal` (Release) | the bench twin: bitwise against Debug |
| `build-ios`, xcodegen, xcodebuild, devicectl install/launch/copy | the iPad: the lab recording, the self replay, the cut series, the Mac demo on the iPad |
| `./gradlew :app:assembleDebug` | the Tab's build (not run: not connected) |
