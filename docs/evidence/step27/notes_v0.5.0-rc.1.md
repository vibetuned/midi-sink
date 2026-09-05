# midi-sink v0.5.0-rc.1

_Phase 5, first release candidate: product, spine, web, docs, macOS lane (steps 23–27)_

The engine leaves the lab. The desktop harness became the product (a real
macOS `.app`, a settings window shared by macOS/Windows/Linux, a lab bench
behind `--dev`); one tag-triggered workflow is the release spine with the
§4.6 field regression as its gate on every renderer; the core gained its one
permitted Phase-5 seam — a WebGPU swapchain — and the same bytes now run in
the browser; a documentation site with live demos of every operator owns
`midi-sink.vibetuned.com`; and the macOS lane signs, notarizes and staples a
universal DMG that bumps a Homebrew cask. Version strings come from the git
tag everywhere; no number is edited by hand. iOS and Android stay manual
procedures by design. Decisions: `DECISIONS.md` Part IV #1–#34
(`_work/DECISIONS_4.md` until the phase folds).

### Step 23 — Desktop productization (macOS)
One name, `midi-sink`, bundle id `com.vibetuned.midi-sink` (#1). The
**settings window**: Dear ImGui v1.92.9b in a second GLFW window with its own
GL 3.2 context, one implementation for all three desktops (#2) — layout &
look, expression routing, ripple, a CC-map editor on a host-side mirror (#7),
the live MIDI-input list with rescan age (#8), paper dip and print export,
About with app version · commit · engine version; `⌘ ,` / `Ctrl ,` reopens
it. Settings persist in a plain INI in the platform config directory (#4).
**`--dev`** gates the whole lab bench — every debug key, scripted flag and
`--field-dump` — so a release build accepts only `--help`/`--version` (#5).
macOS is a real `.app` with Info.plist, `.icns`, hardened-runtime
entitlements and an ad-hoc signature straight out of the build; the runtime
Dock-tile hack retired (#6). `SUMI_APP_VERSION` is a CMake cache variable
injected from the tag, defaulting to `git describe` (#3). Three Cocoa facts
fixed along the way: GLFW's `chdir` into Resources, off-screen settings
placement, and the Latin-1 font (#9). A push/PR workflow builds and runs the
headless suites on ubuntu, windows and macos.

### Step 24 — Release orchestration spine
`release.yml`: a `version` job (tag → `X.Y.Z[-pre]`, release notes from this
file, `--strict` on real tags), a `gates` matrix (macOS/Metal, Windows/D3D11,
Ubuntu/GL under Xvfb + Mesa) that builds with the injected version, runs the
suites, asserts `--version` carries the tag and runs the **§4.6 field
regression through each real renderer** with a negative control that proves
the gate can fail (`tools/field_gate.py`), and a `publish` job that drafts a
GitHub release from every lane's `dist-*` artifact — pre-release when the
version carries a `-`. Humans publish; channel bumps are separate
`release: published` workflows. The lane interface is documented at the top
of the file: four CI lanes (web, macOS, Windows, Linux), one job each;
iOS and Android manual (#10–#14).

### Step 25 — WebGPU backend & marble web
The core's fourth swapchain TU, `swapchain_webgpu.cpp` (sokol `SOKOL_WGPU`
through Emscripten's Dawn port), `SUMI_BACKEND_WEBGPU` and
`sumi_webgpu_surface_t` in the ABI (`sumi_version()` 0.5.0, additive), the
WGSL dialect in the shader pipeline, and a readback seam every backend
implements (injected copy-source textures, non-blocking begin/poll field
read). `web/`: the C-ABI as the wasm export surface, a page of JS as the
host — pointer/touch/pen gestures, Web MIDI on Chrome/Edge, a lil-gui panel
mirroring the desktop settings window, and the **scene API**
(`?scene=…&param=…&embed=1`) whose sliders are the formulas' symbols. The
web tier joined the §4.6 regression at the desktop tolerance (max |Δ|
9.8e-4 vs the Metal fixture); Lighthouse mobile performance 1.0, first
marble 252 ms. `tools/web_gate.mjs` drives headless Chrome for the field
dump, the scene sweep and page captures. Found on the way: the port never
exports `Module.WebGPU` (device via `preinitializedWebGPUDevice`), map
callbacks must be `AllowSpontaneous`, and the "no WebGPU" overlay that hid a
working canvas was a CSS specificity bug (#15–#21).

### Step 26 — Documentation site
`site/`: Astro Starlight with KaTeX, deployed from the release tag next to
the marble app (`/marble/`) at `https://midi-sink.vibetuned.com/` — frozen
URLs `/`, `/privacy/`, `/support/` for every later lane and store listing
(#22). Five books and a chart: user guide (10 pages), **The Operators** (one
page per deformation — formula, invariants, ownership rule, and a live embed
of the release wasm; no second implementation, enforced by a post-build
check), architecture, a performance gallery rendered from a runtime manifest
(the Jaffer tribute piece identified from his own video credits as *Ali
Paşa*, #27), design notes and this changelog generated verbatim from the
repository (#24), and the **MIDI implementation chart** — data rendered by
the page and verified against the Play-mode byte logs by
`tools/chart_check.py` (#26). Citations checked against the publications.
After review the operator scenes were paced (frames per step) and two-sited
so the mathematics is seen taking effect, the pressure feed got its own scene,
and the swirl shows its far field (#29); a Marble-mode feed/swirl gesture is
recorded for the Step-33 unfreeze (#30).

### Step 27 — macOS release lane
`packaging/macos/release.sh`, one script for CI and local runs: universal
(arm64 + x86_64) `.app` → Developer ID signature with a secure timestamp →
notarize → **staple the app** → DMG → sign → notarize → staple the DMG →
`spctl` assess → sha256; ad-hoc without credentials, notarization required
on a real tag with a certificate (#31). The `macos` job in the spine
attaches `midi-sink-<version>-macos-universal.dmg`; `publish-cask.yml` opens
a **pull request** on `vibetuned/homebrew-tap` when a release is published,
so a release-candidate cask never reaches the tap's main (#33). Credentials
are the organization secrets the author's other apps use (#32); a
release-candidate tag published as a pre-release is the end-to-end test (#34).

---
Notes generated from `docs/CHANGELOG.md` by the release workflow at commit `c484861`.
