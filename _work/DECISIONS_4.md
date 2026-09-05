# DECISIONS_4 — Phase 5: Packaging, Release, Web & Documentation

Ambiguities resolved during Phase 5 (steps 23–33). Prior history:
`docs/DECISIONS.md` (Parts I–III; references written as `DECISIONS_3 #n`
mean Part III). This file merges into that document as Part IV when the
phase ships. Where these entries and `_work/PHASE5_SPEC.md` /
`_work/ROADMAP_4.md` conflict, the later entry is the record of what shipped.

## Step 23 — Desktop productization (macOS)

1. **Product naming — one name, already in use everywhere: `midi-sink`.**
   The app is what the repo, the iOS App Store Connect record and the Linux
   desktop entry already call it; only the core library carries the `sumi`
   name (`libsumi`, `sumi_core.h`). Recorded once so every later manifest
   references it:
   * Display name / executable / window title: **`midi-sink`**.
   * macOS bundle id: **`com.vibetuned.midi-sink`** (the iOS app's id, one
     Team, one identity family; Android stays `com.vibetuned.midisink` —
     Java package rules forbid the hyphen, and that record exists).
   * Homebrew cask token: **`midi-sink`**. winget id: **`Vibetuned.MidiSink`**.
     Debian package: **`midi-sink`**. Linux `app_id` / `WM_CLASS` /
     `.desktop` basename: `midi-sink` (unchanged, DECISIONS_2 #39).
   * Config directory name on every desktop: `midi-sink` (#4 below).
   Renaming was considered ("Sumi", "Suminagashi") and declined: the store
   records, the tap and the apt repo are the author's existing infrastructure
   (working rule), and a name change would orphan them for no user benefit.

2. **The shared settings UI is Dear ImGui in a SECOND GLFW window with its
   own OpenGL context — one implementation for macOS, Windows and Linux.**
   The constraints that force it: the harness may not include sokol headers
   (working rule — sokol lives behind `renderer.cpp`), so `sokol_imgui`
   drawing into the core's swapchain is out; the core OWNS the main window's
   device/drawable (§5.1), so a second renderer cannot share that surface
   without a core change (frozen); native toolkits would be three
   implementations of one window (the "authored once" rule). A separate
   window with `imgui_impl_glfw` + `imgui_impl_opengl3` (its self-contained
   loader — no GLEW/GLAD) needs nothing from the core and is identical on all
   three platforms. GL 3.2 core / `#version 150` everywhere (the macOS
   ceiling for a forward-compatible context; deprecated there but present
   through macOS 26 — a settings panel is the right size of bet). On Linux,
   where the MAIN window's context is the core's (§5.1 GL exception), the
   settings frame makes its own context current and restores the main one
   before the next `sumi_update` — the core never sees a foreign context.
   Dear ImGui `v1.92.9b`, pinned via FetchContent like every dependency (§7).
   The window opens beside the canvas at launch; closing it hides it;
   **⌘ , / Ctrl ,** reopens it (the platform's own "Preferences" chord — the
   one keyboard binding a release build keeps, and the first-run hint names
   it).

3. **Version strings: `SUMI_APP_VERSION` is a CMake cache variable the
   release spine injects from the tag; locally it defaults to
   `git describe --tags --always --dirty`.** No version is hand-edited
   anywhere (working rule). `sumi_version()` (0.4.0, the ABI) is a DIFFERENT
   number and stays so: About shows app version, commit and engine version
   side by side. Info.plist's `CFBundleShortVersionString`/`CFBundleVersion`
   must be numeric, so the build extracts `X.Y.Z` from the tag and falls back
   to `0.0.0` for untagged dev builds (the describe string still appears in
   About and in a custom `SumiBuildDescribe` key). Until the first tag exists
   About reads e.g. `dev 5ba722f`.

4. **Settings persist in a plain INI in the platform config directory**:
   `~/Library/Application Support/midi-sink/settings.ini` (macOS),
   `%APPDATA%\midi-sink\settings.ini` (Windows),
   `$XDG_CONFIG_HOME/midi-sink/settings.ini` (Linux, `~/.config` fallback).
   Everything the settings window shows is stored — params mirror, CC map,
   ripple CC values, print folder, whether the settings window was open,
   and `first_run_dismissed` (the spec's one dismissible hint needs exactly
   one persisted bit). Written on every change (the file is a few hundred
   bytes); a missing or malformed file yields defaults, never a failure.

5. **`--dev` scope — the lab bench is a flag, never a build variant.** Without
   `--dev` the harness accepts only `--dev`, `--help`, `--version`; every
   debug key (1–9, L, B, S, V, K, C, P, J, W/E, M, O, R/T, F/G, X), every
   scripted flag (`--field-dump`, the step-19/20 test battery, `--drop-test`,
   demos, `--dip-*`, `--cycle-visuals`, `--resize-test`, `--exit-after`,
   `--map-cc`, `--layout`, `--sim-scale`, `--print-out`) and the raw-MIDI log
   toggle are refused with one line pointing at `--dev`. With `--dev`
   everything behaves as before, and the settings window gains a "Lab bench"
   section (key legend, ripple live/bake override, raw MIDI log, the swirl
   test voice). Release builds keep the flag so support can say "run with
   --dev". The §4.6 field regression is therefore invoked as
   `midi-sink --dev --field-dump …` from now on. `S` (save print) lost its
   key and became the "Save last print as PNG" button.

6. **macOS is a real `.app`; the runtime Dock-tile hack retires.**
   `MACOSX_BUNDLE` target with `packaging/macos/Info.plist.in`
   (`LSMinimumSystemVersion 12.0`, `NSHighResolutionCapable`, music
   category), `packaging/macos/midi-sink.icns` generated by
   `tools/gen_icons.py` (Pillow's ICNS writer — no `iconutil` dependency, so
   the generator stays cross-platform; the generator gained `--only` so a
   macOS step regenerates only macOS assets, per the one-platform rule),
   hardened-runtime entitlements (`packaging/macos/entitlements.plist`,
   deliberately empty: CoreMIDI needs no entitlement, the app loads no
   plugins), and an **ad-hoc `codesign --options runtime` post-build step**
   (`SUMI_CODESIGN_IDENTITY`, default `-`; Step 27 sets the Developer ID).
   `sumi_macos_set_dock_icon` and the generated `app_icon_macos.h` are
   deleted — the bundle's `CFBundleIconFile` is the Dock tile now. The
   binary path on macOS becomes
   `build/desktop/midi-sink.app/Contents/MacOS/midi-sink`; Windows and
   Linux keep the bare executable (the Linux install component is untouched).

7. **The CC-map editor works on a HOST-SIDE MIRROR because the core has no
   map readback and stays frozen.** The mirror is seeded with the core's
   `install_default_cc_map` table plus the harness's two ripple routes
   (CC 102 → `RIPPLE_AMP`, CC 103 → `RIPPLE_FREQ`, DECISIONS_3 #32) and
   applied as `sumi_clear_cc_map` + one `sumi_map_cc` per route — so the
   core's state is always exactly the mirror. Consequence: "Restore
   defaults" restores the documented README table, and the ripple sliders in
   the settings window drive whatever CC is routed to the ripple dims (they
   grey out if the user removes that route — the sliders are MIDI, not a
   private channel into the core).

8. **MIDI port list = the harness's open-input snapshot + rescan age.** The
   harness already rescans every second (DECISIONS #25); it now exposes a
   copied name list, seconds since the last rescan, a "rescan now" and the
   raw-log toggle, all taken under its existing mutex, so the settings
   window shows exactly what the byte path is connected to. No new MIDI
   code path: the list is the truth the rescan already had.

9. **Three macOS shell facts learned while making the bundle real (Step 23
   evidence).** (a) GLFW's Cocoa backend `chdir()`s a bundled app into
   `Contents/Resources` by default (`GLFW_COCOA_CHDIR_RESOURCES`), so the
   first `--dev --field-dump out.bin` wrote INTO the bundle and every
   relative print path would have too; the hint is now `GLFW_FALSE` before
   `glfwInit`. (b) A position set on a still-hidden window is replaced by
   macOS's own frame on first show, and a `glfwFocusWindow(canvas)` issued
   after the settings window appeared buried it behind the canvas on a
   display too narrow for both side by side (the visible strip past the
   canvas's edge looked exactly like a screen-edge clip and cost three
   captures to diagnose). The settings window's placement is clamped to the
   monitor work area (right of the canvas, else left, else flush right),
   re-applied once on first show, and the canvas is not refocused after it
   appears; `Cmd ,` raises it any time. (c) Dear ImGui's default font covers
   Latin-1 only — `⌘` and `—` render as `?`; UI strings say `Cmd ,` and use
   ASCII dashes. Also: settings are written on FIRST RUN, not only on
   change/exit, so a session that ends by force still leaves the file.
