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

## Step 24 — Release orchestration spine

10. **One tag-triggered workflow, `release.yml`, whose spine has zero platform
    lanes: `version` → `gates` (matrix) → `publish`.** `version` is the single
    source of the version string — `${GITHUB_REF_NAME#v}` on a tag push,
    the dispatch input (or `0.0.0-dry.<run>`) otherwise — validated as
    `X.Y.Z[-pre]`; every downstream job builds with
    `-DSUMI_APP_VERSION=${{ needs.version.outputs.version }}` and the gate
    asserts the binary's `--version` carries it (a version that CI injects
    but the binary does not show is the bug the working rule exists to
    catch). `dry_run` is a first-class output: on a manual dispatch it
    defaults to true, the gates and the notes run in full, and `publish`
    is skipped (`if: dry_run != 'true'`); a tag push is never a dry run.
    Following battuta / midi-stroke: the release is created as a DRAFT
    (`softprops/action-gh-release`, notes as body, prerelease when the
    version has a `-`), a human publishes it, and the channel bumps (cask /
    winget / apt) are separate `release: published` workflows in Steps
    27/29/30 — a draft has no public asset URLs, so a bump inside the spine
    would always point at nothing. **iOS and Android have no lane job, by the
    author's decision (roadmap revision during Step 24):** store builds are
    manual procedures (Steps 28/31 — Xcode archive → TestFlight, Android
    Studio bundle → Play internal, from a `RELEASING.md` checklist against a
    tagged checkout with the CI-injected version). Store credentials never
    enter CI; push CI keeps mobile build-only compile checks so a tag never
    surprises the archive.

11. **The §4.6 field regression is the packaging gate, run through the REAL
    renderer on every desktop runner, and it carries its own negative
    control.** `tools/field_gate.py` dumps the canonical field with
    `midi-sink --dev --field-dump`, compares it to the committed Metal
    fixture with the backend's tolerance (desktop backends: the comparator
    defaults 1e-2 / 1e-4 — D3D11 and GL both matched the fixture within
    them in Steps 11/12), and then compares the dump to a DELIBERATELY
    CORRUPTED copy of the fixture (+0.5 on a 32×32 block of u) and requires
    the comparator to FAIL it — "proven red before it is trusted green",
    every run, not once. Distinct exit codes name the failure class: 1 the
    field regressed, 3 the gate cannot go red, 4 no renderer on this machine
    (infrastructure, not a regression). `publish` and every lane `needs:
    gates`, so any red blocks packaging by construction. Linux runners have
    no display: the GL gate runs under `xvfb-run` with Mesa llvmpipe
    (`LIBGL_ALWAYS_SOFTWARE=1`). **Flagged, not fixed here:** the D3D11
    swapchain creates a `D3D_DRIVER_TYPE_HARDWARE` device only; a GPU-less
    Windows runner may return exit 4. If the first dry run does, the WARP
    fallback belongs to the Windows lane step (29) — a `swapchain_*` seam
    change like the Step-11/12/14 pattern, not a core change — and until
    then the Windows gate is honestly red, never quietly skipped.

