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

35. **Gate calibration on the first real three-runner run (tag
    `v0.5.0-rc.1`): the CI software rasterizers run the SECOND tolerance
    tier.** #11 set the desktop gate at 1e-2 / 1e-4 from hardware evidence
    (D3D11 and GL bitwise-identical to the Metal fixture on real GPUs, Steps
    11/12); the GitHub runners have no GPU — D3D11 comes up on WARP and GL on
    Mesa llvmpipe 25.2 — and both landed at **mean 6.0e-4** from the Apple
    fixture (max 3.4e-3 on u/v, 9.3e-3 / 1.6e-2 on the ink phase at one
    band-edge texel), so both gates went red while the Metal gate was green.
    The evidence that this is rounding, not a regression: the two runners
    agree **with each other** to mean 1.8e-5 (33× tighter than either is to
    Metal); the coordinate maxima are exactly 7 and 3 half-float quanta
    (2^-11) — accumulated RGBA16F rounding over seven ping-pong passes, where
    an orientation or math error would be O(0.1); and the local Metal gate
    stays bitwise. The Apple GPU is the outlier among the three, not the two
    software rasterizers. Decision: the gate matrix carries per-runner tiers —
    **reference tier 1e-2 / 1e-4 on macOS** (the fixture's own hardware
    family), **second tier 2.5e-2 / 1e-3 on the Windows and Linux runners**
    (the tier #20 had reserved for non-reference hardware; the web tier,
    Dawn on the same Apple GPU, stays at the reference tier as measured). What
    the second tier still catches: a one-texel shift anywhere is a 2e-3 mean
    (fails); a flipped pass is O(0.1) (fails); the negative control (+0.5
    block) fails on every tier — re-verified. What it does not: sub-texel
    drifts below 1e-3 mean on CI only, which the author's real GPUs (bitwise)
    and the Metal gate (1e-4) still see. The measured headroom is 1.6–1.7×
    on both dimensions; if a runner-image update moves it, the numbers here
    are the reference for re-calibrating, and per-backend committed fixtures
    (a bitwise self-check per rasterizer) are the next step up, declined for
    now to keep 8 MB of binaries out of the tree. Also learned: Windows
    runners DO create a hardware-type D3D11 device (the earlier WARP-fallback
    worry was wrong, and no core change is needed).

