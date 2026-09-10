# tools/

Scripts that outlive the evidence folders they were written in (`docs/evidence/`
is removed from the tree when a phase ships; git history keeps it).

| Path | What it is | Where it came from |
|---|---|---|
| `chart_check.py` | Verifies `site/src/data/midi-chart.json` against the Play-mode byte logs in `tests/fixtures/bytelogs/` (`npm run chart` in `site/`, and the PR workflow). | Step 26 |
| `field_gate.py`, `web_gate.mjs`, `web_serve.py` | The §4.6 cross-backend field regression on the desktop renderers and on WebGPU (headless Chrome: field dump, scene sweep, page captures). | Steps 24–25 |
| `midi_asserts.py`, `pen_trace.py` | Byte-log analysers for the tablets' Play-mode streams (handshake order, rate policies, legato reconstruction). | Phase 4 |
| `release_notes.py` | The `## v<version>` section of `docs/CHANGELOG.md` as release notes (`--strict` on real tags). | Step 24 |
| `gen_icons.py` | Every platform's icon from `images/midi-sink.jpg` (`--only site` for the docs/web icons). Needs Pillow + numpy. | Step 14 / Phase 5 |
| `stokeslet_verify.py` | The 2-D unsteady Stokeslet displacement kernel derived and checked numerically (E₁ series, divergence, the d_y sign) — the viscous stroke's paper trail (DECISIONS_4 #53). | Step 33 |
| `linux/roli_capture.sh` | ROLI-over-ALSA byte capture while a human plays (`tools/linux/roli_capture.sh [seconds] [out.csv]`), with the byte summary and the informational `midi_asserts.py capture`. | Step 30 |
| `linux/aptlocal.sh` | End-to-end test of the apt repository shape `publish-apt.yml` builds: throwaway key, the same `apt-ftparchive`/`gpg` commands, a clean container installing from it over http. | Step 30 |
| `linux/jammy_probe.sh` | Builds the tree inside an `ubuntu:22.04` container (Kitware CMake, gcc-12) — the glibc-floor probe for the Linux lane. | Step 30 |
| `linux_automation/` | The Step-33 desktop checks driven on X11 (XTest pointer/keyboard, MIDI files into the ALSA Through port, `import` captures — `x11auto.py` is the helper, `cases*.py` the cases, `mkmidi.py` writes the SMF files) and on Wayland through mutter's remote-desktop API (`mutterrd.py`, `wlcases_mutter.py`, `wlclick.py`). Set `MIDI_SINK_ROOT` if the tree is not two levels up. | Step 33 (Linux box) |
| `android_automation/` | The Step-33 Android checks over adb (`droid.py` is the uiautomator/`input` driver; `droid_final*.py`, `droid_rows.py`, `droid_replay_check.py` the check sets; JSON results beside the run). Set `MIDI_SINK_ROOT` for the APK path. | Step 33 (Linux box) |

The desktop lab bench itself (`midi-sink --dev …`, the scripted operator tests)
lives in `desktop/src/dev_tools.cpp`; the headless suites in `tests/`.