12. **Release notes ARE the changelog section.** `tools/release_notes.py`
    extracts `## v<X.Y.Z>` from `docs/CHANGELOG.md` verbatim under a title
    line (the heading's subtitle becomes the italic deck) and appends the
    generating commit. No section yet → the latest section is used and the
    draft is marked **DRAFT** (a dry run on a test tag still yields a
    versioned notes draft, as the DONE asks); on a REAL tag the spine runs
    `--strict`, so a tag cut before the changelog carries its section fails
    in `version` — the release cannot exist without its notes. No third
    format, ever (the condensed-evidence practice continues).

13. **The lane interface is a contract written in the workflow file, for
    exactly four CI lanes.** A lane is one job: `needs: [version, gates]`;
    every upload guarded by `dry_run != 'true'`; exactly one artifact
    `dist-<lane>` (web, macos, windows, linux) whose files are named
    `midi-sink-<version>-<platform>[-<variant>].<ext>`; lanes never create
    releases — `publish` merges `dist-*` and drafts one. No mobile artifact
    names exist in the contract (see #10). Written at the top of
    `release.yml` so Steps 25/27/29/30 read it where they will edit.

14. **`build.yml` (push CI) stays compile + headless suites.** The field gate
    is deliberately NOT promoted to every push until a dry run has shown
    which runners can render (D3D11 on a VM, Mesa on Ubuntu). Once it has,
    promoting it is one step copied from `release.yml`. Recorded so nobody
    reads its absence from push CI as an oversight.

## Step 25 — WebGPU backend & marble web

15. **The WebGPU seam: the HOST creates the device, the CORE owns the
    surface.** A browser can only create adapter and device asynchronously,
    and `sumi_create` is synchronous by contract — so the page does
    `requestAdapter/requestDevice`, imports the device into the wasm
    (`Module.WebGPU.importJsDevice`, emdawnwebgpu) and passes it in a new
    `sumi_webgpu_surface_t {device, canvas_selector, color_format}` as the
    `native_surface_handle` for the new `SUMI_BACKEND_WEBGPU` (= 4). The core
    then creates the surface from the CSS selector, configures it in the
    canvas's preferred format (`getPreferredCanvasFormat`, passed in — no
    adapter needed core-side), acquires the frame texture, reconfigures on
    resize and releases everything at destroy. The browser presents. This
    keeps §5.1's "core owns the swapchain" everywhere the platform allows it
    and moves exactly the one async step to the host. Additive ABI →
    `sumi_version()` 0.5.0; `abi_c_compile` exercises the struct in C11.
    ASYNCIFY/JSPI (to make `sumi_create` block on device creation) was
    rejected: whole-module instrumentation for one call, and JSPI is not in
    every target browser yet.

16. **Readback on WebGPU: injected CopySrc textures + copy + mapAsync, and
    the field read split into begin/poll.** sokol's WebGPU backend never sets
    `CopySrc` on the textures it creates, so `copyTextureToBuffer` from the
    field/print targets would fail validation. Rather than patch a
    dependency or write a raw-WebGPU blit, the swapchain contract gained a
    prepare/release pair: the renderer calls `sumi_swapchain_prepare_image`
    right before `sg_make_image` for the two readback-bound targets (field
    pair, print) and `release_image` before `sg_destroy_image`; on WebGPU
    prepare creates the texture with RenderAttachment|TextureBinding|CopySrc
    and injects it via `sg_image_desc.wgpu_texture`, on the other three TUs
    both are no-ops. `mapAsync` completes only when control returns to the
    event loop, so `sumi_swapchain_yield` is a no-op on the web and the sync
    `sumi_renderer_read_field` was split into `read_field_begin` /
    `read_field_poll` (the sync form is now those two plus the old bounded
    yield loop); `sumi_debug.h` exposes the pair and the web host polls the
    §4.6 dump across frames. The print path was already async and needed
    nothing. Rows come back 256-byte padded and are de-padded in poll.

17. **The wasm export surface is the C-ABI itself, plus a struct-building
    shim.** Every `SUMI_API` function is in `EXPORTED_FUNCTIONS` and called
    through `cwrap`; `web/sumi_web.cpp` adds only what JS cannot do without
    byte-level struct layouts — `sumi_web_create` (builds `sumi_config_t` +
    the surface struct), flat `get/set_param(id)` accessors over
    `sumi_params_t`, and the field-dump hooks (internal, static-link-only;
    the wasm IS a static link). `-sMODULARIZE -sEXPORT_ES6`
    (`createSumi()`), `-sENVIRONMENT=web`, memory growth on. hostmpe is not
    built for the web (Play mode is web-deferred, PHASE5 §5/§7).

18. **WGSL joins the shader dialect list for EVERY build** (`SUMI_SHDC_SLANG`
    gains `:wgsl`): the generated headers carry all dialects and each backend
    picks its own at `sg_make_shader`, exactly how Metal/HLSL/GLSL coexist
    today. The desktop binaries grow by the WGSL text — negligible — and
    there is one shader header, not a web-specific one. The §4.6 orientation
    story needs nothing new: WebGPU's texture origin is top-left like Metal
    and D3D11, so the `flip_vert_y` GLSL-only option stays GLSL-only.

19. **The scene/embed API is a page contract, not a second engine.**
    `?scene=<name>` selects an operator scene — drop, tine, vortex, rankine,
    wake, pinch, ripple, lamb_oseen, scroll — each a deterministic script on
    a fresh sheet whose sliders are the FORMULA'S SYMBOLS (r; α z; A R; ω R;
    a d; k θ n v; A k φ b; Γ v t; bpm ρ) and whose values can be preset
    through query parameters; `&embed=1` strips the chrome. The docs (Step
    26) embed these URLs, so their live examples are the release wasm and
    nothing else drifts. `?fielddump=1` is the §4.6 web tier: fixed 512×512,
    the canonical script, the non-blocking readback, and a download of the
    same `.bin` format the desktop harness writes (half→float in JS), so
    `field_dump_compare` needs no web-specific code. Pages layout: the marble
    app lives under `/marble/`; the docs site owns the root from Step 26 (a
    redirect stands in until then). The web lane is the first lane on the
    Step-24 spine: `dist-web` = `midi-sink-<version>-web.tar.gz`, Pages deploy
    guarded by `dry_run`. Emscripten pinned at 4.0.15 in CI
    (`mymindstorm/setup-emsdk`); locally Homebrew's 6.0.9 — the wasm is
    reproducible per pin, and the pin moves deliberately.

