# Evidence — Step 29: Windows release lane

PHASE5 §2–§3; ROADMAP_4 Step 29. Decisions: `_work/DECISIONS_4.md` #39–#43.
Machine: the author's Windows 11 box (real GPU, 5120×2160 @ 125 % display,
MSVC 2022, CMake 4.4.3, Ninja; no code-signing certificate — signing is
designed if-cert-present and verified unsigned). No core change.

## What landed

* **`windows` job in `release.yml`** — per the LANE INTERFACE: `needs:
  [version, gates]`, MSVC (`ilammy/msvc-dev-cmd`) + choco Ninja, Release
  build with `-DSUMI_APP_VERSION` from the tag, `--version` check, artifact
  **`dist-windows`** = `midi-sink-<v>-windows-x64-setup.exe` +
  `midi-sink-<v>-windows-x64-portable.zip` + `.sha256`s. `publish` now needs
  `windows`. Signing-if-cert-present: `WINDOWS_CERTIFICATE` (base64 .pfx) +
  `WINDOWS_CERTIFICATE_PASSWORD` → signtool on the exe and the installer,
  timestamped; absent → a `::notice::` and an unsigned release (#41 — secret
  names are proposed, ask the author).
* **Installer: Inno Setup** (`packaging/windows/midi-sink.iss`, #39) —
  per-user, no UAC, Start-menu entry, uninstaller; uninstall preserves
  `%APPDATA%\midi-sink` unless the user opts in (never in silent/winget
  uninstalls). **Static CRT** (#40): the exe now runs on a clean VM with no
  VC++ redistributable; **VERSIONINFO** from the tag in the exe.
* **`publish-winget.yml`** — sibling-shaped (`release: published`,
  `WINGET_TOKEN`, missing-manifest skip, `wingetcreate update
  Vibetuned.MidiSink`), plus the pre-release skip (#42). Staged manifest
  conventions in `packaging/windows/winget/Vibetuned.MidiSink/`
  (`winget validate` passes) with `stage.ps1` for the RC `--manifest` check.
* **Settings window verified + polished on Windows** (#43): the HiDPI scale
  Step 23 captured but never applied is now applied (window, style, font ×
  content scale via the ImGui GLFW helper — 1.0 on Apple/Wayland by design);
  the mojibake first-run log line is ASCII.
* README (Windows lane section + install commands + SmartScreen note) and
  the site's `guide/install.md` Windows paragraph (winget, installer,
  portable, SmartScreen, settings survival).

## Verified here

| Check | Result | Evidence |
|---|---|---|
| Build + suites on Windows (static CRT) | **PASS** — 5/5 ctest (ABI ×2, normalizer, hostmpe ×2); exe depends on system DLLs only (no MSVCP/VCRUNTIME/ucrt) | `ctest.log`, `dumpbin_dependents.txt` |
| §4.6 field gate through the real D3D11 renderer, reference tier | **PASS** — max 3.9e-3 (ink), mean 6.8e-6 vs the Metal fixture (1e-2/1e-4 tier); negative control red | `field_gate_d3d11.txt` |
| `--dev` gating in a release binary | **PASS** — `--field-dump`/`--resize-test` refused with the one-line pointer, exit 2; `--version`/`--help` fine; keys dead without `--dev` (only `Ctrl ,` lives) | `dev_gating.log` |
| Settings window on Windows: GL 3.2 second window beside the D3D11 canvas | **PASS** — created, placed right of the canvas, all sections render | `run_settings_err.log` (placement line), `settings_window_win.png` |
| HiDPI at 125 % | **PASS** — 700×950 physical window, correctly sized text (was 560×760 with ~13 px fonts before the #43 fix) | `settings_window_win.png`, `settings_bottom_sections.png` |
| `Ctrl ,` reopen after close | **PASS** — WM_CLOSE hides (visible=False), scancode-level Ctrl+comma to the canvas brings it back (visible=True) | this file (scripted user32 run, transcript in the step log) |
| CC-map editor | **PASS** — GUI "remove" dropped route `255:1:0` from the INI immediately; "Restore default map" brought the documented 12-route table back | `settings_ini_roundtrip.ini`, `settings_canvas_actions.png` |
| INI round-trip | **PASS** — GUI Rankine click → `vortex_profile=1` persisted; reload on next launch (`[settings] loaded`); first-run write works | `settings_ini_roundtrip.ini`, `run_gui.log` |
| Print export to the chosen folder | **PASS** — dip + "Save last print as PNG" buttons → `%USERPROFILE%\Pictures\midi-sink-print-<stamp>.png` (1.4 MB), status line shows the path | `settings_canvas_actions.png` |
| Installer round-trip (the local half of the clean-VM DONE) | **PASS** — `/VERYSILENT /CURRENTUSER`: no UAC, installs to `%LOCALAPPDATA%\Programs\midi-sink`, Start-menu entry, uninstall registry record (`midi-sink 0.5.0-test by Vibetuned`); silent uninstall removes app + shortcut, **settings survive** | `installer_roundtrip.log` |
| Portable zip | **PASS** — unzip anywhere, `--version` runs | `installer_roundtrip.log` (same exe), lane step |
| VERSIONINFO from the tag | **PASS** — ProductVersion `0.5.0-test`, FileVersion `0.5.0.0`, Vibetuned | `versioninfo.txt` |
| Staged winget manifests | **PASS** — `winget validate` succeeds | `winget_validate.log` |
| WinMM MIDI: input list + the MPE stress | **PASS** — the settings window lists `loopMIDI Port 1` with the rescan age; the feeder pushed **69,023 messages / 30 s** (10 voices × 200 press/s + bends + CC74 after MCM) through loopMIDI: **164.5 fps avg, 0 dropped**, MCM decoded (MPE, lower zone, 15 members). The ROLI was not connected this session; the list is the harness's own open-input snapshot (DECISIONS_4 #8) and hardware presence was proven in Step 11 | `settings_midi_inputs.png`, `run_midi.log`, `run_midi_err.log`, `feeder_midi.log` |
| Workflows parse; lane contract wiring | **PASS** — jobs `version, gates, web, macos, windows, publish`; `publish.needs` includes `windows`; `windows.needs = [version, gates]` | (pyyaml load, step log) |

Lab bench under `--dev` on Windows: key legend, ripple insertion, raw-MIDI
toggle all render (`settings_devmode_labbench.png`); key path exercised end
to end ('9' → dip → "paper-dip print ready").

## YOURS

1. **The runner run.** Actions → release → Run workflow (`dry_run = true`)
   must show `windows` green next to the others before any tag; then a
   pre-release tag (e.g. `v0.5.0-rc.5`) exercises the full path.
2. **Clean-VM install.** Download the setup exe from the published
   pre-release on a clean Windows VM: SmartScreen "More info → Run anyway"
   (unsigned), install without UAC, launch, About reads the tag; also unzip
   the portable zip and run it. The static CRT (#40) is what makes this pass
   without a VC++ redistributable.
3. **winget RC check.** After publishing the RC:
   `powershell packaging\windows\winget\stage.ps1 v<RC>` then
   `winget install --manifest packaging\windows\winget\Vibetuned.MidiSink`
   (enable `LocalManifestFiles` once). The first REAL release then gets the
   human `wingetcreate new` submission (#42).
4. **Signing secrets.** Confirm or rename `WINDOWS_CERTIFICATE` /
   `WINDOWS_CERTIFICATE_PASSWORD` (#41) if a certificate materializes;
   nothing blocks unsigned releases meanwhile.

## Found on the way

* /MD would have failed the clean-VM DONE (no VC++ redist on a clean box and
  a no-UAC installer cannot add one) → static CRT, #40.
* winget parses every file in a `--manifest` directory — a README beside the
  manifests breaks validation → subdirectory, #42.
* The Step-23 HiDPI scale was captured but never applied → #43.
* The teVirtualMIDI driver (loopMIDI's kernel side) had VANISHED since Step
  11 — only loopMIDI.exe survived, port creation silently failed
  (`sc query teVirtualMIDI`: not installed; likely a Windows-update
  casualty). Reinstalling loopMIDI (one UAC approval) fixed it; if loopback
  MIDI ever "stops working" on a Windows box, check the driver before the
  app.
* DECISIONS_4 #35's note held: this machine's real GPU passes the REFERENCE
  tier against the Metal fixture (mean 6.8e-6 — no longer bitwise as in Step
  11, GPU/driver drift since, still 15× inside the reference mean budget).

## Not taken

* NSIS (the siblings' installer tech) — theirs is Tauri-generated; a
  hand-authored .iss is the readable in-tree source (#39).
* Bundling the VC++ redistributable or its DLLs — the static CRT removes the
  need (#40).
* An uninstall-time "remove settings" checkbox page — Inno has no native
  uninstall wizard page; the post-uninstall prompt (interactive only) honors
  the spec with less machinery (#39).
* `Pictures/midi-sink-print-…` shows a mixed path separator in the status
  line (the join is `"/"`) — Windows accepts it everywhere; cosmetic, left
  alone rather than forking `default_print_path` per platform.

**Tree ready** — evidence: `docs/evidence/step29/` (this file).