36. **The `github-pages` environment needs a `v*` TAG deployment policy —
    a repository setting, recorded so it survives.** GitHub auto-creates the
    environment with "deployment branches: main only"; a tag-triggered deploy
    (`deploy-pages` from the release `web` job) is refused with "Tag … is not
    allowed to deploy to github-pages due to environment protection rules".
    Added through the API (`POST /environments/github-pages/deployment-branch-
    policies {name: "v*", type: "tag"}`) alongside the existing `main`
    branch rule; a fresh fork or a re-created environment needs it again. The
    `web` job also runs `actions/configure-pages` with enablement on, so
    enabling Pages itself is no longer a human step; the custom domain and
    its DNS CNAME still are (#22).

## Step 28 — iOS release procedure (manual by design)

37. **The iPad app's version comes from the tag through xcodegen's
    environment substitution; `ios/prepare_release.sh` is the one entry
    point.** `project.yml` reads `MARKETING_VERSION`, `CURRENT_PROJECT_VERSION`
    and `SumiBuildDescribe` from the environment; the script derives them
    from `git describe --tags --always --dirty` exactly as the desktop build
    and the spine do (#3), rebuilds `libsumi.a` / `libhostmpe.a` for iOS with
    `SUMI_APP_VERSION` injected (the app links the PREBUILT archives — a stale
    core has shipped to the iPad before), then runs xcodegen. Two App Store
    Connect facts shape the mapping: `CFBundleShortVersionString` must be
    numeric `X.Y.Z`, so a release-candidate tag's `-rc.N` cannot appear in it
    — it lives in `SumiBuildDescribe` and in the About sheet; and build
    numbers must strictly increase per version, so `CFBundleVersion` is the
    commit count of HEAD (monotonic on `main`, unique per commit, no file to
    edit). The settings sheet gained an **About** section showing
    `midi-sink X.Y.Z (build) · describe` and `libsumi a.b.c` — the DONE check
    "installed build is the tag" is read there. `ITSAppUsesNonExemptEncryption
    = false` in the plist (CoreMIDI/BLE/RTP-MIDI, no custom cryptography)
    answers the export-compliance prompt once for every upload.

38. **No iOS job in Actions at all — not a lane, not a compile check.** The
    roadmap's Step 28 text asks PR CI to keep "a build-only compile check for
    iOS"; the author decided otherwise during the step ("the iOS build is
    only made locally, never in an action"), so `build.yml` carries no iOS
    (or Android) job — FLAGGED as a deliberate deviation from ROADMAP_4. What
    guards the tag instead is the procedure itself: `ios/prepare_release.sh`
    (core rebuilt with the tag's version, project regenerated, values echoed
    for a human to read) before every archive, and the About check on the
    iPad after. The procedure is `ios/RELEASING.md` (checkout tag → prepare →
    Archive → Distribute/Upload with Xcode's build-number management OFF →
    ASC TestFlight internal, then external for the beta wave → About on the
    iPad reads the tag); the listing text, URLs (the frozen Step-26 pages),
    review notes and screenshot plan are staged in `ios/metadata/`.
    Screenshots are author input from real sessions.

## Step 29 — Windows release lane

39. **The installer is Inno Setup, hand-authored, per-user.** The roadmap left
    the technology open (Inno and NSIS are both on the runners; the siblings'
    NSIS is Tauri-GENERATED, so it is no precedent for hand-authoring).
    `packaging/windows/midi-sink.iss` is ~90 readable lines: stable AppId
    GUID, `PrivilegesRequired=lowest` (installs to
    `%LOCALAPPDATA%\Programs\midi-sink` — no UAC, the scope winget expects
    for user packages), Start-menu entry, uninstaller + registry record,
    optional desktop icon (unchecked). **Settings survive uninstall unless
    the user opts in**: the uninstaller asks once, interactively only —
    a silent/winget uninstall never touches `%APPDATA%\midi-sink`. Verified
    end-to-end locally: silent install → installed `--version` → silent
    uninstall → app and Start-menu gone, settings intact.

40. **Windows builds use the STATIC CRT, and the exe carries VERSIONINFO from
    the tag.** Found packaging the first installer: /MD linked
    MSVCP140/VCRUNTIME140, which a clean Windows VM does not have — and a
    per-user/no-UAC installer cannot install the redistributable, so the DONE
    check ("runs on a clean VM") would fail by construction.
    `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded` makes the exe self-contained
    (dumpbin: system DLLs only; +~0.9 MB); the whole tree links statically, so
    one flag covers it. `desktop/midi-sink.rc` gained a VERSIONINFO block
    (Explorer Properties / SmartScreen / winget metadata) fed from
    `SUMI_APP_VERSION` — the numeric `X,Y,Z,0` comes from the semver part the
    root CMakeLists already extracts, so no number is hand-edited (#3).

41. **Windows signing consumes `WINDOWS_CERTIFICATE` (base64 .pfx) +
    `WINDOWS_CERTIFICATE_PASSWORD` — proposed names, FLAGGED for the author.**
    The handoff says a certificate was never mentioned, so the lane is
    unsigned-first per the spec: without the secrets it prints a notice and
    ships, and the SmartScreen consequence ("More info → Run anyway", once
    per new version) is documented in the README and the install page, not
    fought. With the secrets, signtool signs midi-sink.exe (before ISCC) and
    the setup exe (after), RFC-3161 timestamped, and verifies both. The
    names mirror `APPLE_CERTIFICATE`; if the author's cert lands under other
    names, only the `windows` job's env block changes.

42. **winget: pre-release tags are SKIPPED by `publish-winget.yml`; RCs are
    tested from an in-tree manifest instead.** winget-pkgs takes releases
    only, and unlike the cask flow (a PR that can be tested and closed) there
    is no safe pre-release path. The conventions live in
    `packaging/windows/winget/Vibetuned.MidiSink/` (validated:
    `winget validate` passes) with `stage.ps1` to fill version + sha256 from
    any PUBLISHED tag, so the RC DONE check is
    `winget install --manifest packaging\windows\winget\Vibetuned.MidiSink`.
    Two facts recorded so they are not re-learned: winget parses EVERY file
    in the directory it is pointed at (a README beside the manifests breaks
    validation — hence the subdirectory), and the first winget-pkgs
    submission stays human (`wingetcreate new`, moderation wants a human on
    the initial PR) — until it merges the workflow detects the missing
    manifest and skips.

43. **HiDPI on Windows was real and is fixed in the shared settings window
    (the step owns Windows-motivated fixes; the change is a no-op on the
    other platforms by construction).** Step 23's `scale_` was captured but
    never applied, so at 125 % the window rendered 560×760 physical pixels
    with ~13 px fonts. The fix uses the backend's own helper
    (`ImGui_ImplGlfw_GetContentScaleForMonitor` — returns 1.0 on Apple, where
    Retina lives in FramebufferScale, and on Wayland, where the compositor
    scales): window created at 560×760 × scale, `style.ScaleAllSizes(scale)`,
    `FontScaleMain = 1.15 × scale`. Verified at 125 %: 700×950 window, crisp
    correctly-sized text. macOS/Linux lanes should re-verify visually but the
    helper's platform table is exactly why it was chosen. Also fixed on the
    way: the first-run log line carried a double-encoded em-dash (mojibake in
    a cp1252 console) — ASCII now, matching #9c's ASCII-UI rule.

## Step 30 — Linux release lane

44. **The Linux package is a CPack DEB of the `desktop-integration`
    component, built on `ubuntu-22.04`; the Debian version maps `-` to `~`
    while the file name keeps the tag.** `cmake/LinuxPackaging.cmake`
    (included by the root list on Linux only) sets `CPACK_DEB_COMPONENT_INSTALL`
    with `ALL_COMPONENTS_IN_ONE` grouping so the package carries exactly the
    ten files Step 14 installs — the binary, the absolute-`Exec` `.desktop`,
    seven hicolor icons, and `copyright` — and none of the ~190 GLFW /
    libremidi headers a monolithic `cpack` swept in on the first try. The
    Step-14 install rules were made DESTDIR-safe (the generated `.desktop`
    goes under `$ENV{DESTDIR}`, the icon-cache/desktop-database refresh is
    skipped when staging; dpkg's file triggers do that job on a real install,
    so there are no maintainer scripts). Version: `0.5.0-rc.5` becomes
    package version `0.5.0~rc.5` (tilde sorts before the release, so `apt`
    upgrades an RC to the release and never the reverse), the asset stays
    `midi-sink_0.5.0-rc.5_amd64.deb` per the LANE INTERFACE. Dependencies:
    shlibdeps sees only what is linked (libc6, libgcc-s1, libstdc++6,
    libopengl0) — GLFW and libremidi `dlopen` X11, Wayland, EGL, xkbcommon,
    libdecor and ALSA at run time, so those are listed by hand
    (`libasound2t64 | libasound2` covers 24.04's t64 rename and 22.04).
    Runner: `ubuntu-22.04` for glibc 2.34 reach (a 24.04 build needs 2.38);
    22.04's own CMake 3.22 and gcc 11 are too old for this tree, so the lane
    takes CMake from Kitware's jammy repository and `gcc-12` (probed in a
    `ubuntu:22.04` container: builds, `--version` prints the injected tag).
    The lane also ships `midi-sink-<v>-linux-x64.tar.gz` — the bare binary,
    LICENSE and a README.txt — as the portable-zip sibling; it then installs
    the deb into a clean 22.04 container, runs `--version`, checks the
    `.desktop`/icon and removes it, before anything is attached. FLAGGED: the
    handoff states the local RTX gate "holds bitwise"; measured here on GL
    4.1 it is mean 6.85e-6 / max 3.9e-3 — green at the reference tier
    (1e-2 / 1e-4 on the mean), not bitwise. Nothing was loosened.

45. **The apt repository is rebuilt from every published release by
    `publish-apt.yml` on `release: published`, with a `stable` suite for
    releases and an `rc` suite for pre-releases.** The Pages tree is
    produced at tag time by the `web` job, when the draft has no public
    asset URLs, so the repository cannot be composed there. `publish-apt`
    checks out the tag, rebuilds `site/` with `SITE_VERSION` = the tag, unpacks
    the release's `midi-sink-<v>-web.tar.gz` under `pages/marble/` (no emsdk),
    downloads the `*_amd64.deb` of every non-draft release into
    `pool/<suite>/`, writes `Packages(.gz)` / `Release` / `Release.gpg` /
    `InRelease` per suite (`apt-ftparchive`, `gpg` with `APT_GPG_PRIVATE_KEY`),
    exports the public key as `apt/midi-sink.asc`, and redeploys Pages
    (`concurrency: pages`, the environment already allows `v*` tags, #36).
    Without the secret every step prints a notice and the job ends green,
    like the siblings. Two departures from battuta's pattern, recorded: the
    suite split (a user on `stable main` never receives an RC; testers add
    `rc main`), and rebuilding from ALL releases each time, so the pool is
    the release list and an accidental double publish is idempotent. Users
    install with a keyring under `/etc/apt/keyrings/midi-sink.asc` and
    `deb [signed-by=…] https://midi-sink.vibetuned.com/apt stable main`
    (install page + README). Verified locally end to end with a throwaway
    key: the same commands produce a repository a clean `ubuntu:24.04`
    container adds, `apt update`s without a signature warning and installs
    `midi-sink` from (`docs/evidence/step30/aptlocal.log`).

46. **Flatpak: spike, not a channel.** `packaging/linux/flatpak/
    com.vibetuned.midi-sink.yml` is the manifest (freedesktop 24.08 runtime,
    cmake-ninja module, `--component desktop-integration` install into
    `/app`, `rename-desktop-file`/`rename-icon` for the reverse-DNS id). What
    the spike established before building: (a) there is no portal for
    `/dev/snd` — ALSA raw/sequencer MIDI inside the sandbox needs
    `--device=all`, which surrenders the device isolation that is flatpak's
    point, and PipeWire's MIDI bridge is not a substitute for an app that
    opens ALSA sequencer ports through libremidi; (b) the FetchContent
    dependencies need `--share=network` at build time, which Flathub forbids
    (they would have to become vendored sources); (c) `.desktop` and icon
    names must be renamed to the app id, so a flatpak install and the deb
    would present two different desktop entries. Verdict: **closed** — the
    deb, the apt repository and the tarball are the Linux channels; no
    publish hook. The spike was then BUILT and RUN (the author installed
    `flatpak-builder`): with `--share=network` at build time the module
    compiles against the freedesktop 24.08 SDK (`no-debuginfo` because the
    box lacks elfutils), installs as `com.vibetuned.midi-sink` with the
    renamed desktop entry and icon, `--version` prints the injected version,
    and the app renders on the host's NVIDIA GL through `--device=dri`.
    ALSA: with `--device=all` the sandboxed app opens every sequencer port
    the host has (Midi Through and the tablet's USB port at the time of the
    run — the ROLI had been unplugged after the capture); with
    `--nodevice=all` it opens **none** — (a) confirmed, the verdict stands.
    The manifest stays in the tree as the record; nothing publishes it.

## Step 31 — Android release procedure (manual by design)

47. **Android versions come from git in `build.gradle.kts`; there is no
    Android CI job (author's decision, confirmed, mirroring #38).**
    `versionName` = the numeric `X.Y.Z` of `git describe --tags` (Play wants
    a plain string, as ASC does), `versionCode` = `git rev-list --count HEAD`
    (monotonic, unique per commit — Play requires strictly increasing codes
    across tracks), the full describe goes into `BuildConfig.BUILD_DESCRIBE`
    and into `-DSUMI_APP_VERSION` for the core; `SUMI_APP_VERSION` in the
    environment overrides the describe. The settings sheet's About row reads
    `midi-sink X.Y.Z (versionCode) · describe` and `libsumi a.b.c` (a new
    JNI `nativeCoreVersion` returns `sumi_version()`), the on-device DONE
    check. `android/prepare_release.sh` prints the triple, warns on a dirty
    tree or off-tag checkout, and exits non-zero if Gradle's values differ
    from git's. Unlike iOS there is no stale-archive trap: Gradle compiles
    the core from source on every build. FLAGGED against ROADMAP_4 Step 31's
    "PR CI keeps a build-only compile check for Android": asked, the author
    chose "no Android job, like iOS" — `build.yml`'s comment already states
    it; the tablet apps are built from a tagged checkout in Android Studio
    only. `android/RELEASING.md` is the checklist (signed bundle with the
    author's upload keystore → Play internal track → About on the Galaxy Tab
    → USB-MIDI to this box); `android/metadata/` holds the listing, Data
    safety (nothing collected or shared), the frozen privacy/support URLs
    and the screenshot plan.

48. **Android paper dip = two buttons in the settings sheet, and a saved print
    is a PNG in the user's gallery (`Pictures/midi-sink`, MediaStore).**
    Author's request during Step 31: the tablet had `nativeTriggerDip` but no
    button. "Paper dip — save the print" dips, waits for the core's async
    readback (§5.3) on the render thread, hands the RGBA print to Kotlin as
    ARGB and writes it through `MediaStore.Images` (no storage permission on
    API 29+, our minSdk; `IS_PENDING` until the PNG is complete); "Paper dip —
    discard (fresh sheet, no print)" dips and frees the buffer. Both READ the
    print: the core keeps two print buffers and `sumi_trigger_paper_dip`
    REFUSES while both are busy, and only `sumi_read_print` frees one — so a
    dip nobody reads leaks a buffer, and the third dip is silently refused
    (a warning in the log, no visible effect). The Android shell therefore
    drains unread prints before every dip. FLAGGED for the iOS owner: the
    iPad's "Paper dip (fresh sheet)" (SumiApp.swift, #67) never reads the
    print, so it stops working after two dips per launch — same fix
    (read-and-drop after the readback lands) or the same two buttons. Not a
    core change: the core's contract is as specified (§5.3 double-buffered
    prints, host consumes); the shells had not been consuming.

## Step 33 — Feedback incorporation (first batch, while the beta runs)

49. **The two pressure operators became Marble-mode gestures through two
    additive enum values, not new functions.** `sumi_add_vortex` gained the
    profile `SUMI_VORTEX_LAMB_OSEEN` (2): it pushes the §4.3(7) swirl pass the
    voice mapper already emits, the host supplying `strength` = Γ·Δt (signed)
    and `radius` = r_c. `sumi_add_drop` gained the layer `SUMI_DROP_FEED` (2):
    the drop shader's interior branch copies the CENTRE texel (band, aux and
    pre-image) instead of writing a new phase, so a pass of radius
    sqrt((R+ΔR)² − R²) widens the band already there — the §3.4/§4.4 boundary
    growth with the host tracking R; the drop counter is untouched. ABI
    `sumi_version()` 0.5.0 → **0.6.0**, additive (the struct is unchanged; a
    host passing layer 2 before this got a clear drop). The §4.6 field script
    does not use either value, so every fixture stands; the regression test
    for the new passes is the desktop `--pressure-test`. **The gesture**
    (identical constants on desktop, web, iOS, Android): a **long press**
    (250 ms, no travel; Shift + right button with a mouse) lays an ink drop
    and becomes Play mode's bipolar Y axis without a note — hold or push UP
    = feed (ΔR = 0.12·(0.35 + up)·dt canvas heights/s, so a still hold
    grows slowly and a push fast), pull DOWN = swirl (Γ·Δt = 3.0·down·dt·2π·R²,
    r_c = R, i.e. 3 rad/s of core rotation at full pull; feeding pauses while
    stirring); travel 0.15 canvas heights for full effect. The drop stays
    where it was pressed; the finger only modulates. Shift + LEFT drag stays
    the pinch (the user's first suggestion clashed with it, #30). Android is
    written to the same design but not compiled on this machine — the Linux
    box verifies it. Spec flag: PROJECT_SPEC §5.3 carries the header verbatim
    and §8.1 lists the Marble gestures; both are stale until the author folds.

50. **The Airwave default CC map is the measured one.** The capture
    (`docs/evidence/airwave-mapping`) showed twelve CCs 20–31 in left/right
    pairs — Grasp 20/21, Slide 22/23, Glide 24/25, Raise 26/27, Tilt 28/29,
    Flex 30/31 — against a default table that imagined 20 = Raise, 21/22 =
    Glide, 23 = Tilt, 24/25 = Flex and left 26–31 dead. New defaults
    (`install_default_cc_map`, the desktop mirror, README, the devices page,
    the chart): **left hand = the water** — 26 Raise L → vortex strength, 24
    Glide L → centre X, 22 Slide L → centre Y, 30 Flex L → paper roughness,
    28 Tilt L → ripple wavelength; **right hand = the material and the waves**
    — 29 Tilt R → viscosity, 31 Flex R → palette morph, 27 Raise R → ripple
    amount; 20/21 Grasp, 23 Slide R, 25 Glide R free for the editor.
    `INK_FLOW` was not given an Airwave CC: it only acts in wind mode. This is
    a taste assignment (Step 33: UX-feel needs the author's sign-off) — every
    row is a one-line remap in the settings window if the author prefers
    another hand. The normalizer goldens that pinned CC 20/21/23 as defaults
    moved to 26/24/29, and the "unmapped" examples to 20/21.

51. **A dip never silently fails: the core recycles the older unread print.**
    `sumi_read_print` is a SYNCHRONOUS copy, so no host ever holds a core
    buffer between calls; the only unsafe overwrite is a readback still in
    flight. `dip_ready()` therefore refuses only while `pending_idx >= 0`
    (a few frames), and `snapshot_print` reuses the lower-`buf_seq` READY
    buffer when both are unread (INFO log). This is the core-side fix for
    #48's finding (iOS's dip stopped after two per launch; desktop and web
    had the same latent stall, and the web page's Replay button dips on every
    replay — the third replay would have been refused) and it needs no host
    drain. iOS gained Android's two buttons — *save the print* (RGBA8 →
    CGImage → `UIImageWriteToSavedPhotosAlbum`, waited for on the display
    link; `NSPhotoLibraryAddUsageDescription` added) and *discard*. The header
    comment for `sumi_trigger_paper_dip` states the new contract; the mapper's
    `dip_allowed` path is unchanged and its test still holds.

52. **Jaffer's three remaining patterns, read from the papers, and where each
    belongs (design, not yet built).** *Oseen Flow in Paint Marbling*: the
    stroke's velocity field is closed-form — F_x = U(rL − y²)/(rL·e^{r/L}),
    F_y = U·xy/(rL·e^{r/L}) in the stroke frame (L = the viscous length, U the
    speed) — but Jaffer shows the DISPLACEMENT is not (§6–7: "unlikely… to be
    expressible in closed form"); he applies the velocity field in ⌈λ/L⌉
    Euler steps and accepts imperfect reversibility (his Fig. 13). Plan: a
    sub-stepped pass `P_src = P − d·F̂(P)` per step with d ≤ L/2 (our wake's
    discipline), the viscous sibling of the inviscid wake: bands compressed
    ahead, spread perpendicular, the trailing V. Gesture: the pen with a
    "viscous" tip setting, or a second stylus mode. Honest label: exact
    velocity field, approximate map — the first operator in the book that is
    not exact. *Pigment Transport*: the **Spanish wave is a TRANSFER-TIME
    mapping, not a paint deformation** — the paper moves sinusoidally while
    being laid: parallel term f(a) = a + (A/2)·sin(2πa/λ) (monotonic iff
    |πA/λ| < 1, inverted by his power-law seed g(a) + one Newton step, eqs
    5–6), perpendicular term −(B/2)·cos(2πa/λ), and the **tint**: pigment
    thins as 1/f'(a), colour^γ with γ = f'(a) (eq 7) — which is the 3-D
    shading that makes the pattern. Our live ripple IS the B term without
    shading; the Spanish wave therefore extends the composite/print path:
    parallel A, the γ shading, and — by physics — it BELONGS in the print
    (the paper moving while touching the water), unlike the live ripple.
    **Turkish moiré** = Spanish wave with curved shading contours and the
    tint reversed for dark paper (eq 9, two branches by paper vs paint
    tone); it needs the paper-tone parameter and a curved `a`. *Drop shading*
    (eq 8): pigment thickness exp(−ζa²/r²) normalised — our ink channel
    carries the radial coordinate, so it is a cheap composite term with one
    ζ. Order proposed: (1) Spanish wave + tint in the composite (print-time,
    parameters A/B/λ/θ/Ω + shading on/off), (2) drop shading ζ, (3) Turkish
    moiré as the dark-paper branch with a curved axis, (4) the Oseen stroke
    as a new sub-stepped pass. Each is a core change under Step 33's
    unfreeze with its own regression fixture; the §4.6 script grows only for
    (4) (the others do not touch the field).

53. **The viscous stylus stroke is the 2-D unsteady Stokeslet displacement of
    an impulse spread over the tip — closed form, verified, ABI 0.7.0.** The
    author proposed the impulsive point force in unsteady Oseen/Stokes flow as
    an exact operator (Galilean reduction to the comoving frame; displacement
    = time integral of the Green's tensor). Three corrections shaped it: (a)
    the layer is 2-D, so the kernel is the 2-D one (E₁ and Gaussians, not the
    3-D erf/r form); (b) a point impulse has infinite displacement at the
    point (log-singular in 2-D), so the force is a Gaussian blob of the tip
    radius a — done exactly as the difference of two point kernels, at
    diffusion times t+t₀ and t₀ with a² = 4νt₀, which keeps the field exactly
    divergence-free; (c) it is the LINEARIZED (Eulerian) displacement, area-
    preserving to first order per pass, hence sub-stepped like the wake — not
    an exact homeomorphism, and neither is Jaffer's Oseen stroke, which he
    iterates. **Derivation:** ψ = (F/ρ)·y·(1−e^{−s})/(2πr²), s = r²/4ντ;
    u = (∂_yψ, −∂_xψ); ∫₀ᵗ u dτ with S = r²/4νt gives, for D₀ = F/(ρν),
    d_x = (D₀/8π)[χ(S)+E₁(S)] − (D₀/4π)(y²/r²)χ(S), d_y = +(D₀/4π)(xy/r²)χ(S),
    χ = (1−e^{−S})/S. Blob: S₀ = r²/a², S₁ = r²/ℓ², ℓ² = a²+4νt; normalising
    the centre displacement to the tip's motion d gives D₀ = 4πd/ln(ℓ/a) and
    the kernel in the header comment of `sumi_deform_stokeslet_t`.
    **Verified** (`docs/evidence/step33/stokeslet_verify.py`): E₁ to 1e-10;
    closed form = numerical ∫u dτ in both components at three points (the
    first draft had d_y's sign wrong — caught by exactly this check); blob
    kernel divergence 1e-11; mirror-symmetric; d(0) = d; far field
    d/ln(ℓ/a)·(ℓ²−a²)/(2r²) — a doublet tail; max|∇d| per (d/a) = 0.98 at
    ℓ/a = 1.5 falling to 0.42 at 8, so **the wake's d ≤ a/4 sub-step keeps
    |∇d| ≤ 0.25 for every ℓ/a ≥ 1.5** (fold-free needs d < 1.0 a at worst).
    **Core:** `SUMI_DEFORM_STOKESLET` pass (`stokeslet_fs`: E₁ by series
    below 1 with the two logs cancelled analytically near the centre, A&S
    5.1.56 rational above; χ by series below 1e-3), `sumi_add_wake` picks it
    when `params.wake_profile == 1`, `params.wake_spread` = ℓ/a clamped
    [1.5, 12] (default 3); the struct grew → `sumi_version()` 0.7.0, hosts
    rebuild. The §4.6 script is untouched (fixtures stand); the pass has its
    own `--stokeslet-test`: tip texel moves by d (0.0100 = d), mirror to a
    half-float ULP (the two rows sit in different exponent bands), one a/4
    pass has pre-image det ≥ 0.75 and mean 1.00012, a 10a stroke in one call
    is fold-free outside the swept corridor (inside it, bilinear resampling of
    the strongly compressed pre-image aliases and finite differences stop
    measuring the map — the same exclusion the wake's flick test makes).
    **Picture:** bands ahead of the tip compress into a point and spread
    perpendicular, a sharp V trails — Jaffer's tank observation (Oseen §4),
    where the doublet threads rings around a rigid hole. Surfaced as "Stylus
    wake: inviscid doublet / viscous stroke" + spread on desktop, the web
    panel and the iOS sheet; the Android bridge exists, its sheet row is the
    Linux box's. The docs' `viscous` scene exposes a, ℓ/a, d. Supersedes the
    Oseen-field plan in #52; the Spanish wave stays deferred (print-time UI).
    **Next, agreed in principle:** the Airwave right hand as a hand in the
    water — Glide R / Slide R its position, its motion delivering these
    impulses (force = hand velocity, so a still hand does nothing), Grasp the
    delta-driven pinch there — needs global dimensions for a moving force
    point and a pinch delta; not in this batch.

54. **The stylus draws its wake in Marble mode too — it never did on the
    tablets.** Spec §8.7: "the dipolar wake rides every stroke segment in both
    modes". Both shells implemented the pen only inside the Play overlay,
    which Marble mode hides and makes interaction-inert, so a Pencil or S-Pen
    in Marble mode fell through to the finger handlers: a tine on drag, a drop
    on tap, and since #49 a pressure long press. Fix, host-side on both:
    iOS — the Marble recognizers (tap, pan, twist, pinch, long press) accept
    DIRECT touches only (`allowedTouchTypes`), and `SumiCanvasView` handles
    `.pencil` touches itself in Marble mode with `sumi_add_wake` and the
    overlay's tip mapping (a = 0.006 + 0.030·force, force in UIKit units
    clamped like the overlay); Android — `SumiSurfaceView.onTouchEvent`
    routes `TOOL_TYPE_STYLUS` / `_ERASER` pointers to `nativeAddWake` with the
    overlay's `0.006 + 0.030·pressure`, cancels the long-press timer, and lets
    fingers keep their path. No hostmpe, no MIDI: the wake is physical (§8.7).
    Desktop (middle-drag) and web (pointerType 'pen') already did this. The
    Android side is written, not compiled here (handoff).