20. **The web tier's §4.6 tolerance is the DESKTOP default (max ≤ 1e-2,
    mean ≤ 1e-4) — measured, not provisioned.** First WebGPU dump (Chrome 152
    headless, Apple Metal-3 adapter, Dawn) against the committed Metal
    fixture: **max|Δ| 9.77e-4 (ink), 4.88e-4 (u, v), 0 (aux); mean 7.9e-9**
    — an order of magnitude inside the desktop budget and far from the
    mobile tier (2.5e-2 / 1e-3) the plan had reserved for it. Recorded so the
    gate stays honest: `tools/web_gate.mjs` defaults to 1e-2 / 1e-4.
    Three things the first headless runs taught, kept in the code:
    * **The device bridge is `Module.preinitializedWebGPUDevice` +
      `emscripten_webgpu_get_device()`**, not `Module.WebGPU.importJsDevice`
      — the port's `WebGPU` library object is never exported onto the module.
      The accessor is marked deprecated-in-name in the port's JS ("TODO:
      remove once fully deprecated in users"); if it goes, the replacement is
      exporting the library's import function explicitly. The C-ABI contract
      (`sumi_webgpu_surface_t.device`) is unaffected either way — only the
      export glue fetches the handle.
    * **Map callbacks must be `AllowSpontaneous`.** With `AllowProcessEvents`
      the completion is queued on the instance that owns the device's event
      manager — the host-imported device's, not the instance the TU creates
      for the surface — and pumping ours never delivered it (polled 1,700
      frames without a completion). Spontaneous delivery comes straight from
      the browser's promise resolution.
    * **Copying the host page is its own always-run target**
      (`sumi_web_site`), not a POST_BUILD of the wasm: a JS-only change never
      relinks the module, so the copy silently did not happen.
    Also: the gate tool (`tools/web_gate.mjs`) serves the build, drives
    headless Chrome and receives the dump + the page console over POST — the
    page cannot download in a headless run and a headed tab throttles its
    frames when the display sleeps, which cost an hour before the tool
    existed. It is the release lane's future evidence hook as well.
    * **Secure context (found by the user on first launch):** `navigator.gpu`
      exists only on `https://` or `localhost`; the LAN URL over plain http
      reads as "no WebGPU". The overlay now names the real cause and the fix,
      and `tools/web_serve.py` serves the build over HTTPS with a
      self-signed certificate (SANs = the Mac's IPs) for iPad/LAN testing.
      Same rule applies to WebMIDI, and the Pages deployment is HTTPS by
      nature — the constraint is a dev-serving one only.
      (The first cut of that server was Node; the author's macOS firewall
      has an explicit "block incoming" rule for Homebrew's node binary, which
      reset every LAN connection while localhost worked — Python, which the
      firewall allows, is the server now. Recorded because the symptom looks
      exactly like a TLS or routing bug.)
    * **The "no WebGPU" overlay showed over a WORKING canvas** (user report,
      confirmed: the scene rendered behind it). Cause: `.overlay { display:
      grid }` — an author `display` rule outranks the browser's
      `[hidden] { display: none }`, so the card was painted whatever the
      attribute said; every earlier "no WebGPU" report on this Mac and the
      phone was this bug, not a WebGPU problem. Fixed with
      `.overlay[hidden] { display: none }`. Lesson kept: never give a
      `hidden`-toggled element an unconditional `display`.

21. **The web page gets the desktop settings window, as lil-gui (user
    request: "the Mac version has its options").** `lil-gui` 0.21.0 is
    VENDORED (`web/site/vendor/`, MIT notice beside it), never CDN-loaded: the
    Pages site must work with no third-party origin and reproducibly per
    pin. The panel mirrors `desktop/src/settings_ui.cpp` section for section
    where marble mode has the concept — Layout & look (six layouts, three
    palettes, viscosity / ink feed / roughness, full-resolution, tempo and
    roll speed), Expression routing (per-note bend, channel pressure, CC 74,
    pinch style, vortex profile), Ripple (amount / wavelength through the
    routed CC 102/103 exactly like the desktop's sliders, angle), Canvas
    (paper dip, save last print), About (engine version, WebMIDI inputs,
    live frame stats, reset). No CC-map editor and no MIDI-port rescan (no
    ports to open on the web — WebMIDI hands them to us). Settings persist
    per browser in `localStorage` (`sumi-web-settings`, the INI's role) and
    are applied before the first frame; a `?scene=` page does NOT apply them —
    a scene owns its parameters so embeds are deterministic; `embed=1` hides
    the panel; it starts collapsed under 720 px. The old info card and topbar
    buttons folded into the panel. Placement (user): the panel sits at the TOP
    LEFT and there is no top bar — the panel's title is the brand; the scene
    card moved to the top right; a status pill at the top left shows only the
    messages that arrive before the panel exists (loading, no adapter,
    insecure origin). lil-gui's own `top: 0; right: 15px` auto-place rule is
    injected after the page stylesheet, so the override carries a third class
    (`.lil-gui.lil-root.lil-auto-place`) — 0.21 renamed `root`/`title` to
    `lil-root`/`lil-title`.

## Step 26 — Documentation site

22. **The site is Astro Starlight at `site/`, deployed from the release tag,
    and three URLs are frozen.** The author's other documentation sites
    (battuta, Midi Stroke) are Starlight; this one follows them — same
    package layout, same relative-link discipline, same `DOCS_BASE`
    project-site escape hatch. Domain: **`https://midi-sink.vibetuned.com/`**
    (the `<project>.vibetuned.com` convention of the sibling sites; the custom
    domain is set in the repo's Pages settings with a DNS CNAME to
    `vibetuned.github.io`, both human actions). The **frozen URLs** every later
    lane and store listing hardcodes: homepage `/`, privacy policy
    `/privacy/`, support `/support/`, and the marble web app `/marble/`.
    Changing any of them after Step 27 means editing a cask, two store
    records and the beta guide — so they do not change. **Deploy path:** the
    docs build joined the release workflow's `web` job (Node 22, `npm ci`,
    `npm run build`, compose `site/dist` at the root and `build-web/web-dist`
    under `/marble/`), so the site can only ever document the release it
    ships with — the same tag builds every artifact and the site, one
    deploy. There is deliberately no push-to-main Pages deploy: it would
    publish a site whose `/marble/` and version line disagree with the
    released artifacts. `build.yml` gained a `docs` job (build + drift check
    + chart check) so PRs cannot break the site.

23. **Formulae are KaTeX through the `@astrojs/markdown-remark` bridge.**
    Astro 7's default Markdown processor no longer accepts remark/rehype
    plugins directly; `remark-math` + `rehype-katex` need that package
    installed and the (deprecated-but-working) `markdown.remarkPlugins`
    form. Recorded so the warning in the build log is not chased.

24. **Design notes and changelog are GENERATED, verbatim.** `scripts/
    build-notes.mjs` renders `docs/CHANGELOG.md`, the four parts of
    `docs/DECISIONS.md` (split on the `# Part` headings) and, while Phase 5 is
    in flight, `_work/DECISIONS_4.md` as Part IV, into `notes/` pages that are
    gitignored and rebuilt before every build. The only edits are mechanical:
    frontmatter, the H1 dropped, repository path prefixes (`_work/`, `docs/`)
    trimmed to bare file names. Not one entry is reworded — the spec's
    "lightly edited … entries kept verbatim otherwise", taken literally so
    the record cannot drift from the file.

25. **"No second implementation" is a build failure, not a convention.**
    Every operator demo is `<Operator scene=…>`, an `<iframe>` of
    `/marble/?scene=…&embed=1` — the release wasm through the Step-25 scene
    API. `scripts/check.mjs` runs post-build and fails on: any `.wasm`,
    `sumi.js` or `sumi-host.js` inside the docs output; any WebGPU or engine
    call in a docs page; any iframe not pointing at `PUBLIC_MARBLE_URL` with
    a known `scene=` and `embed=1`; any of the nine scenes never embedded.
    Plus dead internal links, the three frozen URLs, the gallery manifest's
    caption fields, and unrendered `$$`. Authoring locally points
    `PUBLIC_MARBLE_URL` at `tools/web_serve.py`.

26. **The MIDI chart is data, and the data is checked against the byte
    logs.** `site/src/data/midi-chart.json` holds every row; `<Chart>`
    renders it; `tools/chart_check.py` replays the Play-mode byte logs
    (`docs/evidence/step26/bytelogs/`, the Step-22 sessions restored from
    git history: one per source — fingers, stylus, strip + pen pedal, session
    config) and asserts every `present` row is observed for that source and
    channel class, every `absent` row is not, and **every observed message is
    described by a row** (an undocumented output fails), plus the constants
    (zone size 15, RPN 0 = 48 on 15 distinct members, first finger bend =
    centre, strip on the master). Input-mode sections are the normalizer's
    contract and cite the headless suite. The CI `docs` job runs the check.
    First run caught one bug in the checker itself: a session that re-syncs
    repeats RPN 0 on all members, so the count is of distinct channels, not
    rows.

27. **The gallery is a runtime manifest; the tribute piece is *Ali Paşa*.**
    `public/gallery/gallery.json` is fetched by the page, so a new recording
    is an entry plus a file (schema in `public/gallery/README.md`); entries
    without media render as "recording pending", and the step ships with the
    three planned performances so marked (videos are an author input).
    **Tribute identification, from the credit frames of Jaffer's own
    videos** (downloaded from his CSAIL page; title cards read with ffmpeg):
    *Bouquet* → `voluntocracy.org/Music/Kendime.abc`, *Latte* →
    `AliPasa.abc`, *Wave* → `RampiRampi.abc` — his page says only "Turkish
    songs popular for international folk-dancing", the videos name them.
    Chosen: **Ali Paşa**, the tune of *Latte*, the one animation drawn with a
    single stylus rather than a rake — midi-sink's pen. Public-domain check:
    the ABC header says `C: Trad.`, `O: Turkey`; the Society of Folk Dance
    Historians records the song as the anonymous lament for Ali Pasha of Van,
    Turkish Folk Music Archive no. 398 (collected by M. Sarısözen); the DANCE
    was set by Bora Özkök, and is not what is performed. The alternates are
    also traditional (*Rampi Rampi* = *Çadırımın Üstüne*, 9/8, credited
    Traditional, first recorded 1946; *Kendime*, presented by Özkök). The
    optional Turkish-moiré scripted scene is NOT taken (it would need a new
    scene in the web host — Step 25 code — for an optional bridge).

28. **Privacy and support pages say what is true and nothing more.** The
    privacy policy states "collects nothing" and enumerates every place data
    actually touches (MIDI in memory, settings on device, prints on request,
    evidence logs in the app's own folder), the permissions and why, the
    Apple "Data Not Collected" and Play "no data collected / shared"
    statements, and GitHub Pages' own request logging. The support contact is
    the public `info@vibetuned.com` (the address the author's other sites
    publish) and the issue tracker — never a personal address.

29. **Operator scenes are paced and two-sited, and the pressure feed got its
    own scene (user review of the operator book).** The first cut showed each
    operator's RESULT; the book is about the mathematics, so every scene now
    (a) takes a `pace` slider — frames between steps, default 2, 0 = instant —
    and applies the exactly-composing operators as a run of small passes
    (tine z/n × n, vortex A/n × n, pinch and wake sub-steps one per step, the
    ripple amount ramped in), which is also the composition invariant shown
    live; and (b) works on two ring clusters, A top-left (0.30, 0.30) and B
    bottom-right (0.70, 0.70), so one operator shows two orientations or two
    signs at once — horizontal vs vertical tine, +A/−A vortices, fold axes θ
    and θ + 90°. The swirl is the exception (user review): stirring a
    ring cluster about its own centre shows little, so one voice sits at the
    centre and four ring pools in the corners are carried by the 1/r² far
    field over a long stir — the picture is the far field. New scene **`feed`**:
    A re-strikes a note (separate drops, rings) while B holds channel
    pressure on one note (one band, boundary growth) — the drop page shows
    the two side by side. Voice-driven scenes (swirl, feed) place their notes
    with the LAYOUT PROBE, now exposed to the page as `sumi_web_probe`
    (shim only — the probe is existing ABI, no core change): the chromatic
    grid's cell at A and at B, so the picture lands where the clusters are on
    any aspect. The headless sweep runs with `pace=0` (it tests completion,
    not pacing). Two things the first captures taught: (i) band parity
    follows the GLOBAL drop counter, so two sites fed A B A B … get one
    parity each — a solid disk and a hollow one; the order is A B | B A | …
    so each site alternates; (ii) a rotation of concentric rings about their
    own centre is invisible, so the vortex and Rankine scenes sit each vortex
    (0.07, 0.05) off its cluster — the exponential then visibly shears the
    rings and the Rankine visibly carries a rigid piece of them. Vortex
    defaults were then raised (A 6, ω 5 over 48 passes; sliders to ±12.6) so
    the demos show a FORMED vortex, not the start of one; the swirl's centre
    voice is struck softly and covered with a CLEAR drop (`sumi_add_drop`
    layer 1) so the core is unmarked water — the per-note strike cannot be
    suppressed from the host, which is the gesture gap #30 records; the
    author has since scoped Step 33 to unfreeze the core for such fixes.

30. **A Marble-mode gesture for the feed and the swirl — NOT taken in Phase 5,
    recorded for Phase 6.** The user proposed Shift + left-drag (mouse) and a
    long press (tablets). Two facts block it here: (1) it needs the core —
    neither the pressure feed (boundary growth on an existing drop's own
    band) nor the Lamb–Oseen swirl has a gesture entry point in the ABI; both
    exist only as voice dimensions, and in Marble mode a touch point has no
    note (the probe answers only on the playable lattices), so they cannot be
    synthesized as MIDI either — a `sumi_add_swirl` / `sumi_feed_drop` pair
    is a core ABI addition, and the core is frozen this phase (working rule,
    Step 30's seam excepted); (2) Shift + left-drag is already the desktop
    PINCH (README, DECISIONS_3 #34). Recommended mapping when the core
    reopens: desktop **Option/Alt + left-drag** (up = feed, down = swirl, the
    Play-mode Y axis with the mouse); tablets **long press** (~250 ms without
    travel) that turns the touch into the Play-mode bipolar Y without a note
    — push away = feed, pull back = stir — so the gesture vocabulary matches
    what fingers already do in Play mode. Sits with the Phase-6 layouts as
    the next core change.

## Step 27 — macOS release lane

31. **The macOS lane is one job plus one script, and the script is the same
    one you run locally.** `packaging/macos/release.sh <app> <version> <out>`
    does the whole release path — verify universal, `codesign` with a secure
    timestamp and the hardened runtime, notarize the app, **staple the app**,
    build the DMG around the stapled app (`hdiutil` UDZO, `/Applications`
    symlink), sign the DMG, notarize the DMG, staple the DMG, `spctl` assess
    both, sha256 — driven entirely by environment variables, so without any
    credentials it produces an ad-hoc DMG (fork, local dry run: the mechanics
    are provable on any Mac) and with them the real thing. Stapling only the
    DMG was rejected: a dragged-out `.app` would carry no ticket, and a first
    launch offline would be refused — two notarizations cost minutes and buy
    Gatekeeper-clean everywhere. The universal build is CMake's
    `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` with deployment target 12.0 (the
    Info.plist's `LSMinimumSystemVersion`); every FetchContent dependency
    builds fat, the shader compiler runs on the host arch. Tests are OFF in
    the lane (the arm64 gate already ran them); the lane re-checks `--version`
    against the tag. **On a real tag with a certificate present,
    notarization is REQUIRED** (`REQUIRE_NOTARIZATION=1`): an unnotarized DMG
    is never attached. Dry runs sign but skip the notary round trip.

32. **Credentials are the organization secrets the author's other apps
    already use — Apple ID + app-specific password, not the roadmap's "notary
    API key".** battuta and Midi Stroke sign and notarize with
    `APPLE_CERTIFICATE` (base64 .p12), `APPLE_CERTIFICATE_PASSWORD`,
    `APPLE_SIGNING_IDENTITY`, `APPLE_ID`, `APPLE_PASSWORD`, `APPLE_TEAM_ID`,
    and `TAP_PUSH_TOKEN`; the working rule says the author's infrastructure is
    an input, so the lane consumes exactly those names. FLAG: ROADMAP_4 Step
    27 lists a "notary API key" — the script also accepts an App Store
    Connect key (`NOTARY_KEY_P8` / `NOTARY_KEY_ID` / `NOTARY_ISSUER`) should
    the author switch, but nothing requires it. The certificate is imported
    into a throwaway keychain on the runner (`security import`, partition
    list for codesign) and the p12 file deleted at once.

33. **The cask bump opens a PULL REQUEST on the tap; the siblings push to
    main.** Same file conventions as `Casks/battuta.rb` (`#{version}` URL,
    `livecheck :github_latest`, `depends_on macos`, `zap trash`), same
    trigger (`release: published` — a draft has no public asset URL), same
    token. The deviation is the branch `midi-sink-<version>` + PR, because
    Step 27's DONE demands that a TEST tag exercise the whole path down to
    `brew install --cask` — and a release-candidate cask must never land on
    the tap's main. Merging the PR is the human's release act, exactly like
    publishing the draft. `TAP_PUSH_TOKEN` therefore needs
    `pull-requests: write` besides `contents: write`; if it lacks it the
    branch is still pushed and the job prints the compare URL. `homepage` is
    the frozen Step-26 URL (#22); `depends_on macos: ">= :monterey"` mirrors
    `LSMinimumSystemVersion 12.0`.

34. **"How do we test this?" — a release-candidate tag, published as a
    pre-release.** `v0.5.0-rc.1` runs the spine like any tag; the `version`
    job already marks anything with a `-` as a pre-release, so the draft is
    a pre-release draft. The human publishes it (still a pre-release),
    `publish-cask` opens a `[pre-release]` PR, and the DONE checks run
    against it: a fresh macOS user account downloads the DMG from the
    published pre-release (Gatekeeper-clean, About shows `0.5.0-rc.1`), and
    `brew install --cask ./Casks/midi-sink.rb` from the PR branch installs.
    The PR is then closed unmerged; a later real tag repeats the path and is
    merged. Nothing about the RC leaks to Homebrew users. Store submissions
    remain human (working rule) — the lane stops at "uploaded".
