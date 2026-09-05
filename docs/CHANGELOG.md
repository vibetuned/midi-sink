# Changelog

Condensed from the per-step DONE evidence (`docs/evidence/` in git history,
removed from the working tree when each phase ships — last after step 22).
Spec: `PROJECT_SPEC.md`; decision log: `DECISIONS.md` (Part III = Phase 4,
referenced below as `DECISIONS_3 #n`).

## v0.5.0 — Phase 5, first release candidate: product, spine, web, docs, macOS lane (steps 23–27)

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

## v0.4.0 — Phase 4: Touch & Stylus MPE Play Surface (steps 15–22)

The tablets become MPE instruments: every touch is a joystick on a pitch
lattice, the pencil plays legato, a floating control strip rides the master
channel, and the generated stream feeds both the loopback visualizer and
real DAWs over USB, virtual, network and BLE transports. All host-side MPE
generation lives in one shared pure-C library, `hostmpe/`, consumed by Swift
(module map) and Kotlin (JNI) — one implementation, 1,569 headless checks,
zero drift between platforms. The core grew four deformation operators
(v0.4) and the layout probe; otherwise it stayed frozen.

### Close-out (after step 22)
Pencil Pro **squeeze = sustain pedal**, the S-Pen barrel button's twin
(#62): `UIPencilInteraction` drives the same `hostmpe_strip_t` engine as the
palette's pad, transition-only, Play-mode only; Pencil 2 double-tap latches.
iOS byte log adopts Android's `src` taxonomy (pen = 4, #63) so
`tools/pen_trace.py` and `tools/midi_asserts.py` read both platforms'
logs — the §3.3 stylus-only CC74 rule became provable (1,237 pen / 0 finger
CC74 in a live session). In-app **evidence capture** (timed full-screen PNG
burst through the render-server path, which carries Metal content) and a
log flush, pulled with `devicectl device copy from` (#64). **Echo
suppression** in hostmpe (#66): a live iPad log showed 99.5% of "external"
MIDI was our own output mirrored back (median 0.3 ms), marking our own
channels externally held and painting every note twice; every delivered
byte is now recorded at the single transport emit point and matching input
inside 300 ms is consumed — the post-guard session carries zero external
rows and passes every assert. **The sustain pedal no longer wipes the
canvas** (#67): `CC 64 → paper dip` is honoured only outside MPE mode
(§2.4's classic-keyboard mapping survives); the dip became a deliberate
settings control. Flagged, unchanged: with the bend booster engaged, a cell
crossing is no longer pitch-continuous (#68 — 69% of a free-play session's
crossings carried boosted bends; expressive by design, options recorded).

### Step 22 — Android port (Galaxy Tab S8 Ultra, on the Linux box)
Full parity: `sumi_play.cpp` hosts `hostmpe_t`, the strip engine, one
limiter per transport and the byte log on the **AMidi poller thread** (the
§5.2 single producer, #46); a params snapshot lets the instance-free probe
answer on the UI thread (#47). Kotlin `PlayOverlayView` (probe-swept
two-tone lattice, joystick indicators, fingers, S-Pen, hover), floating
`ControlStripView` with the long-press CC editor, `MidiOutputs` (**USB
gadget as primary sink**, `MidiDeviceService` virtual device, a hand-rolled
BLE-MIDI GATT peripheral — MidiManager is central-only, #44), Marble pinch.
Measured on the Linux side: gadget appears in `amidi -l` within ~150 ms of
the USB mode flip; MCM/RPN 0 = 48 on 15/15 members in wire order on USB and
BLE simultaneously; **BLE − USB latency +29.8 ms median**; the full hostmpe
(1,559) + normalizer (14,997) suites pass on-device; GLES3 field dump
reproduces the Metal fixture numbers exactly; a **10-minute mixed session**
(three sinks + 10-voice storm + scripted play) — 834,018 wire messages,
1,149/1,149 notes released, worst slot 92/s under the 100 Hz policy, +1 ms
cumulative lag, 0 dropped, 120 fps, PSS flat at 253 MB, 0 EGL errors. An
independent review of the diff fixed real bugs (#54): an inverted §5.4
teardown block, a lost command-queue wakeup, an `AMidiDevice` double-free,
duplicate ingestion after relaunch, BLE flow-control stalls, and two false
greens in the analysers (now negative-tested). Late additions: the piano
grid's accidental knob made aspect-invariant in the core (#57), the S-Pen
barrel button as sustain (#58), a natural's drawn cell re-centred on its
octave pair (#60), the black-key glissando corridor (#61); a black-key
glissando experiment rolled back in full (#59).

### Step 21 — Stylus legato, wake & pinch (iOS)
The pen redesigned same-day into **per-cell legato retriggers** (#39): the
shell probes the cell under the tip each move, a crossing emits
bend→Note On(live-force velocity)→old Note Off on ONE channel (the legato
overlap idiom — DAWs record real notes, mono/MPE synths glide), the in-cell
offset is the bend, ±0.65 st boundary hysteresis keeps vibrato from
machine-gunning at cell edges; the ±47 re-anchor and piano retune ramps were
retired. Velocity in UIKit force units (baseline tap = 96, force 3 = 127,
#38). Barrel controls became derivative-only **gestures** (#40, four
iterations): the Pencil Pro roll multiplies the in-cell bend ×1→×3 with a
0.4 s decay; azimuth tail-stir is the fallback; tilt→CC1 and every
orientation→vortex path removed after repeated leaks. `sumi_add_wake` on
every stroke (physical, never MIDI); `slide_mode = 1` drives the pinch with
the fold axis from azimuth, CC74 then outbound-only. Polish batch (#41):
Marble two-finger pinch on iOS, piano accidentals narrowed to 0.6 keys with
white-key tops between them (natural→natural glissandi), the two-tone
lattice (paper halo under each ring, accidentals drawn on top), and the
§3.4 lift ring removed. Goldens: a 12-cell scripted glissando tracks pitch
< 1 cent across every crossing; device evidence: a live 10-note glissando
reconstructed from the byte log, `pen_trace.py` PASS on 10 iPad strokes,
screen captures of the strip lit by the squeeze.

### Step 20 — Lamb–Oseen swirl & bipolar press (core + iOS)
`SUMI_DEFORM_SWIRL` (θ = S/(2πr²)·(1−e^(−r²/r_c²)), series guard below
x < 1e-3), 0xA0 decoded as poly pressure → the *swirl* dimension keyed by
the voice's note, `press_mode` arbitration of 0xD0 (feed or swirl, one
consumer), r_c = the voice's boundary, band-parity sign, echo fan-out (#37).
hostmpe bipolar Y: one radial knee, up → 0xD0 feed, down → 0xA0 swirl on the
voice's channel, lift releases the engaged half before Note Off. 19/19
scripted checks: core coherence (ring sharpness inside 0.7 r_c retained
exactly through ~4 rad while the annulus moved 29 k texels), counter-rotation
of adjacent notes, small-r stability (half-float ULP freeze documented),
full step-19 battery and fixture bitwise.

### Step 19 — v0.4 deformation operator batch (core, desktop harness)
`sumi_version()` → 0.4.0. Vortex **Rankine profile** (rigid core, 1/r²
exterior; 20 rotations leave the interior unblurred: mean |Δink| 0.00000 vs
0.758 for the exponential control); **dipolar wake** `sumi_add_wake`
(lab-frame doublet, sign corrected from the draft, rigid tip body, ≤ a/4
sub-steps because a/2 is the fold threshold; a one-frame 8×a flick keeps a
positive Jacobian everywhere); **Hamiltonian pinch** `sumi_add_pinch`
(streamline-windowed saddle, det = 1) plus the crossed-tine variant the
user kept behind `pinch_variant` (#34), also reachable from CC74 deltas via
`slide_mode = 1`; **sine ripple** live (composite-time view displacement,
prints un-rippled, amp = 0 bit-identical to v0.3) and baked (delta-driven
passes). `bend_mode` (#35/#36): the PER-NOTE bend either glides the drop or
raises the ripple by its distance from center, permanently (phase drift per
vibrato cycle), stilling on re-center/release/mode flip — unit-tested as
glide-XOR-ripple. Pinch soak (#33): ink MASS is the observable — 500
reversible pairs hold ±0.5%, zero fabrication after the §3.4 ingress rule
was extended to every new pass (the edge clamp had fabricated +9.5%), per-pass
erosion 1.9× the glide tine's. Field-dump fixture bitwise, all stress
regressions clean.

### Step 18 — Performance control strip (iOS)
`hostmpe_strip_t`: spring wheel (master bend ±2, ~50 ms return, guaranteed
exact-center final message), latch wheels (Mod CC 1 + two assignables,
relative accumulation so a regrasp never jumps, protocol CCs refused),
momentary/toggle sustain, announce after every MCM re-sync; 128 master-channel
CC slots joined the limiters so wheels are policed while buttons ride the
never-dropped class (#30). 494 checks. First device test moved the strip
from a docked band — which displaced every drop from its touched cell — to a
compact **floating palette** top-left over the full-canvas lattice, and
fixed the long-press recognizer cancelling a held sustain (#31); sustain
stays momentary by default.

### Piano-grid layout (between steps 17 and 18)
`SUMI_LAYOUT_PIANO_GRID = 5` (#29): the chroma grid's frame with each octave
drawn as a two-row keyboard — accidentals above at white-key units
{1, 2, 4, 5, 6}, naturals below; position + inverse probe, true-step glides,
goldens over all 84 notes at two aspects. R_max revised after the first
device test to half of min(key width, octave-pair height) so the knobs
match the chroma grid's feel.

### Step 17 — Outbound transports & per-pipe rate limiting (iOS)
Dispatcher fan-out: loopback full-rate; outbound change-only + per-voice
per-dimension latest-wins decimation (≤ 100 Hz), or a token budget with
round-robin fairness for BLE (~300 msg/s); Note On/Off and center bends
exempt. Sinks: virtual CoreMIDI source (which also sends explicitly to the
`AppleIDAMDriver` destination — a virtual source is NOT bridged to a tethered
Mac, #27), rtpMIDI network session, BLE peripheral (`MIDISend` to the
Bluetooth destination; the local mirror duplicated delivery, #25), with
MCM/RPN 0 = 48 re-sent on every sink appearance (#28) and a "Re-sync DAW"
button. Wire-order captures on every transport: handshake ordered, 100% of
Note Ons bend-preceded, worst per-slot 70/s under the 100 Hz policy, BLE
315/s at the sender, no cumulative lag over 60 s. Found by instrumenting:
`MIDISourceCreate` needs `UIBackgroundModes: [audio]` (#24); an analyser
that re-sorted equal-timestamp records had faked an rtpMIDI reordering
(#22 — delivery order IS the data). Panic + per-sink silence primitives
(#26).

### Step 16 — hostmpe allocator & loopback Play mode (iOS, fingers)
LRU-by-release round-robin allocator with external-occupancy masking (30 s
timeout), saturation = silent drop + HUD blink; joystick → MIDI with the
semitone-exact bend (one grid column = exactly ±171 counts at ±48; the knee
is a deadband, not a travel limit — identity beyond the circle, #10;
absolute deadband floor for finger-sized jitter, #16); Y-up is pressure,
fingers emit no CC74 (#19); Jankó's semitone axis is horizontal in the core
(#18); true-step glides on the lattices (#20). Live on the iPad with a ROLI
Piano held over Bluetooth: 2,766 external messages, zero touch allocations
on held channels, touch-down → visible drop **0.39 ms median**, all 15
member channels rotated, 0 stuck voices.

### Step 15 — Layout probe ABI & Play-mode overlay (iOS)
`sumi_layout_probe` + `sumi_cell_info_t` (#6): an instance-free pure
function callable from any thread — the shells' hit-test — sharing the
semitone-step derivation with the glide axis (#7). Goldens round-trip every
cell center on CHROMA_GRID and all three Jankó echo rows (suite 8,934 →
12,480 checks); lattice/drop alignment ≤ 0.0004 canvas (½ px). `hostmpe/`
seeded with the §3.2 soft knee behind a pure-C header (#8). iOS Marble/Play
toggle and `PlayOverlayView` — a faint lattice built by sweeping the probe
(#9), per-touch joystick indicators, zero MIDI yet. Version 0.3.0.

## v0.2.0 — spec v2: hardening, layouts, piano rolls, all five platforms

### Step 14 — Android shell (Jetpack Compose)
Compose `AndroidView`/SurfaceView shell with a one-file JNI host
(`android/cpp/sumi_jni.cpp`): dedicated render thread owning a host-created
EGL context, §5.4 blocking teardown (condvar; context outlives the surface),
render-thread command queue for UI→core calls, AMidi + BLE-MIDI
(`MidiManager.openBluetoothDevice`) ingestion, PowerManager thermal listener
driving sim_scale 0.75 ↔ 0.60. `SOKOL_GLES3` split in the shared GL swapchain
TU with ES3 extension checks. On a Galaxy Tab S8 Ultra (Adreno 730): 10-minute
Osmose stress at min 119.6 / avg 120.9 fps (vsync-limited), 1.37 M messages,
0 dropped, PSS flat; live thermal step-down and recovery with the drawing
preserved; ×10 surface destroy/recreate mid-stress with zero EGL errors,
zero crashes, flat memory; GLES3 field dump passes the §4.6 regression vs the
Metal fixture (max 1.51e-2 under the documented mobile tier); ROLI over
Bluetooth MIDI painting MPE end-to-end.

### App icons (all platforms)
`tools/gen_icons.py` generates every platform's icon from
`images/midi-sink.jpg`: Android adaptive/legacy/monochrome mipmaps (cream
keyed to alpha so the mask never crops ink), iOS 1024 asset catalog, Windows
.ico + .rc + runtime window icon, X11 runtime icon, Wayland .desktop + XDG
hicolor theme (with install component and icon-cache refresh), and a macOS
Dock tile set at runtime from an embedded PNG (bare executable — no bundle).

### Step 13 — iOS shell (SwiftUI)
~200-line Swift shell, no Objective-C wrapper: `import SumiCore` via
module.modulemap, `CAMetalLayer`-backed `UIViewRepresentable`, CADisplayLink
(paused when backgrounded — Metal in background is a crash), CoreMIDI with
notification-driven hotplug + CoreAudioKit Bluetooth pairing, touch gestures
(tap drop / drag tine / two-finger twist vortex), settings sheet (layout
picker, sim_scale toggle — default 1.0 on Metal GPU family apple7+),
activity-gated idle-timer hold. On an iPad Air 11" (M4) with the ROLI over
Bluetooth: 5.5-minute traced session at a flat 60.00 fps, zero seconds below
50, thermal Nominal for the entire Instruments trace; backgrounding/rotation
crash-free. Fixed on the way: `metal_ios` added to the shdc dialects (device
abort without it), and **resize now carries the drawing across on every
platform** (passthrough resample of the resolution-independent field;
pristine fields keep exact identity init so the §4.6 dump stays
bit-identical). Core purity held: identical exported-symbol tables macOS↔iOS,
zero platform conditionals in core.

### Step 12 — Linux backend (OpenGL 4.1 core)
`swapchain_gl.cpp` hosts SOKOL_IMPL for GL; host-owned GLFW context (§5.1
exception), ALSA MIDI via the same 1 Hz rescan. §4.6 resolved at the shader
DIALECT level: `@glsl_options flip_vert_y` on offscreen vertex shaders makes
every GL offscreen target top-left-origin exactly like Metal/D3D11 (MSL/HLSL
outputs byte-identical), leaving the final composite as the single divergence
point — zero runtime flip branches; a control build without the directive
fails the regression with the v channel mirrored. Non-blocking PBO+fence
print readback. Field regression vs the D3D11 dump: max|Δ| 1.34e-3, aux
bit-identical, no orientation correction. Dip PNG parity with Metal;
mpe/wind stress green on NVIDIA (Wayland and X11).

### Step 11 — Windows backend (D3D11)
`swapchain_d3d11.cpp` hosts SOKOL_IMPL; DXGI flip-model swapchain, feature
level 11.1 with 11.0 fallback, vsync Present. DECISIONS #7 resolved: objects
compile twice on Windows only (dllexport baked into the shared set; no .def
file), ABI test links both artifacts. The print-readback seam became
backend-neutral (`sg_image` + bytes-per-pixel through the swapchain API;
staging-texture + `Map(DO_NOT_WAIT)` polling). Introduced the §4.6
cross-backend field regression: a canonical 7-pass deform script, harness
`--field-dump`, and `tests/field_dump_compare` — two D3D11 runs bit-identical.
WinMM stress feeder (`mpe_stress_win.cpp`) with absolute-clock pacing;
Steps 3–7 checks green on Windows at 165 Hz.

### Step 10 — Piano-roll layouts & BPM scroll
`SUMI_LAYOUT_ROLL_H`/`ROLL_V`: drops spawn on a fixed now-line and the whole
field translates as a closed-form scroll pass (emitted first, once per frame,
outside the deform budget) at `(bpm/60) × roll_speed` canvas-lengths/s, with
ingress as an explicit fresh-water shader branch (sampler clamp modes cannot
express it). Spec conflict resolved with the author: default roll_speed
0.0625 (16 beats = 4 bars of 4/4 across the canvas). Harness keys B/Shift-B
nudge BPM. Evidence: metronome drops evenly spaced to 0.1%, held-pressure
comet trails, vortex smear over scroll with the washi grain bit-identical
across prints (screen-locked), pristine ingress after 6 canvas-lengths, and a
10-minute soak: RSS flat (+160 KB), 98.2 fps, 0 dropped.

### Step 9 — Layout system: chromatic grid & Jankó
Pluggable pure pitch→position layouts (`layouts.cpp`) with echo sets
(`SUMI_MAX_ECHOES` = 3): Jankó stamps all three parity rows as fully-fed
triplets — one voice owns the set, dynamics fan out, one drop counter tick,
all-or-none budget reservation. Chromatic grid C1–B7 (7×12, edge-row clamping
keeps pitch class). Layout-derived glide axis (shortest-neighbor rule) —
grids glide along their row, including the B→C wrap. Params v0.2
(`sumi_layout_t`, bpm, roll_speed), `sumi_version()` → 0.2.0. Golden-position
tests (128 notes × layouts × aspects); live layout switching under MPE stress
at 98.3 fps with no voice teleport.

### Step 8 — v1 hardening & visual polish
Overflow-armed stuck-voice timeout (~10 s, grace measured from the overflow
moment); double-buffered dip prints (third dip refused with WARN, newest
read first); drop-counter/aux rebase on dip (3000-drop session stays
bounded); §4.4 feed episodes with onset/release hysteresis stamping nested
rings; composite polish — per-region fiber angle drift and segment break-up
(no lattice), near-black pooled ink, ink thickness via neighborhood band
probing (fract-based thickness fails on feed micro-shells). 500-drop
sharpness and Osmose stress unchanged (98+ fps, 0 dropped).

## v0.1.0 — spec v1: core engine + macOS harness (steps 1–7)

### Step 7 — Washi paper, palettes, paper dip
§4.5 composite: simplex mulberry-fiber strands, sizing mottle, absorption
grain scaled by paper_roughness; sumi/indigo/ochre palettes with morph
blending and aux-driven per-drop hue drift; linear math, manual sRGB encode.
Full paper dip: freeze → async GPU readback on a separate queue (never blocks
the loop; dip-window worst frame 14 ms at 2560×1440) → `sumi_read_print` →
identity reset with a "lift the paper" fade. PNG export moved to a detached
thread after an inline encode stalled 1080 ms.

### Step 6 — Wind mode & Airwave CC routing
Wind mode: one wandering-brush voice — breath (CC2/CC7/CC11/chanAT) drives a
breath-proportional line width, legato = VoiceMigrate with a wake tine.
Runtime CC routing table (`sumi_map_cc`) with Airwave defaults; global
controls as smoothed dt-scaled state. Mode detection became
activity-windowed after live playing latched MPE forever (user-reported);
mode changes flush the old dialect's voices.

### Step 5 — Full MPE mode
MCM (RPN 6) zone config, ±48 member bend with RPN 0 override, per-member-
channel voice table with note steal, per-voice smoothing/coalescing, §4.4
press feeds converted to area-correct boundary growth (spec-literal radius
steps looked frozen), per-voice glide tines, lift surfactant rings, slide→aux,
64-pass/frame deform budget with overflow merging. Osmose stress (10 voices ×
200 press/s): 99.6 fps, 0 dropped.

### Step 4 — MIDI ingest, normalizer, classic mode
Lock-free SPSC ring (4096, drop-oldest + diagnostics counter), stateful
decoder (14-bit bend, RPN/NRPN, running status), §2.5 dialect auto-detection,
classic mode lowering (fifths layout, sqrt-velocity radii, bend shear, CC1
vortex, CC64 dip). libremidi quirks found: virtual endpoints need
`track_virtual`, hotplug callbacks never fire → 1 Hz port rescan. 10 k
messages/30 s with 0 dropped.

### Step 3 — Jaffer deformations via mouse
Drop/tine/vortex passes (§4.3 closed forms, aspect-corrected), continuous
ink-phase encoding, ring-banding composite, mouse gestures. Two foundational
bugs fixed: the y-down texture-space convention (raw NDC interpolant flipped
the field every pass — consecutive deformations cancelled) and the
RGBA16F-safe parity ink phase (seam speckle past 256 drops). 500 drops stay
pixel-sharp; 40 × z/40 tine == 1 × z tine.

### Step 2 — Ping-pong targets & identity pass
RGBA16F ping-pong field targets, identity/passthrough passes, deformation
queue, sim_scale end to end. The 1000-swaps/frame stress test exposed a
~4 GB/s autorelease leak in a runloop-less Metal host — fixed with the
core-owned per-frame autorelease pool (applies to iOS unchanged).

### Step 1 — Skeleton, build system, black window
CMake + FetchContent pins (sokol, glm, libremidi, GLFW, stb, pinned
sokol-shdc binary download), static+shared libsumi from one object set, pure
C11 ABI test linking the dylib, GLFW harness with CAMetalLayer glue, clean
create/destroy at ~100 fps with Metal validation on.
