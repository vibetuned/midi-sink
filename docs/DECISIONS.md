# DECISIONS

Implementation decisions and resolved ambiguities, in four parts:
**Part I** covers the v1 build-out (spec v1, roadmap steps 1–7);
**Part II** covers spec v2 (steps 8–14); **Part III** covers Phase 4 — the
Touch & Stylus MPE Play Surface (steps 15–22, v0.3 → v0.4); **Part IV**
covers Phase 5 — packaging, release, web and documentation, and the Step-33
feedback batches (steps 23–33, core 0.5 → 0.9). Entry numbering restarts in
each part — references of the form `DECISIONS.md #n` (code comments,
evidence, git history) mean Part I, `DECISIONS_2 #n` / `DECISIONS_2.md #n`
mean Part II, `DECISIONS_3 #n` / `DECISIONS_3.md #n` mean Part III, and
`DECISIONS_4 #n` / `DECISIONS_4.md #n` mean Part IV (the files they referred
to were merged into this one).

---

# Part I — v1 (steps 1–7)


Spec ambiguities resolved during implementation, per the working rules.
Guiding principle: keep the core identical for iOS/Android.

## Step 1

1. **`SOKOL_IMPL` lives in `swapchain_metal.mm`, not `renderer.cpp`.**
   sokol_gfx's Metal backend must be compiled as Objective-C++ with ARC, which
   a plain `.cpp` cannot provide. `renderer.cpp` includes sokol declarations
   only. The working rule ("every sokol call behind renderer.cpp /
   swapchain_*") still holds, and the pattern ports cleanly: each platform's
   swapchain TU hosts the sokol implementation for its backend (identical on
   iOS via `metal_ios`; `swapchain_gl.cpp` will host it for GLES3 on Android).

2. **sokol-shdc dialect `glsl410` instead of the spec's `glsl330`.**
   Current sokol-shdc (pinned sokol-tools-bin commit) dropped the `glsl330`
   output. `glsl410` matches the spec's own phase-2 Linux target ("OpenGL 4.1
   core", spec header + §5.1). GLES3 output is `glsl300es`.

3. **Log level convention for `sumi_log_fn`'s `int level`** (unspecified in
   §5): sokol's numbering — 0 panic, 1 error, 2 warning, 3 info
   (`core/src/log_levels.h`). Lets sokol's logger bridge through unchanged on
   every platform.

4. **Version encoding**: `sumi_version()` returns `(0<<16)|(1<<8)|0` = 256 for
   0.1.0, per the header comment `(maj<<16)|(min<<8)|patch`.

5. **Swapchain pixel format BGRA8Unorm (non-sRGB) for phase 1.** §4.5's
   "write sRGB-encoded swapchain" is a composite-pass concern (a later step);
   deciding it now would risk baking in a format some mobile swapchains
   handle differently. The clear color is authored directly in swapchain
   space. Revisit in the composite step.

6. **Deep indigo clear** = (0.055, 0.050, 0.220, 1.0).

7. **Static + shared built from one CMake OBJECT library.**
   `SUMI_BUILD_SHARED` only changes behavior on Windows (`__declspec`); on
   macOS/iOS/Android the `visibility("default")` attribute serves both
   artifacts, so building objects once is safe. Must be revisited when the
   Windows/D3D11 build lands in phase 2 (objects will need to split, or the
   define moves to an export-map approach).

8. **Harness test flags** `--exit-after <seconds>` and `--resize-test`
   (programmatic 2-stage window resize) exist only in `desktop/src/main.cpp`
   to automate the step DONE checks. Never part of the core.

9. **`leaks --atExit` used as the "short Instruments pass"** for the DONE
   leak check (same malloc-introspection machinery, scriptable in CI).

10. **Placeholder shader** `core/src/shaders/placeholder.glsl` is compiled at
    build time purely to prove the shdc download + cross-compilation wiring
    required by step 1; nothing includes the generated header yet.

## Step 2

12. **The core owns a per-frame autorelease pool on Apple platforms.**
    Every Metal pass encoder/drawable is an autoreleased ObjC object; a plain
    C render loop (this harness, and any non-runloop host) has no draining
    pool, so at 1,000 passes/frame RSS grew ~4 GB/s in the stress test. Fixed
    with `objc_autoreleasePoolPush/Pop` wrapped around each frame (and each
    target recreation) via `sumi_swapchain_frame_pool_push/pop`; the same code
    path serves iOS. Non-ObjC backends will implement these as no-ops. Fix
    verified: flat 147 MB RSS over the same stress run.

13. **`sumi_set_params` clamps `sim_scale` to (0, 2]** (header comment
    "(0,1]..2"), and computed target dimensions are guarded to [8, 8192] so no
    window/scale combination can exceed GPU texture limits. §4.1's "clampable
    to e.g. 2048²" is read as *the user can clamp via sim_scale*, not a hard
    engine cap (a hard cap would silently change visuals between a MacBook and
    a phone).

14. **Stress hook = `SUMI_STRESS_SWAPS` env var**, parsed once in
    `sumi_create`, pushing N passthrough deforms per `sumi_update`. Chosen
    over an ABI addition (the contract must not carry test-only entry points)
    and over harness-side pushes (no public path to enqueue passthrough passes
    exists, by design). Deform queue capacity is 4096/frame — the spec's
    per-frame deform budget (§3.4, later step) is 64, so the headroom exists
    purely for this stress mode.

15. **Resize re-initializes the field to identity.** Until real state-carrying
    deformations exist (later steps), recreating targets at the new simulation
    resolution and re-running identity init is the only correct behavior;
    "without corrupting state" = no stale/garbage texels, which re-init
    guarantees. Revisit (content-preserving rescale?) when the field carries
    performance state worth keeping across resizes.

## Step 3

16. **Naming: the app is `midi-sink`; only the core library is `sumi`.**
    Desktop target/binary and window title renamed accordingly (user
    direction, 2026-08-31). ABI (`sumi_*`), core library output name, and
    header stay `sumi`.

17. **One orientation convention everywhere: texture space, v grows down.**
    The fullscreen-triangle vertex shaders emit `st = (u, 1 − v_clip)` — the
    texture-space coordinate of the fragment's own texel — and every pass
    (identity, deforms, composite) works purely in that space. Found the hard
    way: sampling at the raw interpolant made every offscreen pass vertically
    flip the field (Metal NDC y-up vs texture row 0 = top), so consecutive
    deformations cancelled instead of composing; identity+composite and
    even-count stress runs masked it. With `st`, passthrough is a true no-op,
    deformations compose exactly (verified: 40 × z/40 tine == 1 × z tine),
    and mouse coords/texture rows/screen rows all share one y-down space.
    This convention must be revisited per backend in phase 2 (GL's row order
    differs) — the flip, if any, belongs in the swapchain/composite boundary,
    never in the deform chain.

18. **Ink phase = parity-derived, in [1,3): `1 + (counter % 2) + radial`**
    (water = 0). A raw `counter + radial` phase breaks in RGBA16F: ULP(512) =
    0.5 destroys band parity past ~256 drops, and a wrapped counter speckles
    seams where far-apart counters touch (interpolation sweeps many integers).
    With the parity form, any two field values interpolate across at most one
    band threshold — no speckle at any drop count (verified at 500). Still
    §4.2-conformant: the phase is *derived from* the monotonic counter, stays
    continuous, and the composite bands it with a periodic function. The raw
    counter is stored in `aux` as the per-drop selector (note: aux itself
    degrades above ~2048 in half float — revisit when palettes land).

19. **Drop radius/lengths are in canvas-height units** (aspect-corrected
    space normalizes y to [0,1]); `sumi_add_drop`'s radius, tine alpha /
    magnitude, and vortex radius all share that unit. Deformation math uses
    the actual field texture's aspect (sim_width/sim_height), not the window's.

20. **Vortex demo is deliberately off-center from the rings** — θ(d) rotation
    concentric with circular rings maps circles to circles (rotation-invariant),
    so a centered vortex is invisible on rings; spirals require an offset
    center or angular content. The mouse gesture (vortex at cursor) is
    naturally off-center.

## Step 4

21. **Normalizer emits *musical* events; the §3.3 vocabulary is produced by
    voice_mapper** (two stages: normalize -> §3.3 events -> lower to
    deformations). §3.3's VoiceBegin carries mapped positions, which need
    params (pitch_layout) and aspect — that is §3.4 territory, i.e.
    voice_mapper per §6. Both stages are GPU-free and unit-tested headlessly.

22. **`GlobalBend` added to the internal §3.3 vocabulary.** Classic mode maps
    pitch bend to a global shear tine (§2.4), but §3.3 has no bend-shaped
    event and `sumi_ctl_t` no bend dimension. The event stays device-agnostic
    (semitones); only the classic lowering turns it into a tine.

23. **SPSC ring: drop-oldest via a single producer-side CAS on `head`.** Slots
    are single atomic 32-bit words (a packed message can never tear); the
    consumer CASes `head` too so producer-steal (overflow) and consume can
    race safely. Producer stays wait-free: one CAS attempt, no loop — if it
    fails the consumer just freed a slot. Capacity 4096 (power of two, §3.1).

24. **Host serializes MIDI producers with a mutex (harness-side).** §5.2
    demands exactly one producer thread; CoreMIDI may deliver different
    devices on different threads. The core stays lock-free; the lock is host
    plumbing only.

25. **libremidi's CoreMIDI hotplug notifications never fire in this app; the
    harness rescans instead.** Verified empirically: raw CoreMIDI
    (`MIDIClientCreateWithBlock`) delivers ObjectAdded to this process even
    with zero-duration run-loop slices, but libremidi's observer callbacks
    (v5.4.3) never fire — not even the constructor-time enumeration. The
    harness therefore polls `get_input_ports()` once per second from the main
    loop (open new, prune gone) — also inherently portable to the phase-2
    backends. Additionally `track_virtual = true` had to be set: libremidi
    filters virtual endpoints (DAWs, IAC, test sources) out by default.

26. **Step-4 DONE evidence uses a virtual CoreMIDI source** (Swift tool in the
    session scratchpad) sending a dense scripted performance (~10 200 msgs /
    30 s: 200 Hz bend + CC1 streams, walking notes), since an agent cannot
    play the physical ROLI. The user's real ROLI Piano was also opened and
    painted drops live during the session.

27. **Classic-mode tuning constants** (voice_mapper.cpp): drop radius
    0.02 + 0.075·sqrt(strike); shear tine alpha 0.45, 0.015/semitone; mod
    vortex ≤ 0.12 rad/update, radius 0.35, coalesced to one per update (§3.4).
    Mod vortex strength is per-update (frame-rate dependent) until the §3.4
    smoothing/budget step lands.

## Step 5

28. **Press feed steps are *boundary growth*, not raw expansion radii.** A
    center expansion of radius r only moves an existing drop boundary R to
    sqrt(R² + r²) (area conservation), so spec-literal small radius steps
    (§4.4) grow a drop quadratically slowly — visually almost frozen. The
    voice tracks its nominal boundary R; the accumulated per-frame step ΔR
    (∝ smoothed pressure × dt × expansion_rate, §3.4) is converted to the
    emitted pass radius via r = sqrt((R+ΔR)² − R²). Same closed-form
    framework, physically meaningful growth rate.

29. **Deformation budget applies to continuous streams only.** Discrete
    events (note strikes, lift rings, paper dips) always emit — §3.4's budget
    text targets tine segments/feeds, and silently losing a played note is
    musically wrong. Continuous emissions (glide tines, press feeds, global
    shear/vortex) are budget-capped; on exhaustion their accumulators simply
    stay unflushed, so the motion/growth merges into the next frame's single
    emission. Budget = 64/frame, test-overridable
    (sumi_voice_mapper_set_budget); merges are counted and logged (throttled).

30. **Glide's "pitch axis" in the circle-of-fifths layout** (§3.4): adjacent
    semitones sit 7/12 of a turn apart, so there is no continuous chromatic
    axis. The axis is the direction from pos(note) to pos(note+1) with the
    per-semitone distance capped at 0.03 canvas heights (a ±48-semitone glide
    stays on canvas). The grid layout's natural column step is below the cap
    and unaffected.

31. **Master-channel bend in MPE mode maps to the classic global shear tine**;
    §2.1 gives the master channel zone-global pitch, and the shear is our
    existing global-pitch visual. Member bend (±48 default, RPN 0 override)
    is per-voice glide, never global.

32. **Voice steal (§2.1)** emits VoiceEnd with lift = 0 (no surfactant ring —
    the finger did not lift) before the new VoiceBegin on the same channel.
    Note-offs for already-stolen notes are ignored.

33. **Slide (CC74) currently modulates only the aux channel** written by the
    voice's feed expansions (aux = note-on counter + 0.9·slide). Visually
    inert until the palette composite lands — recorded so nobody hunts for a
    missing effect.

## Step 6

34. **The wind brush maintains a breath-proportional WIDTH, not unbounded
    growth.** §2.3's "breath modulates continuous ink flow" implemented
    literally (like MPE press) turns a 20 s legato line into one canvas-sized
    blob — the nominal radius integrates forever across migrations. The wind
    voice instead relaxes toward width(breath) = 0.006 + 0.05·breath (growth
    only up to the target; a migrate clamps the new segment down to the
    current breath width). MPE press keeps its unbounded §4.4 integration —
    that is the Osmose behavior. Verified: blob before, calligraphic line
    after.

35. **CC routing**: any-channel (0xFF) and per-channel tables; per-channel
    wins. CC64 (paper dip) and CC74-on-MPE-members (slide) are reserved and
    checked before the table. `sumi_clear_cc_map` removes the defaults too —
    a host that clears owns the whole routing. Defaults documented in
    README.md (CC1/2/11/20–25); Airwave numbers are user-assigned device-side,
    so the defaults are a convention, not a protocol.

36. **Global controls are smoothed per-frame state, and the vortex is
    dt-scaled** (strength × 6 rad/s × dt, damped by (1 − 0.85·viscosity)).
    This replaces step 4's per-event vortex (frame-rate dependent, noted in
    #27) and gives viscosity a live, testable effect. Vortex center follows
    SUMI_CTL_VORTEX_X/Y. Paper roughness and palette morph are tracked and
    smoothed but consumed only when the washi/palette composite lands.

37. **§2.5 detection is activity-windowed, with a mode-change voice flush.**
    The original latched masks meant that once an MPE piano had been played,
    wind mode could never engage again in the same session (user-reported).
    Detection now weighs only channels played within the last 6 s; breath
    density uses a rolling two-bucket 2 s window (the fresh bucket inherits
    the previous one's verdict, killing boundary flapping); silence holds the
    last mode. An explicit MCM still flips to MPE immediately, but its claim
    also expires with member-channel inactivity. On any mode change the
    mapper synthesizes VoiceEnd (lift 0) for every voice tracked under the
    old dialect so nothing keeps feeding. The engine feeds the normalizer a
    dt-accumulated monotonic clock — no OS time source enters the core.

38. **Breath aliases include CC7** (volume), alongside CC2/CC11/chanAT —
    wind controllers classically transmit on 2/7/11 (user input). All three
    count toward wind-mode density detection and default-map to
    SUMI_CTL_INK_FLOW.

## Step 7

39. **sRGB is encoded manually in the composite shader** rather than via an
    sRGB swapchain format: all color math runs in linear space and the final
    write applies the exact sRGB curve (§4.5 "render in linear, write
    sRGB-encoded swapchain"). Manual encode behaves identically across Metal /
    GL / GLES3 backends and applies equally to the swapchain and the RGBA8
    print target, so the exported PNG matches the screen bit-for-bit in tone.

40. **Paper-dip snapshot rides the RESET deform**: when the renderer meets a
    RESET pass it first composites the current field into the RGBA8 print
    target and schedules the GPU->CPU blit, then runs the identity reset —
    so the print captures exactly what was on screen at dip time, after
    everything queued before the dip and before anything after it.

41. **Async readback ordering without cross-queue sync**: the blit command
    buffer is committed on SOKOL'S OWN MTLCommandQueue (sg_mtl_command_queue),
    after an sg_commit that flushes the snapshot pass — command buffers on one
    queue execute in commit order, so no events/fences are needed. Completion
    sets an atomic flag; the render thread polls it (one 14 MB memcpy when
    done, ~2 ms). CPU print buffer and MTLBuffer are preallocated/reused so a
    dip never allocates mid-frame.

42. **PNG encoding runs on a detached harness thread** — stbi_write_png of a
    2560x1440 print takes ~1 s and stalled the render loop when done inline
    (found by the dip-hitch measurement; fixed and re-measured: dip-window
    worst frame 14-20 ms vs 31-37 ms ambient scheduler jitter in the same
    runs).

43. **Washi fibers**: two directional ridged simplex layers (15 deg / -35 deg,
    strongly anisotropic frequency) + sizing mottle + fine absorption grain,
    all scaled by paper_roughness; ink "soaks" toward paper along grain and
    strands. Palette morph blends each palette toward the next
    (sumi -> indigo -> ochre -> sumi) driven by SUMI_CTL_PALETTE_MORPH; the
    aux channel picks per-drop hue drift via golden-ratio spread
    (fract(aux*0.618), continuous — slide shifts it live).

44. **Pinned dependencies** (all FetchContent):
    - glm `1.0.1`
    - stb `2c980bb59875b0d32144a71867fbdebb2f77cd20` (stb_image_write, harness only)
    - sokol `1847290135f95e57e6d220b0a41208306aafc0dd` (master 2026-08-30)
    - libremidi `v5.4.3`
    - GLFW `3.4`
    - sokol-tools-bin `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (sokol-shdc)

---

# Part II — spec v2 (steps 8–14, formerly DECISIONS_2.md)


Ambiguities resolved during spec-v2 implementation (v1 history: Part I above;
where the two conflict, PROJECT_SPEC.md wins — it absorbed the validated v1
decisions). Guiding principle: keep the core identical across all five
platforms.

## Step 8

1. **Timeout arming refreshes every voice's activity clock** (§3.1). The
   suspicious window starts AT the overflow — that is when a Note Off may
   have vanished — so held voices get a full ~10 s grace period from that
   moment instead of being expired retroactively for pre-overflow silence.

2. **A refused dip refuses everything** (§5.3): no snapshot, no field reset,
   no counter rebase — the performance continues on the same sheet, with one
   warning log. Both the CC64/event path (voice mapper, `dip_allowed` from
   the engine) and `sumi_trigger_paper_dip` behave identically.

3. **`sumi_read_print` returns the NEWEST ready print and consumes it** (the
   buffer frees for the next dip); a size query (pixels = NULL) does not
   consume. Reading twice after a dip burst therefore yields newest, then
   oldest — the harness burst test saves both.

4. **Ink thickness cannot come from `fract(phase)`**: §4.4 feed-grown regions
   are onion-layered micro-shells (one per emission), so the fractional
   radial oscillates across the region and everything read as "edge". The
   composite instead probes the band at four ±5-texel offsets — same-band
   fraction = thickness. Costs 4 extra field taps in the composite only.

5. **Feed-episode hysteresis**: onset at smoothed press > 0.02, release at
   < 0.008; the first episode continues the strike's band (the §3.4 "drop
   keeps growing" behavior), each later onset takes a fresh counter/band
   seeded at R = 0.004 — that is what stamps §4.4's nested rings. Wind-brush
   episodes reset to the breath-width seed the same way.

## Step 9

6. **Chroma-grid out-of-range notes clamp to the nearest edge ROW, keeping
   their pitch-class column** (spec §3.4 says "nearest edge cell"): a G#0
   lands in the top row's G# column, not in C1's cell — preserving pitch-class
   identity reads better than snapping to the literal nearest cell.

7. **Glide axis = shorter of the two neighbor steps** (pos(note±1)), pointing
   toward increasing pitch. This derives the axis from the ACTIVE layout with
   no per-layout code: grids stay on their row at octave wraps (B->C jumps a
   row, B->A# does not), Jankó picks the half-column stagger, fifths keeps
   its capped chord direction (v1 DECISIONS #30 superseded).

8. **Jankó/chroma density vs drop size**: the spec's Jankó geometry packs 42
   whole-tone columns (~0.02 canvas each) — a full chromatic sweep at normal
   drop radii merges the columns within each echo row (three parallel bands);
   discrete lattice reads require sparse intervals. Evidence includes both
   captures. Layouts 3/4 (rolls) are accepted by the ABI but fall back to
   fifths until step 10.

8b. **Echo sets (spec §3.4 rev): fully-fed triplets.** Jankó stamps all three
   parity rows; the voice owns the echo set for its lifetime, press/glide/
   slide/lift fan out to every echo, the drop counter ticks once per
   VoiceBegin (shared band + aux), and the nominal feed boundary is shared so
   echoes grow in lockstep. Budget accounting is per pass but reservation is
   per SET (all-or-none per frame): merging happens within an echo across
   frames — one echo of a set is never culled while another feeds. Echo order
   is top-to-bottom (rows {0,2,4} / {1,3,5}).

9. **Colinear glide tines partially cancel**: two voices bent oppositely on
   the SAME grid row drag along the same infinite line (Jaffer tines have
   lateral locality via alpha, no longitudinal falloff) and mostly cancel.
   Physically consistent, discovered via a self-cancelling demo; glide
   evidence uses voices on different rows.

## Step 10

10. **Roll timing: the spec formula wins over the DONE phrasing.** §3.4 defines
    `s = (bpm/60) × roll_speed` with roll_speed = canvas-lengths-per-BEAT.
    The step's DONE line said "4 beats to traverse ¼ of the canvas" (1/16 per
    beat) while the original default 0.25 gave a quarter canvas per beat —
    inconsistent. Flagged; RESOLVED by the spec author: the DONE phrasing was
    the intent, the default was wrong. Spec updated — **default roll_speed =
    0.0625** (16 beats = 4 bars of 4/4 span the canvas; 0.25 read as a
    waterfall, not a drifting tray). Formula unchanged; scripted-clock test now
    asserts 1/16 canvas per beat, ¼ after 4 beats, 1.0 after 16.

11. **Scroll ingress sampling detail**: the explicit fresh-water branch fires
    for sources outside [0,1]; sources within half a texel of the border
    linear-filter against the clamped edge texel, which the PREVIOUS frame's
    ingress already wrote as fresh water — so no old ink can bleed in after
    the first scrolled frame.

12. **The scroll pass bypasses the deformation budget by construction**: the
    engine pushes it directly into the queue (first, once per frame) before
    the voice mapper runs; the mapper's budget counters never see it.

13. **Roll pitch ranges span the full MIDI 0–127** (with a 0.06 inset), unlike
    the C1–B7 grids: a roll is a timeline, not a keyboard picture, and
    clamping would stack out-of-range notes onto edge lanes.

## Step 11

14. **DECISIONS #7 resolved: objects compile TWICE on Windows, no .def file.**
    `sumi_objs` (bare `SUMI_API`) feeds `sumi_static`; `sumi_objs_shared`
    (compiled with `SUMI_BUILD_SHARED` → dllexport) feeds `sumi.dll`. Every
    other platform keeps the single object set. Rationale: the header's
    three-state `SUMI_API` macro stays the single source of the export
    surface — a .def file would be a second, parallel symbol list to keep in
    sync, and would also have exported the internal test hooks (#18) unless
    hand-curated. DLL consumers get `SUMI_USE_SHARED` (dllimport) via an
    INTERFACE define on `sumi_shared`; the ABI test builds twice on MSVC
    (`abi_c_compile` against the DLL, `abi_c_compile_static` against the
    archive). The static archive is `sumi_static.lib` on Windows only — the
    DLL's import library already claims `sumi.lib`.

15. **The print-readback seam is backend-neutral now**:
    `sumi_swapchain_readback_begin(sc, sg_image, w, h, bytes_per_pixel)` —
    renderer.cpp no longer calls `sg_mtl_*`; each swapchain TU queries its own
    backend object (Metal: `sg_mtl_query_image_info` + blit on the renderer's
    queue; D3D11: `sg_d3d11_query_image_info` + `CopyResource` into a staging
    texture, poll = `Map(DO_NOT_WAIT)` → `WAS_STILL_DRAWING` maps to
    "in flight"). Poll contract unchanged (0/1/2, never blocks).
    `bytes_per_pixel` (4 = RGBA8 print, 8 = RGBA16F field) exists for the
    §4.6 field dump. The renderer's skip-frame check is likewise neutral: a
    zero-width `sg_swapchain` from acquire means "no surface this frame".
    The Metal side was refactored blind and must be revalidated on macOS.

16. **Backend/handle validation moved from engine.cpp into the swapchain
    TUs.** The engine had a hardcoded "Metal only" gate; now each build's
    swapchain validates `config->backend` itself and the engine only checks
    that non-GL backends carry a surface handle — the engine translation unit
    is now byte-identical across all five platforms.

17. **D3D11 swapchain choices** (§5.1): `DXGI_SWAP_EFFECT_FLIP_DISCARD`,
    2 buffers, BGRA8 non-sRGB (mirrors DECISIONS #5), feature level 11.1 with
    a two-step 11.0 fallback (old runtimes reject arrays containing 11_1),
    `Present(1, 0)` — vsync pacing, matching the CAMetalLayer's display-linked
    default. `pixel_ratio` is ignored on Win32: GLFW already reports physical
    pixels. Per §4.6 the step added **zero** flip code; verified by the
    chevron/vortex dip prints and the field dump (row 0 = top).

18. **§4.6 field regression plumbing**: the canonical deform script and the
    field readback are internal, static-link-only test hooks
    (`core/src/sumi_debug.h`, not `SUMI_API`, absent from the DLL) — the
    script's 7 passes are written as float literals in one shared function so
    every backend runs bit-identical uniforms; the harness (`--field-dump`)
    forces a 512×512 field via `sumi_resize(512, 512, 1.0)`, drives one
    scripted-clock frame (dt = 1/120), decodes half→float on the CPU and
    writes `w,h (uint32 LE) + float32 RGBA rows, row 0 = top`.
    `tests/field_dump_compare.c` checks per-channel max|Δ| and overall
    mean|Δ|. Tolerances stay at the handoff's suggestion (max ≤ 1e-2,
    mean ≤ 1e-4) until the Metal fixture exists; two D3D11 runs compare
    bit-identical (max Δ = 0), so the tolerance budget is entirely for
    cross-GPU rasterization differences.

19. **Windows MPE stress feeder** (`tests/mpe_stress_win.cpp`): CoreMIDI
    scripts can't run on Windows and WinMM has no virtual loopback, so the
    feeder opens a loopMIDI port's WinMM output side with the exact
    osmose_stress.swift schedule. `timeBeginPeriod(1)` + absolute
    `sleep_until` pacing removes the Swift feeder's ~3% `Thread.sleep`
    overshoot. Also: the harness's non-Apple rescan clock (DECISIONS #25) is
    `std::chrono::steady_clock` — the previous `now = 0.0` placeholder would
    have disabled the 1 Hz rescan entirely off-macOS.

## Step 12

20. **§4.6 on GL resolves at the shader-DIALECT level, not at runtime:
    `@glsl_options flip_vert_y` on every OFFSCREEN vertex shader; the
    on-screen composite VS stays unflipped.** GL rasterizes FBOs with a
    bottom-left row origin, so with `st = (u, 1 − v_clip)` each offscreen
    pass would write rows where the NEXT pass's sample coordinate does not
    read them — every pass mirrors the previous one's field, the exact GL
    twin of the Metal bug in DECISIONS #17 (verified empirically: see the
    control below). sokol-shdc's `flip_vert_y` negates clip-space y in the
    GLSL outputs ONLY (the MSL/HLSL outputs of flipped and unflipped VS are
    byte-identical, so Metal/D3D11 behavior is untouched), which makes every
    GL offscreen target top-left-row-origin in memory exactly like
    Metal/D3D11. Consequences, all verified: the field dump needs no
    orientation correction (row 0 = top straight out of memory); the §4.6
    "print readback flip" site is UNUSED — the PBO copy is a straight
    memcpy (like D3D11's zero-flip result, #17); the single point where GL
    orientation diverges is the final swapchain composite, where
    `composite.glsl` now carries two programs — `composite` (unflipped VS:
    GL's bottom-up default-framebuffer scanout is itself the §4.6 flip) and
    `composite_print` (flipped, like every offscreen pass). Zero runtime
    branches, zero uniform-driven flips, deform.glsl math untouched.
    Evidence: field regression vs the committed D3D11 dump passes with
    max|Δ| 1.34e-3 / mean 1.35e-8 (aux bit-identical); a control build with
    the directive removed fails at max|Δ| 1.99 / mean 7.5e-2 with the v
    channel mirrored (docs/evidence/step12/field_noflip_control.log); two GL
    runs are bit-identical.

21. **GL swapchain choices (§5.1)**: `sumi_create` validates the host-owned-
    context contract — `backend == SUMI_BACKEND_GL`, `native_surface_handle`
    must be NULL (a non-NULL handle means the host expected device-creating
    ownership; refused loudly), and a current context must exist
    (`glGetString(GL_VERSION)` non-NULL; version/renderer logged). The
    environment default color format is RGBA8, not BGRA8 — GL's default
    framebuffer has no client-visible channel order — and renderer.cpp's
    swapchain composite pipeline now INHERITS the environment default
    instead of hardcoding BGRA8 (backend-neutral; Metal/D3D11 still report
    BGRA8, so nothing changes there). Vsync lives host-side
    (`glfwSwapInterval(1)` in the harness, right after MakeContextCurrent):
    the host owns the context on GL, and interval 1 restores the pacing
    parity Metal/D3D11 get from their swapchains (#17).

22. **PBO readback design (§5.3)**: a private `GL_READ_FRAMEBUFFER` +
    `glReadPixels` into a `GL_STREAM_READ` PBO — NOT `glGetTexImage`,
    because binding the print texture would silently desync sokol's
    texture-binding cache (the read-FBO and `GL_PIXEL_PACK_BUFFER` binding
    points are never touched by sokol; both are restored to 0 after
    scheduling). Completion is a `glFenceSync` polled with
    `glClientWaitSync(…, flags = 0, timeout = 0)` (TIMEOUT_EXPIRED → "in
    flight"); a `glFlush` right after fence creation guarantees the fence
    reaches the GPU — without it a 0-timeout, no-flush poll can report "in
    flight" forever. Rows copy straight (see #20). The double-buffer /
    third-dip-refusal contract is upstream and unchanged (verified:
    run_burst).

23. **ALSA evidence feeders** (`tests/mpe_stress_alsa.cpp`,
    `tests/wind_breath_alsa.cpp`): unlike Windows, ALSA has virtual ports
    built in — each feeder creates an `snd_seq` SOURCE port and emits raw
    bytes through `snd_midi_event_encode` + `snd_seq_event_output_direct`,
    so the wire schedule stays byte-identical to the CoreMIDI/WinMM
    originals (69,023 messages / 30 s, the exact mpe_stress_win count).
    libremidi's ALSA backend with `track_virtual = true` plus the 1 Hz
    rescan (DECISIONS #25) opens them with no code changes. Absolute-clock
    pacing as in #19.

## Step 13

24. **`metal_ios` added to the sokol-shdc dialect list.** sokol-shdc treats
    macOS and iOS Metal as separate slangs (`metal_macos` / `metal_ios`);
    with only `metal_macos` baked in, the first `sg_make_shader` on a real
    iPad asserts (shader desc has no source for the backend). One dialect
    string in cmake/CompileShaders.cmake — the GLSL sources, the generated
    headers' structure, and every other backend are untouched.

25. **iOS host plumbing (§5.4), all shell-side:** (a) raw-CoreMIDI setup
    notifications DO fire on iOS (unlike libremidi's observer on macOS,
    DECISIONS #25) — hotplug is notification-driven, no 1 Hz poll; (b) MIDI
    arrives through `MIDIInputPortCreateWithProtocol(._1_0)` — UMP MIDI1UP
    words carry exactly one complete status/d1/d2 message each (no running
    status to reassemble) and Bluetooth/wired/network sources all funnel
    through it; (c) Bluetooth MIDI pairing is Apple's stock
    `CABTMIDICentralViewController` in a sheet — once paired, the ROLI is
    just another CoreMIDI source; (d) the CAMetalLayer handle is passed
    UNRETAINED: the backing UIView owns the layer (layerClass) and outlives
    the instance, unlike the macOS glue where the host retains a layer it
    created itself.

26. **"iPad-class GPU" for the host sim_scale default = Metal GPU family
    `apple7`+** (A14/M1 and newer): those sustain sim_scale 1.0; older
    devices default 0.75. The core never sees the heuristic (params comment:
    the host owns this default); the settings toggle overrides it live.

27. **"Bit-identical core static library" is read as identical translation
    units, flags, and symbol surface** — a macOS and an iOS archive cannot
    be byte-equal (Mach-O platform/min-version load commands differ by
    definition). Verified: exported-symbol tables of build/core/libsumi.a
    and build-ios/core/libsumi.a are identical (283 symbols, nm diff empty),
    zero `TARGET_OS_*`/`__APPLE__` conditionals anywhere in core/ (the
    swapchain TU needs none either), and the iOS build compiles the same
    source list with the same SUMI_CORE_COMPILE_OPTIONS.

28. **Resize carries the drawing across (all platforms).** The spec never
    defined resize content semantics; the old behavior (recreate targets →
    identity init) erased the performance, which iPad rotation — and iOS's
    app-switcher snapshot layout passes, which resize the view in BOTH
    orientations on every backgrounding — turned from a corner case into a
    constant. Since the §4.2 payload (u, v, ink, aux) is normalized and
    resolution-independent, `create_field_targets` now resamples the old
    current texture into the new targets with one passthrough pass (stretch
    to the new aspect: the tray is the canvas) and destroys the old set
    after. A PRISTINE field (no deform since identity/dip reset — tracked by
    `field_dirty`) still takes the exact identity init, keeping the §4.6
    field dump byte-stable (verified: dump remains bit-identical to the
    committed Metal fixture); sim_scale changes get the same preservation
    for free. Shell-side, layoutSubviews defers `sumi_resize` while the
    scene is inactive and reapplies on activation.

## Step 14

29. **swapchain_gl.cpp hosts Android with `SOKOL_GLES3` behind `__ANDROID__`**
    (backend selection lives in swapchain TUs, #16). GLES3 deltas, all inside
    that TU: (a) `sumi_create` verifies an extension containing
    `_color_buffer_half_float` and fails loudly without it — RGBA16F color
    attachments are not core in ANY GLES version, and this is exactly the
    gate sokol_gfx uses to mark RGBA16F renderable on GLES (its
    `_color_buffer_float` promotion is WebGL2-only); (b) the §5.3/§4.6
    readback queries `GL_IMPLEMENTATION_COLOR_READ_FORMAT/TYPE` for the
    half-float target — if the driver does not report RGBA/HALF_FLOAT it
    reads RGBA/FLOAT (the pair ES 3.0 guarantees for float-type buffers)
    into a 2× PBO and narrows on the CPU, losslessly (the texture is fp16,
    so every widened value is an exactly representable half). Everything
    else — PBO + fence poll (#22), the flip story (#20: flip_vert_y is in
    the glsl300es output too, unflipped screen composite, straight-copy
    readbacks) — ports unchanged; the step added **zero** new flip code, as
    predicted. Desktop Linux verified unregressed: post-change field dump
    bit-identical to the step-12 fixture.

30. **Mobile-tier field-regression tolerance: max ≤ 2.5e-2, mean ≤ 1e-3**
    (comparator argv overrides; desktop-class comparisons keep the strict
    1e-2/1e-4 defaults). Measured on Adreno 730 (SM-X906B, ES 3.2):
    GLES3-vs-Metal max 1.51e-2 / mean 3.76e-4, GLES3-vs-desktop-GL max
    1.71e-2 / mean 3.81e-4; only 54 of 262,144 texels exceed 1e-2 (band
    edges), 99.9% ≤ 6.8e-3, and the dump is **bit-identical across runs** —
    a deterministic per-device rounding profile (Adreno filters fp16
    textures with fp16-precision lerps, ≈≤8 ULP accumulated over the 7-pass
    chain), NOT noise and NOT an orientation/compose break (those measure
    mean ≈ 7.5e-2 with v mirrored ~1.0 — 200× the mobile budget). Related:
    sokol's state reset enables GL_DITHER (the GL default);
    `swapchain_gl.cpp` now disables it at every frame-pool push on both GL
    backends. On Adreno 730 this changed nothing bit-for-bit (the driver
    does not dither fp16), but ES leaves dithering implementation-defined
    and a driver that did dither would break the regression's determinism —
    kept as insurance and verified a no-op on desktop NVIDIA too.

31. **Android host resolution policy: the EGL surface is capped at
    phone-class pixel count (≤ 2.8M px) by integer halving**
    (`SurfaceHolder.setFixedSize`; the display processor upscales for free).
    The SM-X906B's 2960×1848 panel (5.5M px) halves once to 1480×924;
    a 2400×1080 phone stays native. Rationale: the fp16 ping-pong is
    bandwidth-bound — at native panel size the Osmose stress ran ~25 fps on
    the Adreno 730; capped, it holds 120 fps (vsync). sim_scale semantics
    are unchanged (0.75 of the SURFACE); the host owns resolution policy
    (§ params comment), the core never sees the heuristic. Thermal
    step-down on top: PowerManager thermal listener drops sim_scale 0.75 →
    0.6 at THERMAL_STATUS_SEVERE, restores at ≤ MODERATE, both logged as
    CSV events; #28's resample preserves the drawing across every one of
    these changes.

32. **Android threading/teardown implementation (§5.2/§5.4)**: one
    detached-lifetime render thread owns EGL display/context (created once,
    surviving surface cycles so the field textures persist); Kotlin-side
    calls marshal through a command deque drained at the top of each
    render-thread frame (touches, params, dip, field dump); surfaceChanged
    is latest-wins atomics. `nativeSurfaceDestroyed` blocks the UI thread on
    a condvar until the render thread finishes the in-flight frame, calls
    `eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)` (surfaceless
    context — mandatory in Android's EGL since 7.0), destroys the EGL
    surface and releases the ANativeWindow — only then does the UI call
    return. Every EGL call is checked and failures logcat as "EGLERR" (the
    teardown-race evidence sweeps for that marker).

33. **Android MIDI: one AMidi poller thread for ALL ports** (1 ms cadence,
    non-blocking `AMidiOutputPort_receive`, per-port running-status parser) —
    with a single consumer-side thread the §5.2 "exactly one producer"
    contract holds however many devices are open (USB/virtual/BLE all land
    in the same path; BLE devices enter via `MidiManager.openBluetoothDevice`
    after an in-app scan for the BLE-MIDI service UUID). The DECISIONS #24
    producer mutex ports to the JNI layer and also serializes the
    stress-feeder handoff: while the feeder runs it IS the producer
    (`device_midi_enabled` gates the poller's pushes).

34. **Stress transport: an in-process feeder thread calling sumi_push_midi
    directly** (`nativeStartStress`, armed by an `--ei stressMinutes` Intent
    extra) rather than a virtual MIDI device — Android has no scriptable
    virtual MIDI source without a companion app, and the §5.2 contract only
    cares that exactly one producer pushes. The schedule is the byte-exact
    mpe_stress_win/alsa cycle (69,023 messages / 30 s, absolute-clock
    pacing), looped with note-offs between cycles for the 10-minute DONE
    run; per-second fps/worst-frame/thermal CSV (the iOS logger port) is
    written by the render thread to app files and pulled with `run-as`.
    The §4.6 dump ships the same way: `--es fieldDump 1` runs the canonical
    script (the shared float-literal function, #18) at 512×512 —
    **forcing sim_scale 1.0 for the dump** (the shell default 0.75 would
    shrink the field to 384×384) — and restores both afterwards.

35. **Android settings menu mirrors the iOS SettingsSheet, minus the
    sim_scale toggle.** The gear opens a Compose dialog with the same
    five-layout picker (identical names/order to `SumiApp.swift`) routing
    through `nativeSetLayout` → `sumi_set_params` on the render-thread
    command queue, plus the BLE-MIDI pairing entry. The iOS sheet also
    carries a "Full-resolution simulation" toggle; Android omits it because
    the thermal listener (#31) owns sim_scale live (0.75 ↔ 0.6) — a manual
    control would fight the automatic step-down. Layout state lives in the
    Compose shell (Android has no persisted params query need; the picker
    reflects the last selection), matching how iOS holds `@State layout`.

## App icon (all platforms)

36. **Icons are generated by `tools/gen_icons.py`, not Android Studio's Asset
    Studio.** One source (`images/midi-sink.jpg`, 500×500) has to feed five
    platforms, and a checked-in script keeps every derived asset regenerable
    (Asset Studio is Android-only and GUI-driven). Per platform: Android gets
    legacy mipmaps at five densities plus an adaptive icon; iOS gets a single
    1024² universal icon in `ios/Resources/Assets.xcassets` (wired via
    `ASSETCATALOG_COMPILER_APPICON_NAME` — iOS forbids an alpha channel, so
    it takes the opaque full-bleed art); the desktop harness compiles RGBA
    pixels into `desktop/src/app_icon.h` for `glfwSetWindowIcon` (no runtime
    asset path) plus a `.ico`/`.rc` pair so Explorer shows an icon for
    `midi-sink.exe`; Linux additionally gets XDG icon-theme PNGs under
    `packaging/linux/` (see #39). **Source resolution is the limiting
    factor** — 500² is upscaled 2× for iOS's required 1024²; regenerate from
    a larger original or a vector when one exists.

36b. **Three derived forms of the art, because the platforms differ in who
    does the masking.** iOS gets it FULL-BLEED and opaque (the OS applies its
    own squircle and rejects an alpha channel). Android gets the keyed,
    feathered foreground of #37 (the launcher masks it). The desktop trio —
    the Linux icon theme, the GLFW window icon, the Windows `.ico` — gets the
    square artwork with ANTI-ALIASED ROUNDED CORNERS (18% of the side, mask
    drawn 4x oversampled then downscaled, since ImageDraw's rounded rectangle
    is hard-edged), because no desktop shell masks icons: what the file
    contains is what the dock shows. The washi field stays opaque there, which
    is what keeps the mark legible on both dark and light shells.

    Two rejected iterations, recorded so they are not revisited: a round
    medallion (art RGB + radial alpha) was tried first and rejected on the
    author's call — the artwork is square and should read square; and a
    hard-cornered full-bleed square was too sharp beside the system icon set.

37. **The Android adaptive foreground keys the art's cream to ALPHA and
    feathers its outer fringe.** The art is full-bleed with ink running into
    its frame, which breaks both naive options: used full-bleed as a layer
    the launcher mask crops the motif, and inset raw it leaves the
    frame-clipped strokes as hard straight lines against the background
    (measured: a 67/255 step at the boundary). Because the art is cleanly
    bimodal (ink L≈72, cream L≈236) the cream keys out on a luminance ramp,
    and the background layer is painted that same sampled cream (`#F1ECE2`),
    so compositing reproduces the original artwork — verified mean |Δ| 3.5/255
    over the art region — with the seam gone (67.3 → 0.1). A smoothstep
    radial feather (0.80→1.00 of the inscribed radius) dissolves the frame,
    so a launcher mask only ever crops faint ink. The keyed layer doubles as
    the Android 13+ `<monochrome>` themed icon: its alpha already IS the ink
    silhouette. Art occupies 0.74 of the 108dp canvas (safe zone is 0.667;
    the feather covers the overshoot). Verified on device — One UI's squircle
    shows the full medallion with no hard cut
    (`docs/evidence/icons/android_launcher_masked_ondevice.png`).

38. **GLFW window icons are set only where they exist.** `glfwSetWindowIcon`
    is implemented on X11 and Win32; Wayland takes an app's icon from a
    `.desktop` file and macOS from the app bundle, and GLFW raises an error
    on both. The harness therefore gates the call on `glfwGetPlatform()`
    rather than logging a startup error on two of its five platforms. Wayland
    is covered instead by the desktop-entry route (#39); macOS still shows no
    custom icon for the unbundled dev harness, which needs a `.app` bundle.

39. **On Wayland the icon comes from the DESKTOP ENTRY, not the client, so
    app_id is load-bearing.** A Wayland compositor never accepts an icon over
    the protocol: it matches the toplevel's `xdg_toplevel.set_app_id` against
    desktop-entry basenames and loads that entry's `Icon=` from the XDG icon
    theme. Three things therefore have to agree, and the coupling is easy to
    break silently: the harness sets `GLFW_WAYLAND_APP_ID` (plus
    `GLFW_X11_CLASS_NAME`/`INSTANCE_NAME`, which feed WM_CLASS and are matched
    by `StartupWMClass`) to `"midi-sink"`; the entry must be named exactly
    `midi-sink.desktop`; and `Icon=midi-sink` must resolve in the theme.
    Installed with `cmake --install build --component desktop-integration
    --prefix ~/.local`. Verified end-to-end rather than by eye: `WAYLAND_DEBUG=1`
    shows `xdg_toplevel#55.set_app_id("midi-sink")` on the wire, and
    `Gio.DesktopAppInfo` + `Gtk.IconTheme` resolve the entry and its icon to
    real files at 32/48/256 px. (GNOME's `org.gnome.Shell.Introspect.GetWindows`,
    the obvious way to read a live window's app_id, is access-denied to
    unlisted callers — hence the wire-level check.) `Categories` carries one
    main category only; two would list the app twice in the menu.

39b. **GNOME Shell only matches desktop entries it indexed at login: a
    mid-session install shows a generic icon until you log out and back in.**
    Established by elimination on GNOME Shell 49 / Wayland, because every
    "obvious" cause was false and the assets were provably fine throughout:
    the icon PNGs resolve by name under GTK3 *and* GTK4 in every theme chain
    (hicolor/Adwaita/Yaru/Yaru-dark) and decode via GdkPixbuf; pointing
    `Icon=` at an ABSOLUTE PATH — which bypasses theme lookup entirely —
    changed nothing; refreshing the stale `icon-theme.cache` (#40) and
    forcing an icon-theme-changed signal changed nothing. The decisive probe
    was to relaunch the same GLFW window claiming
    `app_id = "org.gnome.Calculator"`: the shell immediately showed the
    Calculator icon. So app_id → desktop-entry matching works fine on this
    compositor and the protocol side is correct (#39); what fails is that
    `midi-sink.desktop`, installed mid-session, is absent from the running
    shell's app table — plausibly also poisoned by a cached negative for
    app_id `midi-sink` from the many harness runs that predate the entry
    existing. Nothing in the app or the entry fixes this; the shell has to be
    restarted, which on Wayland means a re-login. Worth knowing before
    hunting a nonexistent bug in the icon pipeline.

39c. **ROOT CAUSE of the missing dock icon: GIO silently drops a desktop entry
    whose `Exec` binary is not in PATH, and gnome-shell's PATH does not
    include `~/.local/bin`.** So with `Exec=midi-sink` the shell never loaded
    `midi-sink.desktop` at all — the window could not be matched, and GNOME
    drew the generic gear it uses for an unmatched Wayland toplevel. `Exec`
    and `TryExec` are therefore configured to the ABSOLUTE installed path at
    install time (`packaging/linux/midi-sink.desktop.in` + `install(CODE)`),
    which keeps `--prefix` authoritative. Fixed and confirmed on device.

    The debugging lesson is worth more than the fix: **every verification I ran
    passed while the feature was broken**, because my shell has `~/.local/bin`
    in PATH and gnome-shell does not — `Gio.AppInfo.get_all()` listed the entry
    for me and omitted it for the shell. Two false conclusions came out of that
    (an icon-theme problem, then a login-indexing problem, #39b) and both cost
    a user round-trip. What finally worked was reproducing the CONSUMER's
    environment rather than testing in my own: `env PATH=<gnome-shell's PATH>`
    around the same GIO query flips `entry present` from True to False, which
    is the whole bug in one line. Before that, the decisive isolation step was
    relaunching the identical window with `app_id="org.gnome.Calculator"` — it
    matched instantly, proving the compositor, protocol, and icon pipeline were
    all fine and the fault lay in our entry being invisible. When an
    integration works for a reference input and fails for yours, compare the
    two inputs under the consumer's environment, not yours.

40. **`install()` needs an explicit COMPONENT in a FetchContent project.**
    Every fetched dependency (GLFW, libremidi, glm, readerwriterqueue)
    contributes its own install rules to the default "Unspecified" component,
    so a plain `cmake --install --prefix ~/.local` copies ~190 third-party
    headers, static libs, CMake config and pkg-config files into the user's
    prefix alongside the nine files we actually want. Naming our rules
    `desktop-integration` and installing that component keeps the prefix
    clean. (Found by doing it: the stray files were identified from
    `build/install_manifest.txt` and removed. Note `file(INSTALL)` PRESERVES
    source mtimes, so installed copies carry their build-tree timestamps —
    which is what proved they came from this tree and not an older install.)

---

# Part III — Phase 4: Touch & Stylus MPE Play Surface (steps 15–22, formerly `_work/DECISIONS_3.md`)

Ambiguities resolved during Phase 4 — the tablets as MPE instruments: the
layout probe, the shared `hostmpe/` host library, fingers, transports, the
control strip, the v0.4 deformation operators, the pencil / S-Pen, and the
Android port. Prior history: Parts I and II above. Entries #1–#5 were
resolved at spec-review time, before any Step 15 code. Where an entry says
`PHASE4_SPEC.md §n` it means the Phase-4 spec as it stood during the phase —
now folded, corrected, into `PROJECT_SPEC.md` §8 (PHASE4 §n ≈ §8.n); where
these entries and the spec conflict, the later entry is the record of what
shipped. The engine is the same bytes on every platform, so every entry
below binds iOS and Android alike.

1. **Bend formula parenthesization (PHASE4 §3.3): round AFTER the 14-bit
   scale.** `pb = 8192 + round(bend_semitones / pb_range * 8192)`, clamped to
   [0, 16383]. The draft's `round(bend_semitones / pb_range) · 8192` rounds
   the semitone ratio first, quantizing every bend to whole ±8192 —
   all-or-nothing, ±48 semitones or silence. The corrected form is also what
   makes the test assertion true by construction: one semitone at ±48 =
   8192/48 = 170.67 → ±171 counts. The spec now carries an explicit note so
   nobody "simplifies" the parentheses back.

2. **The layout probe is instance-free — the signature IS the thread-safety
   story.** `sumi_layout_probe(uint32_t layout, const sumi_params_t*, float
   aspect, float x, float y, sumi_cell_info_t*)`: a pure function like the
   internal layouts API, not a method on `sumi_instance_t`. Rationale: on
   iOS the UI thread is the render thread, but on Android touches arrive on
   the UI thread while the render thread owns the instance (DECISIONS_2
   #32/#33) — an instance-bound, render-thread-only probe would force every
   touch-down through the command queue just to hit-test, spending the
   ≤2-frame latency budget on a round trip. The shells keep a params
   snapshot beside their UI state (stated obligation in spec and roadmap);
   the probe is callable from any thread and golden-testable headlessly with
   no instance at all.

3. **Outbound rate limiting is per-TRANSPORT, not one policy (PHASE4
   §5.3).** Change-only filtering everywhere, then: virtual CoreMIDI /
   Network Session / MidiDeviceService get ≤ 100 Hz per voice-dimension
   (latest-wins); BLE gets a **global ~300 msg/s budget** with round-robin
   per-voice fairness (testable form: every active voice updates within any
   100 ms window — one wiggling finger cannot starve nine). The arithmetic
   forced the split: a 10-touch storm at 100 Hz × 3 dimensions = 3,000 msg/s
   against a link that sustains a few hundred — a correct implementation of
   the single-policy draft would fail its own DONE test. Note On/Off, the
   initial center bend, and pressure-0-before-Note-Off are exempt on every
   transport.

4. **Entering Play mode pushes MCM + RPN 0 into the LOOPBACK, before any
   notes.** The draft configured only the outbound transports; the loopback
   normalizer would have relied on §2.5 heuristic detection flipping to MPE
   mid-performance, with the first notes decoding under the wrong bend
   range. One MCM (RPN 6, lower zone, 15 members) + RPN 0 = 48 on Play-mode
   entry makes the mode flip and the ±48 range deterministic on both pipes —
   and it is a conformance property of the whole phase (roadmap working
   rule), not a per-step detail.

5. **`hostmpe/`'s public header is pure C, enforced by the build.** Same
   contract as `sumi_core.h` (no STL, no C++ types across the header; the
   C++ implementation lives behind it): the Swift module-map pattern only
   imports C headers, and discovering that mid-Step-16 would mean rewriting
   the API surface under pressure. The existing `abi_c_compile` pattern gets
   a mandated sibling, `hostmpe_c_compile.c`, so the constraint is held by
   CI, not memory.

## Step 15

6. **Probe units: positions normalized, distances in canvas-height units,
   direction aspect-corrected.** PHASE4 §2 said "canvas distance" without
   fixing the metric, and normalized coordinates are anisotropic (a "unit"
   vector in them is skewed on screen). Resolution: `cell_center_*` stay
   normalized (they are positions); `cell_radius` and `semitone_step` are
   distances in canvas-height units — the codebase's universal distance unit
   (deform radii, gesture magnitudes) — and `semitone_dx/dy` is a unit
   vector in aspect-corrected space, derived from the SAME #7 normalized
   delta and converted (`(dx·aspect, dy)`, normalized). A shell measures
   touch deltas in the same metric by dividing pixel deltas by the view
   height; circles are circles. Documented field-by-field in sumi_core.h.

7. **The #7 derivation now has one implementation:**
   `sumi_layout_semitone_delta` (layouts.cpp, internal) returns the
   shortest-neighbor delta UNCAPPED in normalized coords; the voice mapper's
   `pitch_axis` applies its SEMITONE_STEP_MAX rendering cap on top (pure
   refactor — behavior pinned by the pre-existing glide-axis tests), and the
   probe reports the true lattice step. Consequence made explicit: a
   1-semitone bend on the play surface will traverse one full lattice step
   under the finger while the DROP's glide wake is capped at 0.030 canvas —
   the visual glide compression is a v2 aesthetic decision (§3.4), not a
   play-surface bug. Flagged for Step 16's feel pass: if capped wakes read
   as "the drop lags my finger", revisit the cap per-layout (question, not
   code — core stays frozen).

8. **`hostmpe/` is seeded in Step 15 with exactly the §3.2 knee** (soft-knee
   + joystick Δ_eff), nothing else. Step 15's DONE demands headless unit
   tests for the indicator math, and implementing it in Swift would create
   the platform drift the working rules exist to prevent — the same math
   must drive Android's overlay in Step 18. The allocator/bend/rate-limiter
   surface still lands in Step 16 (rule 7 respected: the knee is Step 15
   scope, pulled into its permanent home). `hostmpe_c_compile.c` and the
   unit suite are wired into ctest from day one; iOS links libhostmpe.a via
   a second module map (`import HostMPE`).

9. **The iOS lattice is built by SWEEPING the public probe** (220×120 sample
   points at layout/size change, deduped to cells), not by any Swift-side
   geometry: the overlay renders only what `sumi_layout_probe` returns, so
   the "one source of truth" guarantee is structural — there is no second
   lattice implementation to drift, and the sweep doubles as a smoke test of
   the probe over the whole canvas. Jankó's three-rows-one-note highlight
   falls out of the same map (cells sharing the touched note).

## Step 16

10. **The knee is a DEADBAND, not a travel limit, for the bend axis.** §3.2's
    clamp (d ≤ 1) and §3.3's one-column-one-semitone cannot both hold: on
    the grid a column (0.124 canvas) is ~2.2× R_max (0.057 — half the
    SMALLER cell dimension), so a bend saturating at the joystick circle
    tops out at ~0.46 semitones and the ±171 DONE test is unreachable.
    Resolution: `hostmpe_bend_deflection(d)` = soft knee inside the circle,
    IDENTITY beyond it (continuous at d = 1 where both branches equal 1) —
    far from the origin the bend tracks the finger absolutely, so a
    one-column drag is exactly Δx/step = 1.000 semitone → 171 counts, and
    in-tune glissandi span any number of columns. The CLAMPED knee remains
    the law for the bounded axes: CC74 and the visual thumb indicator.

11. **CC74 polarity lives inside hostmpe, which takes SCREEN deltas.**
    `hostmpe_touch_update(dx, dy, …)` receives raw screen-oriented deltas
    (y grows down) and applies the "up = brighter" ROLI polarity itself.
    Rationale: with two shells feeding it, a pre-negation convention WILL
    eventually be applied twice or zero times on one platform; a single
    documented ingestion orientation cannot.

12. **External-occupancy timeout refreshes on ANY channel traffic.** §5.1
    says occupancy clears "after a 30 s stuck-note timeout" without defining
    the clock. From Note On alone, a ROLI note held 31 s would be declared
    stuck WHILE SOUNDING and channel-stolen — precisely what masking exists
    to prevent. A genuinely held MPE note streams pressure continuously, so
    the timeout counts from the channel's last message of any kind: real
    holds never expire, a silent stuck channel frees in 30 s.

13. **Loopback emission is change-only at the byte level (not decimation).**
    `hostmpe_touch_update` suppresses messages whose byte value is unchanged
    per voice per dimension. §5.3's "loopback = full rate" means no
    RATE ceiling and no latest-wins dropping — an identical repeat carries
    zero information and only pollutes the byte log the DONE asserts read.
    Step 17's outbound change-only filter is a separate, per-transport
    stage on top.

14. **iOS producer topology: one serial DispatchQueue owns hostmpe AND
    `sumi_push_midi`.** CoreMIDI callbacks hop onto it (observe-external +
    push), touch handlers post through it (touch-down uses a sync hop —
    allocation must answer before the overlay can track the touch;
    microseconds), the session config is pushed from it, and the byte log
    appends only there. Serialization-with-barriers satisfies §5.2's
    single-producer contract exactly as the spec's iOS note prescribes; the
    UI thread never calls `sumi_push_midi` directly.

15. **Synthesized finger pressure is BASELINE-RELATIVE** (user-reported: "the
    minimal move makes the drop grow really fast"). An absolute majorRadius
    curve reads a resting fingertip as 0.3–0.8 pressure, and since pressure
    first ships on the first touchesMoved, any wiggle unleashed a strong §4.4
    feed. Now each touch records its contact majorRadius as the baseline and
    pressure = clamp((mr/mr₀ − 1.15)/0.6): resting = 0, the drop grows only
    when the pad visibly flattens (deliberate press). The §4 truth-table
    honesty stands — this is still crude, heavily smoothed glass, not force.

16. **The deadband gets an absolute floor: max(0.03·R_max, 0.006 canvas-
    height)** (`HOSTMPE_KNEE_FLOOR_CH`). §3.2's knee is proportional to
    R_max, but finger jitter is absolute — on Jankó (R_max ≈ 0.018) 3% is
    under a pixel and every micro-wobble bent pitch and slid CC74. The floor
    (~5 pt on the iPad) sits above jitter and below intent on every layout;
    the knee stays smooth and still reaches 1 exactly at the circle (capped
    at 0.9 so it can never swallow it), and the one-column-= -one-semitone
    exactness is untouched because identity-beyond-the-circle is
    knee-independent (unit-tested at Jankó geometry). Applied inside
    hostmpe's r_max-aware entry points; the bare reference forms keep the
    normalized 3% knee.

17. **Bend follows the lattice's 2D pitch GRADIENT, not the #7 axis**
    (user-directed: "the line in Jankó is orthogonal to the chromatic —
    odd"). The #7 shortest-neighbor axis is the stagger vector on Jankó
    (mostly vertical), so the natural horizontal drag bent nothing there.
    `hostmpe_touch_begin` now takes the local pitch gradient (gx, gy) in
    semitones per canvas-height unit; bend = deflection · (gx·dx + gy·dy).
    The shell SOLVES the gradient from the probe-swept neighbor cells (no
    lattice math in Swift): gx from the same-row neighbor, gy from the
    nearest other-row neighbor with gx's contribution removed. The solution
    is illuminating: on the grid, gx = 1 semitone/column and gy = 12/row
    (rows are octaves — vertical drags glide through octaves, lattice-true);
    on Jankó, **gy = 0** — pitch there is a function of x alone (each half
    column = +1 semitone; the rows are echoes of the same notes, which is
    exactly what echo sets assert). Consequence: bend reads horizontally on
    BOTH playable layouts, Jankó gets a continuous chromatic glissando at
    one semitone per half column, and vertical drags drive only CC74
    (timbre) as §3.3 intended. Flagged core question (not code — core
    frozen): the drop's visual glide wake still follows the core's #7 axis,
    which on Jankó is the stagger vector — a horizontal Jankó glissando
    paints slightly diagonal wakes; revisit the core's per-layout glide
    vector if it reads wrong in play.
    SUPERSEDED SAME-DAY by #18 — the 2D gradient shipped for one build and
    the user rejected the feel ("bend and timbre make the same line, timbre
    just larger" — the grid's octave rows made vertical drags read as a
    bigger copy of horizontal ones). The gradient FORM stays in the hostmpe
    API (gx, gy — it is the right abstraction and its unit tests stand), but
    the shipped mapping is gy = 0 everywhere.

18. **Jankó's semitone delta is HORIZONTAL in the core** — the flagged core
    question in #17, resolved by the user's direction. `sumi_layout_semitone_
    delta` special-cases Jankó: half a column straight along +x, because
    pitch there is a function of x alone (the parity rows are ECHOES of the
    same notes — the #7 shortest-neighbor rule mis-picked the stagger vector
    toward note±1's echo row, which is an echo-placement artifact, not pitch
    geometry). Consequences, all aligned: the probe's semitone axis is (1,0)
    on both playable layouts; the drop's glide wake stays IN its row and
    reads horizontally like the grid's; a Jankó glissando is one semitone
    per HALF column (the stagger interleaves them); vertical drags are
    timbre's alone (CC74 — visually subtle by §3.4 design: slide modulates
    the ink selector, not geometry; if it should read stronger on canvas,
    that is a composite question for later). AMENDS spec §3.4's echo-set
    sentence "Glide displaces every echo along … (in Jankó: half-column
    over, one row up)" → "half a column along the row"; the spec author
    should fold that into PHASE4/PROJECT_SPEC text.

19. **Fingers: Y → channel pressure, upward only; no CC74 (spec §3.3 rev,
    author's revision).** Supersedes #15's majorRadius pressure entirely —
    "not sure why I was trying to put pressure under the finger radius."
    Pressure = the upward component of the CLAMPED joystick (-ey): exactly 0
    at touch-down and for any downward Δy, monotonic through the soft knee
    (floored per #16), 127 at full-radius straight up. Pushing INTO the
    lattice upward is the growth gesture — deliberate, visible (drop rings
    grow via the §4.4 feed), and impossible to trigger by resting a finger.
    Fingers emit no CC74 ever (byte-log assertable); timbre belongs to the
    stylus matrix in Step 18. Since the bend gradient is horizontal on both
    playable layouts (#18), the axes are fully separated: X = pitch,
    Y-up = growth, and downward drags are musically silent.

20. **Playable lattices glide at the TRUE lattice step (glide cap lifted for
    grid/Jankó).** User-reported "bug": a far bend + release popped a small
    white drop ~two columns toward the bend, seemingly from nowhere. The
    white circle is the §3.4 lift surfactant ring (spec behavior since
    step 5, radius ∝ release velocity); it looked stray because the visual
    glide was capped at SEMITONE_STEP_MAX = 0.030 canvas per semitone —
    a quarter of a grid column — so the drop never visibly traveled with the
    finger, and the release ring materialized at the capped position. The
    cap protected FIFTHS (neighbor steps up to half the canvas); on the
    playable lattices the true step (grid 0.07/column, Jankó half-column)
    IS the correct visual, and the Step 16 DONE text demands it
    ("one-column drags read as clean semitone glides on canvas"). pitch_axis
    now uses the uncapped delta for CHROMA_GRID/JANKO and keeps the 0.030
    cap for fifths/rolls. Bonus: hardware MPE (ROLI) glides on the lattices
    now also land on their true cells. The lift ring itself stays — with the
    drop now visibly arriving where the finger goes, the ring reads as "the
    drop set here", which is its §3.4 meaning.

21. **The lift ring lands at the voice's BASE — the note's home cell — not
    the glide-displaced position** (user: "better if it appears under the
    note, now that the tablet is the instrument"). The release mark belongs
    to the NOTE: after a far bend the ink has wandered, but the ring stamps
    the pitch home it resolves to — one per echo, as before. Applies to all
    MPE sources (a ROLI release after a bend rings its home cell too);
    wind-mode migrate updates the base, so the brush rings where it last
    landed, unchanged in practice.

## Step 17

22. **RETRACTED AND CORRECTED — rtpMIDI does NOT reorder RPN; my analysis
    did.** The original entry claimed Apple's rtpMIDI recovery journal
    regrouped CCs by controller number, breaking the MCM/RPN0 handshake over
    the network session. That was wrong, and the error was mine: the capture
    analyser did `rows.sort()` on `(t, status, d1, d2)` tuples while the
    listener stamps every message in one delivery callback with the SAME
    timestamp — so ties were re-ordered by status/controller byte, which puts
    CC6 before CC100/CC101 and Note On (0x90) before Bend (0xE0). The
    "controller chapter" theory was fitted to an artifact of my own sort.
    Re-analysed in wire order, **every transport delivers correctly**:
    master MCM `101=0, 100=6, 6=15`, all 15 members at RPN 0 = 48, zero
    misordered data entries, and 100% of note-ons preceded by their center
    bend — on USB/IDAM (90/90), rtpMIDI (20/20) and BLE (80/80). The
    handshake DONE is therefore satisfied on ALL sinks, not just a
    "guaranteed path". Lessons kept: the monotonic per-message timestamp
    stays as ordering hygiene (harmless, and correct for any
    timestamp-batching transport), the 4 ms config spacing was reverted
    (it was treating a phantom), and capture analysis must never re-sort
    equal-timestamp records — delivery order IS the data.

23. **The outbound rate/fairness/change-only policies are transport-agnostic
    and were validated on the network capture** (which the journal does NOT
    reorder — notes and bends have their own journal chapters that preserve
    order): worst per-slot 1 s rate 70/s under the 100 Hz policy, 30 active
    slots, notes balanced, change-only holding (the only identical-value
    repeats were sub-20 ms rtp retransmits and legitimate return-to-value
    sweeps seconds apart), and the 1 Hz exempt marker drifted +46 ms over
    60 s (no cumulative lag). The limiter's headless suite (rate ceiling
    95–101/s under a 1 kHz storm; budget ≤650/2 s with every slot inside a
    115 ms fairness window; exempt notes never dropped) is the authority; the
    live capture corroborates on real transport timing.

24. **iOS refuses virtual MIDI endpoints without the `audio` background
    mode.** `MIDISourceCreate` returned **-10844 (kMIDINotPermitted)** and
    both outbound sources came back as endpoint 0, so every virtual-source
    send went to a null endpoint — invisible, because CoreMIDI status codes
    were not being checked. Adding `UIBackgroundModes: [audio]` to the
    Info.plist fixes it (the entitlement iOS requires for an app to publish
    MIDI other apps can see). Two lessons folded into the code: every
    CoreMIDI call in `MidiOutputs` now logs its OSStatus, and the shell shows
    a live per-sink sent counter (`out v…/n…/b…`) so "are we transmitting?"
    is answered by observation, not inference. Side effect, desirable for a
    controller app: the surface keeps streaming MIDI while backgrounded (the
    display link still pauses — Metal in background is a crash, #13 era).
    Also fixed here: `MIDIPacketList()` is a ONE-packet struct, but
    `MIDIPacketListAdd` was being told it had 1024 bytes — a latent stack
    overflow that a limiter drain (up to 64 messages) would have hit. The
    list is now backed by a properly sized buffer.

25. **BLE topology: send to the Bluetooth-driver DESTINATION, and the
    receive-side count is not the sender's rate.** Our "midi-sink (BLE)"
    virtual source only publishes locally; bytes reach a connected central
    by `MIDISend` to the destination iOS creates for the link (matched by
    `kMIDIPropertyDriverOwner` containing "bluetooth", deduped per ENTITY so
    a multi-endpoint driver cannot be sent to twice). Mirroring to our own
    virtual source was removed — iOS bridges device sources over the same
    link, so the mirror duplicated delivery. Measurement finding: the
    receiver logged 483 msg/s where the sender logged **315 msg/s**
    (18,900 in 60.0 s — the ~300 budget plus burst headroom, exactly as
    designed), with the surplus appearing as duplicate values whose delivery
    timestamps are **identical to the microsecond** (100% same receive
    callback) while only ONE destination existed on the sender. Since the
    limiter is token-metered and change-only (it cannot emit a repeated
    value at all), the inflation is CoreMIDI's BLE→UMP running-status
    expansion on the receive side. **Therefore the budget DONE is asserted
    at the sender**, where the policy lives; receiver captures corroborate
    fairness and lag, which duplication does not distort.

26. **A BLE peripheral cannot disconnect its central; "stop" is panic +
    per-sink silence instead.** User asked for a disconnect button. Apple
    exposes no public API for a BLE MIDI *peripheral* to tear down a link —
    the central (Mac/DAW) owns it, and `CABTMIDILocalPeripheralViewController`
    governs only advertising. What the request really needs is the guarantee
    that stopping never leaves sound stuck, so two shared primitives landed
    in hostmpe (Android inherits them in Step 18):
    `hostmpe_panic` releases every live voice through the normal
    pressure-0-then-Note-Off order and then silences the zone (CC 64 = 0 +
    CC 123 = 0 on master and all 15 members), and `hostmpe_silence_zone` is
    the stateless half — controllers only, voice table untouched. The
    explicit **"Stop all notes (panic)"** button uses the former on the
    loopback and every transport (exempt, never decimated); switching a
    transport OFF automatically uses the latter on just that sink, so a synth
    that stops receiving cannot hold notes while voices still sounding on the
    other pipes keep playing. The UI states plainly that dropping the BLE
    link itself is done from the connected device's Bluetooth settings.

27. **Reaching a wired Mac needs an explicit send to the IDAM destination —
    a virtual source alone is NOT bridged (spec correction to §5.4).** The
    revised §5.4 frames the virtual CoreMIDI source as "also the USB/IDAM
    primary sink". Measured: with the iPad tethered, the Mac lists the port
    (`name='iPad', driver='com.apple.AppleMIDIUSBDriver'`) and the iPad lists
    a destination `'Hôte MIDI IDAM' driver='com.apple.AppleIDAMDriver'`, but
    publishing only via `MIDIReceived(virtualSource)` delivered **nothing**
    to the Mac. The wired path is an explicit `MIDISend` to the IDAM
    destination. Implemented inside the SAME sink and the same ≤100 Hz
    policy — one toggle, two delivery mechanisms (virtual source for
    on-device apps, IDAM send for the tethered host) — which keeps the
    spec's one-sink framing while matching reality. Verified: 926 messages
    on the Mac's `iPad` port, handshake ordered, 90/90 note-ons preceded by
    center bend, worst per-slot rate 34/s (policy 100 Hz).
    Also: MIDI over USB needs no "Enable" — the Enable button in Audio MIDI
    Setup's Audio window is for IDAM AUDIO; MIDI Studio shows the device as
    connected on its own. The in-app hint says so. And no transport can
    expose our per-app port name to the host: BLE, USB/IDAM and rtpMIDI each
    present ONE merged port per link, named after the peer DEVICE (the Mac
    sees "iPad"; the iPad sees "LT-… Bluetooth"). Per-app names exist only
    locally on iOS.

28. **A sink appearing mid-session re-sends the handshake.** IDAM is
    typically enabled (and BLE centrals connect) AFTER the session opened,
    so the MCM/RPN0 sent at Play-mode entry would never reach it.
    `MidiOutputs` now watches CoreMIDI `msgSetupChanged` and fires
    `onSinkAppeared` when the world GREW (destinations/sources/devices
    count up — teardown needs no handshake), which the shell debounces to
    one re-send per 2 s and only while Play mode is effective.

## Between Step 17 and Step 18

29. **`SUMI_LAYOUT_PIANO_GRID` (= 5), a third playable lattice** (user-requested,
    amends PHASE4 §1's "grid and Jankó only" sentence and spec §3.4). The
    chroma grid's frame — C1..B7, insets 0.08/0.10, out-of-range clamps to the
    edge octave keeping pitch class — but each octave is a classical two-row
    keyboard: 5 accidentals on top at the classic boundary positions (white-key
    units {1, 2, 4, 5, 6}; the E–F and B–C gaps stay empty), 7 naturals below;
    14 rows, one echo. Resolved here:
    * **Accidental cells are one white-key unit wide**, centered on the
      boundaries, so the black row tiles [0.5, 6.5] with dead zones at the row
      ends and the two gaps — the probe refuses there (same "off the key bed"
      rule as the Jankó stagger ends). Naturals tile their row completely.
    * **R_max is the KEY footprint, not the drawn row: half of min(key width,
      octave-pair height).** First device test read the inscribed single-row
      radius as knobs half the chroma grid's size (14 rows vs 7). The
      black/white split is a drawing convention — a key's playable footprint
      is one key wide by one octave tall — and R_max is a travel bound, not a
      hit region (hit-testing happens only at touch-down). The octave-pair
      height equals the chroma grid's row height exactly (0.8/7), so knob
      size, deadband scale and CC74 travel match the chroma grid's feel; the
      lattice circles now nest diagonally between rows (no two adjacent-row
      cells share an x), which reads as a honeycomb, verified by headless SVG
      render before shipping.
    * **The semitone axis stays on the generic DECISIONS_2 #7 shortest-neighbor
      rule — no Jankó-style special case.** Jankó got a horizontal override
      (#18) because pitch there is a function of x alone and the parity rows
      are echoes; on the piano lattice pitch is NOT a function of x (two rows
      per octave), so the honest per-note axis is the half-key diagonal toward
      the adjacent accidental/natural (ties, e.g. D between C# and D#, resolve
      inside the existing rule toward the +1 neighbor). Consequence a pianist
      will recognize: glides bend toward the nearest key, alternating up/down
      diagonals, rather than along a fictitious straight pitch line.
    * **Joined the true-step lattice set** in the voice mapper (#20): glides
      render the uncapped lattice step, so a one-semitone bend lands the drop
      on the neighboring key cell.
    * Hosts: iOS picker + playable checks updated ("Piano grid"); the play
      overlay needed nothing — its lattice is a probe sweep (#9). Desktop `L`
      key now cycles 6 layouts. Evidence: `docs/evidence/piano-grid-layout/`.

## Step 18

30. **Strip engine resolutions (§8).** The widget value engines live in
    hostmpe (`hostmpe_strip_t`, pure-C API, headlessly tested), master-channel
    only by construction. Ambiguities resolved:
    * **The limiter gains 128 MASTER-channel CC slots.** Before Step 18,
      generic CCs bypassed the per-transport policies entirely (`lim_slot_index`
      returned -1 → pass-through), so a latch wheel would have flooded the BLE
      budget unpoliced. Master-channel CCs are now ordinary continuous
      dimensions (change-only + decimation/budget + round-robin fairness);
      member-channel CCs other than 74 still pass through — hostmpe generates
      none and external-device bytes never enter the limiters. The
      never-dropped class (§8: CC 64 / buttons) rides the existing `exempt`
      flag, asserted headlessly: a 10-voice 1 kHz bend storm through the
      300 msg/s budget passes every CC64 transition immediately, zero dropped.
    * **Latch regrasp is jump-proof by construction:** the API has no
      absolute-set entry point — only `hostmpe_strip_latch_move(delta)`. The
      shell feeds the CHANGE in knee-shaped grab position, so a fresh grab
      contributes delta 0. Sub-unit deltas accumulate in float (fine control
      by slow dragging); emission is change-only on the rounded 7-bit value.
    * **Assignable wheels refuse the protocol CCs** (1, 6, 38, 64, 98–101,
      120–127): a strip-assigned CC 6 on the master would corrupt the DAW's
      RPN handshake state mid-performance. Defaults: CC 23 / CC 24 — mapped
      to viscosity / roughness in the loopback's default CC map, so the
      wheels do something visible before Step 19 rebinds them to the ripple
      controls. Assignments are session-only (preset persistence: deferred).
    * **The spring ramp is time-driven with a guaranteed exact-center final
      message:** 50 ms linear from the release value, emitted change-only from
      `hostmpe_strip_tick` on the shell's frame drain; a single late tick
      still lands exactly at 8192 (unit-tested). Grabbing mid-ramp cancels it.
      A sustain MODE switch while ON emits the OFF — never a stranded pedal.
    * **Lattice displacement is an overlay resize:** the strip takes a docked
      band (≤ 15% of height, min with 96 pt) and the overlay view gets the
      remainder — its bounds-normalized probe coordinates and lattice remap
      automatically, zero core involvement. Consequence, accepted: loopback
      drops land at FULL-canvas layout positions, so a cell and its drop are
      vertically offset by up to the strip height while the strip is docked;
      the §6 pixel-perfect alignment property holds only with the strip
      hidden. Flagged for the user's feel pass rather than silently absorbed.
    * The wheel joystick metric is POINTS (travel bound 60 pt), not
      canvas-height units: the strip is a fixed-height bar, and the §3.2 knee
      only requires Δ and r_max to share one metric.

31. **Device test rejected the docked-band strip; §8 amended to a compact
    floating palette top-left, over the full-canvas lattice.** Two findings
    from the first on-device session:
    * **Drop-under-finger is non-negotiable.** The §8 draft's "displaces the
      lattice" band resized the overlay, remapping probe coordinates to the
      reduced play area — but loopback drops land at FULL-canvas layout
      positions, so every touched cell and its drop were offset by the strip
      height. The #30 entry flagged this consequence for the feel pass; the
      verdict: it "breaks the feeling of the controller". The overlay now
      always keeps the full bounds (alignment exact, the §6 property restored
      everywhere); the strip floats at the top-left (~300×86 pt, translucent),
      consumes its own touches, and hides only the corner cells under it.
      The dock top/bottom setting is gone with the band.
    * **A held sustain released itself after 0.5 s — the CC-editor long-press
      recognizer was the culprit, not the engine.** UILongPressGestureRecognizer
      cancels the view's touches when it fires (UIKit default), so holding the
      pedal for half a second delivered touchesCancelled → sustain up, while
      the engine and byte log looked "correct". Fix: the recognizer's delegate
      only lets it receive touches over the two ASSIGNABLE wheels (the only
      widgets with an editor). Hardened alongside: every widget touch is now
      tracked in the grab table (sustain included), so a release resolves by
      its grab record, never by where the finger happens to lift. Sustain
      stays MOMENTARY by default (the user wants the press-and-hold pedal
      feel); the latch toggle remains a setting.

## Step 19

32. **v0.4 operator batch — resolutions from implementation and measurement.**
    Version 0.4.0; `sumi_add_vortex` gained the profile argument (breaking,
    all call sites updated); params grew (slide_mode, vortex_profile,
    ripple_bake, ripple_angle); ctl dims 7/8 (ripple amp/freq).
    * **Wake sign: the spec draft's outer formula was inside-out.** With the
      draft's `+`, the outer field at the front seam gave P + d⃗ while the
      inside rule gives P − d⃗ — contradicting its own zero-seam claim. The
      doublet satisfying the no-penetration boundary is φ = −U a² x/r², whose
      displacement field enters the inverse lookup as P_src = P − Δ. Spec
      formula corrected; the §4.3(4) acceptance test (front bulges forward,
      flanks stream backward) passes numerically and visually (evidence PNGs).
    * **Sub-step budget: a/2 is the fold THRESHOLD, not a safe budget.** At
      per-pass displacement d the inverse-map Jacobian at the rear stagnation
      point is 1 − 2d/a — exactly zero at the spec's "≤ a/2". The core
      sub-steps at ≤ a/4 (det ≥ 0.5 everywhere; measured min det 0.44 after
      an 8×a one-frame flick). Spec corrected. Also learned: the correct
      no-fold test is the pre-image Jacobian over the FLUID region — the tip
      corridor carries the body's slip surface (a genuine tangential
      discontinuity of potential flow, not a fold), and u-monotonicity along
      a row is wrong off the symmetry axis.
    * **`sumi_add_pinch(x, y, k_delta, angle)` is a public gesture-ABI
      addition** beyond the spec's §5.3 delta list: the fold axis is
      host-side data (pen azimuth in Step 20, drag angle in the harness) with
      no MIDI path, and the roadmap's harness binding requires it. The MIDI
      route stays: slide_mode = 1 drives the same pass from smoothed per-voice
      CC74 DELTAS at the voice position, fold axis defaulting to the voice's
      lattice pitch axis; the first CC74 of a voice snaps (primes) instead of
      pinching, so a controller's rest position never fires a spurious
      gesture. Window S = 0.02 (shared constant). In slide_mode 1, slide no
      longer modulates aux (one controller, one meaning).
    * **Pinch soak semantics.** (+k, −k) pass pairs at one center/angle invert
      exactly (s = xy is conserved along trajectories, so the −k pass sees the
      same w(s) field): 500 strong pairs hold band areas to < 2%. The DONE
      stream runs through the real slide_mode-1 route (gesture-rate CC74
      wobble, 36 000 frames). NOT asserted, deliberately: an adversarial
      schedule (full-strength k, fold axis rotating every pass) is chaotic
      advection — it filaments ink below texel resolution where bilinear
      resampling averages it to gray, the same way real marbling over-folds
      to mud. That is the §4.1 resampling medium at work, not an operator
      area leak (verified: the analytic map has det = 1; the drained ink
      tracks filament width crossing the texel scale).
    * **Pinch variant pick (by eye, from the evidence pair): HAMILTONIAN.**
      The saddle stretches one diagonal while compressing the other with four
      fading crease arms — pinched paper. The crossed-tine composition merely
      translates material along two lines (no stretch/compress pairing) and
      reads as a lumpy directional smear. PNG pair in the step-19 evidence;
      the harness keeps the X-key prototype for re-judging.
    * **Ripple ctl dims ship UNMAPPED in the default CC map.** The spec enum
      comments suggested "dflt: CC1" — but CC1 → vortex strength is
      load-bearing (Step 18 DONE: the mod wheel stirs the vortex), and
      "bend" is not a CC. The harness maps CC 102/103 locally for its keys;
      the strip's assignable wheels take them on-device (Step 18 rationale).
      Spec comments cleaned. φ has no control surface in v0.4 (fixed 0):
      ripple_angle already rotates the pattern frame, and the k control
      demonstrates the same commutativity boundary a φ control would.
    * **The ripple group-identity DONE runs on the LIVE path** — an LFO on A
      through the composite view displacement leaves the field BITWISE
      identical because live ripple never writes (2 097 152 bytes memcmp).
      Bake-mode passes compose additively in exact math, but each ping-pong
      pass resamples (like every operator); k/angle changes bake residue by
      design. The amp = 0 live branch keeps the un-rippled composite
      bit-identical to v0.3.
    * **Crease-ring measurement is one-sided by construction:** the true
      |dα/dr| is zero inside R and maximal immediately OUTSIDE (1/r³ decay),
      so a sampled max-gradient centers just past R plus a texel of bilinear
      smear. Asserted as [R, R + 3 texels], never inside the rigid core —
      the sharp half of "exactly at R". Interior rigidity: 20 full rotations
      leave interior ink at mean |Δ| 0.00000 vs 0.758 for the exponential
      control at the same total angle.
    * `sumi_midi_harness_inject` (desktop): synthetic bytes through the SAME
      §5.2 producer mutex as device callbacks — the ripple keys ride the real
      MIDI/ctl path without a second producer.

33. **Mass conservation on a finite canvas: the ingress rule extends to the
    v0.4 operators, and the pinch-soak DONE is re-grounded on the medium's
    own baseline.** Measured on the way to the §4.3(5) soak gate:
    * **Edge-clamp FABRICATES ink.** The pinch's fold-axis corridors cross
      the canvas edge at full strength (w does not decay on the axes); with
      clamp-to-edge sampling, every compression half-cycle duplicates
      boundary content inward — measured +9.5% ink mass over 12 000
      gesture-rate passes. The §3.4 scroll ingress rule ("a source beyond
      the canvas is fresh water — this cannot be delegated to sampler
      state") now applies to the WAKE, PINCH and RIPPLE-BAKE passes too.
      Drop/tine/vortex keep their v1 clamp behavior (fixture-pinned;
      flagged, not changed): measurable only under torture — a 36 000-pass
      glide-tine stream NETS +5% mass as clamp fabrication overtakes
      erosion. Normal play never sustains such chains on one voice;
      extending the ingress rule to the v1 operators would break the
      committed cross-backend fixture and belongs to a deliberate future
      decision, not this step.
    * **Bulk erosion is the resampled medium, not the operator.** After the
      ingress fix, a 6 000-pass CC74 stream still lost 10.6% ink mass —
      spatially uniform across the inked region, ZERO edge component. The
      control: the identical stream driven through the v1 GLIDE TINE erodes
      6.9% (1.15e-5/pass vs the pinch's 1.76e-5/pass — same mechanism,
      same order). Every sub-texel warp pass pays a small bilinear-resample
      mass fade; 36 000 passes of ANY operator fail a strict "conserves
      within noise" reading, the incumbent tine included. Chaotic schedules
      (full-strength k, rotating fold axis) additionally filament ink below
      texel resolution — over-folded real marbling mixes to gray the same
      way.
    * **The soak gate, re-grounded:** (a) det = 1 verified symbolically
      (the streamline window keeps the Jacobian exactly 1 — derived in
      review); (b) 500 strong (+k, −k) pairs invert analytically (s = xy
      conserved along trajectories) and hold ink mass to ±0.5%; (c) zero
      fabrication (mass never grows > 0.5%); (d) the pinch's per-pass
      erosion ≤ 2× the glide-tine baseline under the identical stream.
      Level-set areas and band-parity histograms were rejected as
      observables — blur moves any threshold's contour and parity mixes to
      the regional mean; ink MASS (Σ phase) is what the analytic map
      conserves. **The roadmap DONE's literal wording ("10-minute stream
      conserves total ink band area within measurement noise") is
      unsatisfiable on this medium for any operator and needs the user's
      amendment to the four-part gate above.**

34. **Both pinch looks ship; `pinch_variant` params switch (user override of
    the #32 pick).** Shown the step-19 pair, the user kept the crossed-tine
    variant ("worth having") beside the Hamiltonian saddle. Resolution: a
    v0.4 params field (0 = saddle, 1 = crossed tines; 0.4.0 was uncommitted,
    so the struct amendment folds into the same version) honored by BOTH
    pinch routes — the MIDI path (slide_mode = 1) and `sumi_add_pinch` —
    via one shared constructor (`sumi_deform_crossed_pinch`, displacement.cpp:
    the step-19 prototype verbatim, one tine along the fold axis + one along
    the perpendicular; |k| → magnitude × 0.2, the demo's calibration; k's
    sign reverses both drags). Costs two passes per emission (the mapper
    reserves 2 × echo_count). The crossed look is NOT area-preserving in the
    saddle's exact sense — it is two ordinary tines, with the tine's known
    behavior. Surfaced on iOS ("Slide (CC74)" section: Hue/Pinch routing +
    Saddle/Crossed style) and Android (same rows in the settings dialog);
    desktop key `C`. The pinch-demo evidence pair now renders the crossed
    variant through the real params path. Also fixed while wiring Android:
    `nativeSetLayout` still rejected layout > 4, silently ignoring the
    Piano-grid picker entry added in #29.

35. **`bend_mode` — the sine ripple's toggle, CORRECTED SAME-DAY: it governs
    the PER-NOTE pitch bend, not the master bend, and the mod wheel / vortex
    routing is untouched.** The first implementation misread the user's spec
    note as master-bend routing and paired it with a CC1→ripple-amp remap;
    the stated intent: "make the vibrato more subtle when the music requires
    that" — a note's bend wobble should shimmer the water instead of
    wiggling its drop. As implemented:
    * **Mode 0 (default)** = v1 glide: a note's bend drags its drop along
      the pitch axis. **Mode 1** = the per-note bend feeds the sine ripple's
      wavelength; the drop HOLDS (one consumer owns the note bend). Master
      bend keeps its v1 shear tine in BOTH modes; CC1/vortex untouched.
    * **The amount IS the bend's distance from center** (second same-day
      refinement, the user's design: "as with the glide — we even test that
      we can come back from a ripple"). amp ctl = |semis| / 6, clamped
      (±0.5-semitone vibrato breathes ~8%, |±6| saturates; last writer wins
      across voices, smoothed like any global control). The water stills
      ITSELF: bend re-centers → amount 0 (and the bake deltas compose back —
      the group property); the last note's release → amount 0; a mode flip
      1→0 zeroes the residual target (mapper tracks the flip). The
      wavelength k stays a flavor ctl (RIPPLE_FREQ, resting mid-range 0.5;
      CC 103 / a strip wheel adjusts it). No amount slider — removed.
    * **Clean flips:** in mode 1 the voice's glide target is left untouched,
      so switching back to glide lets the smoothed glide catch up to
      wherever the bend actually is — no jumps, no stale-delta tines
      (unit-tested both directions in `test_bend_mode_single_consumer`,
      which also holds the roadmap's OR-never-both gate: a member-channel
      bend sweep emits glide tines XOR ripple passes).
    * **Shells:** iOS/Android map CC 102/103 → amp/freq (otherwise-unused
      CCs, external/strip handles; in mode 1 a CC-102 writer and the bend
      share the amp slot last-writer-wins). iOS "Note bend" section
      (Glide/Ripple); Android row; desktop key `M` (R/T = CC 102, F/G =
      CC 103 as before).
    * Params grew inside the still-uncommitted 0.4.0 (`bend_mode`, dflt 0).
      Housekeeping: spec/roadmap files are edited by the USER only — agents
      record here and the user folds decisions into the documents.

36. **Ripple vibrato is PERMANENT, like glide (user request: "it fixes
    itself, contrary to the glide — make it permanent").** The self-healing
    the user saw was the LIVE insertion point (a view-only displacement,
    §4.5). Resolution, using the spec's own designed mechanism (§4.3(6):
    "changing φ between passes bakes residue in — that residue IS marbling"):
    * The Ripple toggle now selects `bend_mode = 1` AND `ripple_bake = 1`
      together on iOS/Android/desktop-M — there is no separate live/bake
      control on the tablets; the toggle IS the choice. (Desktop keeps `K`
      as a manual live/bake override for experimentation.)
    * Under BEND-driven bake, the emitted pass phase drifts with activity
      (φ += |ΔA|/A_max × 1.5 rad per pass, wrapped): an excursion never
      retraces exactly, so each vibrato cycle lays a slightly shifted comb —
      a faint feathered record that accumulates with the music, the way
      glide leaves tines. The DYNAMIC still stills (#35's three come-back
      paths hold: the amp ctl goes home on re-center/release/mode-flip);
      the MARK stays.
    * CC-driven bake (bend_mode 0) keeps φ fixed — the pure composing-back
      group property, so the step-19 ripple-group DONE gate is untouched
      (re-verified). The LIVE path also survives unchanged underneath: a
      CC-102-driven shimmer while in Glide mode remains the self-healing
      view effect.
    * Proven at field level: `--ripple-permanence-test` — rings + a 3-cycle
      ±2-semitone vibrato ending at center, note released, amp settled to
      zero → 76,540 far-field texels permanently changed (u channel,
      bitwise), while the same scene through the group test's CC path
      composes back (mean |du| 0.0003). Unit level: the mode-1 sweep's
      ripple passes carry drifting phases (asserted); glide-XOR-ripple and
      all #35 gates re-pass.

## Step 20

37. **Lamb–Oseen swirl & bipolar press — resolutions from implementation and
    measurement.**
    * **Swirl rate is CORE-ANGULAR:** the accumulated per-frame step is core
      rotation in radians (ω = SWIRL_OMEGA·amount·expansion_rate, ω_max =
      2 rad/s), converted at emission to the pass strength S = θ_core·2π·r_c²
      with the CURRENT r_c — so the core's felt speed is amount-proportional
      and the far field scales with the drop's own size (a grown drop stirs
      farther, by physics). Steps below 0.0008 rad merge (budget starvation
      carries over, like press growth); echo sets emit all-or-none.
    * **GLSL has no expm1** — the shader guards the 0/0 form with the series
      branch below x = r²/r_c² < 1e-3 (θ = S·(1 − x/2)/(2π·r_c²), whose x = 0
      value IS the analytic θ(0) limit). The guard is verified CPU-side
      against the analytic limit (1e-3 relative) with branch continuity
      2.3e-5; the field shows no NaN within 2 r_c and a bounded core.
    * **Half-float ULP freeze at the core, found while measuring:** per-pass
      swirl displacement θ·r falls below the u/v channel's storage quantum
      near the center (the quantum depends on the texel's VALUE — one voice's
      neighborhood accumulated while another's froze at identity, u ≈ 0.14
      vs 0.28). PROTECTIVE in practice — sub-quantum stirring cannot erode
      the core — but field measurements near r → 0 read low, never high;
      the swirl-test asserts boundedness there and does its profile checks
      at radii above the quantum. Same medium family as #33.
    * **press_mode does not reroute the wind brush** (breath is the brush's
      life; §2.3 semantics win) — it arbitrates 0xD0 on MPE member channels
      only. 0xA0 routes to the swirl unconditionally, keyed by the voice's
      note (a stray note number is ignored).
    * **Lift releases an engaged swirl half:** touch_end emits 0xA0 0 between
      pressure-0 and Note Off when the down axis was engaged — a synth
      latching poly AT must not stick. Buffer contracts grew (touch_update/
      end need 3; panic 77) — header comments updated, all call sites already
      passed larger buffers.
    * **Counter-rotation is drop-counter parity** (phase_base odd/even), so
      consecutively struck notes counter-rotate regardless of pitch —
      field-verified: +1.50 vs −0.65 rad about two cells' centers.
    * Core coherence, field-verified: ring sharpness inside 0.7 r_c retained
      EXACTLY (0.0143 → 0.0143 mean |∂ink/∂x|) through ~4 rad of stirring
      while 29 014 texels moved in the 2–3 r_c annulus — the drop really is
      the vortex core.
    * Full regression: the entire step-19 battery + fixture (bitwise) + ctest
      re-pass with the new operator in the build (19 ok / 0 fail).

## Step 21

38. **Stylus legato, wake & pinch — resolutions.**
    * **Engine split:** the SHELL owns the probe (cell under the pen,
      displacement projected on the anchor cell's semitone axis); hostmpe
      owns pitch state (`pen_begin/bend/retune/slide/pressure/tick`) so the
      golden traces are headless and Android inherits the engine verbatim.
      Pen voices come from the same §5.1 allocator and end through
      `hostmpe_touch_end` (the Note Off releases the CURRENT anchor after
      re-anchors — unit-asserted).
    * **Re-anchor accounting:** the shell keeps sending TOTAL displacement D
      from the strike; hostmpe subtracts its accumulated `pen_offset`. At
      |D − offset| ≥ 47: offset += round(eff), note += the clamp-safe same
      amount, emit center bend → Note On → residual bend as ONE batch. The
      shell sends any batch containing a Note On WHOLE as strike class
      (exempt) — a re-anchor seam must arrive intact on every transport.
      Golden: a 60-semitone sweep tracks pitch within 1 cent ACROSS the seam,
      exactly one re-anchor, monotone throughout.
    * **Piano retune ramp:** 30 ms (inside the spec's 20–40), tick-driven on
      the frame drain like the strip spring; lands EXACTLY on target; a
      mid-ramp retune restarts from the current interpolated pitch (golden:
      the first post-restart bend lies strictly between the half-ramp pitch
      and the new target). Dead zones = no calls = sustain, by construction.
    * **slide_mode = 1 + pen: CC74 goes OUTBOUND ONLY.** The shell drives the
      azimuth-fold pinch through `sumi_add_pinch` (azimuth has no MIDI path);
      pushing the same CC74 into the loopback would have the mapper emit a
      SECOND pinch at the voice position. A DAW replay still pinches — via
      the mapper's CC74 route with the pitch-axis fold (the humbler axis);
      live keeps the azimuth. slide_mode = 0 sends CC74 to both pipes (aux
      hue, the v1 meaning). Logged as the §5 loopback-conformance exception
      beside the wake (which is physical, never MIDI).
    * **Velocity from real force** (§4) — CORRECTED after the first DAW test
      (strikes recorded near-silent): touch-down force is sampled BEFORE
      contact force builds, and normalizing by maximumPossibleForce (≈4.17
      finger-units on the Pencil) compressed everything toward zero. The
      calibration is UIKit's native unit (UITouch.force: 1.0 = an average
      finger touch): velocity = 96 + (max(force, 1) − 1)·15.5, clamped 127 —
      a baseline tap equals the finger default (96), force 3 is maximally
      loud, and sub-baseline touch-down readings clamp UP to baseline so the
      pen never whispers by accident. Wake tip radius stays
      a = 0.006 + 0.030·(force/maximumPossibleForce).
    * **Tilt → master CC 1 by default** (altitude → 0 upright..1 flat,
      change-only): stirs the vortex through the default map — remappable via
      the CC map like every assignable. Hover (M2 iPads) draws a ghost ring
      in the overlay; inert elsewhere. Palm rejection: unchanged, deliberately
      (pencil touches are typed; fingers keep the joystick path).
    * CC74 default for pens is CENTER (64) at pen-down — the §3.3 stylus law
      — with change-only emission from there.

39. **Stylus legato REDESIGNED to per-cell same-channel retriggers (user:
    "the stylus was mainly introduced to make a legato–glissando... right now
    it never changes note and behaves like the finger").** The diagnosis was
    architectural: the finger joystick ALREADY bends continuously and
    semitone-exactly (#10's identity-beyond-the-circle), so §7's
    continuous-bend pen added nothing audible — the stylus's reason to exist
    is REAL NOTE CHANGES. Replaces #38's bend/retune design:
    * `hostmpe_pen_glide(voice, cell_note, offset_semis, velocity)` — the
      shell probes the cell UNDER the pen each move; crossing into a new cell
      emits the legato overlap idiom: bend(offset) → Note On(new, velocity
      from the CURRENT force — glissando dynamics are live) → Note Off(old).
      Same channel throughout: mono/MPE synths glide, the DAW records real
      terminated notes, our normalizer's same-channel steal ignores the stale
      Off. Inside a cell the offset from its center is the bend — vibrato
      without retriggering. Sounding pitch is CONTINUOUS across crossings
      (the offset flips sign as the reference cell changes) — golden: a
      12-cell sweep shows 12 retriggers, 12 Offs, sub-cent tracking
      everywhere, monotone.
    * The ±47 re-anchor rule is RETIRED (bend never accumulates past ±½ cell)
      and with it the piano-grid 20–40 ms retune ramp (the synth's own
      legato/portamento is the smoothing; retriggering IS the note change).
      Dead zones still sustain by construction (no probe hit → no call).
      One behavior for all three playable lattices.
    * Retrigger batches ship WHOLE as strike class (the bend→On→Off crossing
      must arrive intact); bend-only batches stay policed continuous.
    * **Boundary hysteresis (same-day refinement — "bend should still exist
      inside the cell"):** a crossing commits only once the pen is ±0.65 st
      past the CURRENT note; until then the true pitch offset keeps bending
      the current note, so vibrato near a cell edge bends instead of
      machine-gunning retriggers (golden: a ±0.1-st wobble across a boundary
      emits bends and ZERO Note Ons; a deep push commits exactly once). The
      end-to-end path is also unit-proven: the pen's in-cell offset bend
      routes through the bend_mode machinery like any per-note bend — mode 0
      glide tines, mode 1 ripple breathing (the amp then tops out at
      |0.5|/6 ≈ 8% because a cell bounds the pen's bend — flagged: raise the
      /6 saturation if the pen's shimmer needs more presence).
    * Visual consequence, deliberate: each retrigger is a VoiceBegin — a pen
      glissando paints a trail of drops across the lattice (band parity
      alternating), unlike the finger's single dragged drop. The pen finally
      LOOKS different too.

40. **Pen barrel controls are DERIVATIVE-ONLY dials; tilt→CC1 removed
    (user: azimuth-class sensors "generate a lot of noise and are nearly
    impossible to go back to zero").** The math that makes derivative-only
    right: Σdeltas telescopes to (current − value-at-strike), so sensor noise
    never integrates beyond instantaneous jitter and the reference is where
    YOUR hand started, not a compass zero. As implemented:
    * **Azimuth derivative → bend multiplier** (latch-style, ×[0.25, 3],
      quarter-turn ≈ ×2, shortest-angle deltas): twisting the pen dials the
      depth of the in-cell bend live. `hostmpe_pen_glide` gained
      `bend_scale`, which multiplies the EMITTED bend only — note tracking
      and the #39 hysteresis stay on raw geometry, so cells commit where the
      pen physically is (golden: 0.3 st at ×2 emits bend14(0.6); a scaled
      boundary wobble still never retriggers). This also lifts the pen's
      ripple-mode ceiling flagged in #39 (~8% → ~25% at ×3).
    * **Barrel roll (Pencil Pro rollAngle, iOS 17.5+) derivative → the mod**
      (CC 1, latch-style 0..127, quarter-roll ≈ full sweep) — and the vortex
      it stirs sits AT THE PEN, not the canvas center: the shell rides
      CC 21/22 (vortex X/Y in the default map) with the pen position,
      change-only, master channel, both pipes. Non-Pro pencils simply have
      no roll: the dial is inert, nothing else changes.
    * **Tilt (altitude) → CC 1 REMOVED** — a nuisance: altitude drifts with
      natural hand posture, exactly the always-on absolute sensor the
      derivative rule exists to avoid.
    * **Posture gate (same-day correction — "tilt still triggers CC 21/22"):**
      tilting cross-talks into the reported roll and azimuth, so a posture
      change was walking the roll dial and firing the vortex-position CCs.
      Both dials now FREEZE while altitude is moving (|Δalt| > 0.02/event —
      a tilt is posture, not a gesture); azimuth is additionally ignored when
      the pen stands near vertical (altitude > 1.2 rad, where UIKit's azimuth
      estimate swings wildly); per-event deltas are spike-clamped (±0.2 az,
      ±0.3 roll) and the roll dial has a 0.004 rad jitter floor. Latch-style
      + deltas remains the point: a still hand is a still value — the dial
      moves only when the hand does, holds where left, and a regrip never
      jumps it.

41. **Step-21 polish batch (user-directed): one bug, two usability fixes, one
    visual-feedback removal.**
    * **Marble pinch on iOS (the bug — never implemented):** a literal
      two-finger UIPinchGestureRecognizer → `sumi_add_pinch` at the gesture
      centroid; the fold axis IS the finger-to-finger line (point space is
      isotropic, so its angle is the aspect-corrected fold angle directly),
      the squeeze is the delta-driven k (×1.5 per unit scale change, spike
      floor 0.0015). Runs simultaneously with tap/pan/twist; disabled in
      Play mode with the other marble recognizers.
    * **Piano grid: narrow accidentals + white-key tops** — accidentals
      shrink to 0.6 white units (real black-key proportions) and the
      black-row area they do not cover belongs to the NATURAL below, so a
      horizontal glissando passes natural→natural without grazing
      accidentals — "the whole point of the piano layout". Consequences:
      the E–F / B–C gaps and row ends are white-key tops (NO dead zones
      remain on this lattice — supersedes #29's dead-zone rule and the pen's
      piano-gap sustain), and the accidental R_max/knob is proportionally
      smaller (0.6 keys), like a real black key. Goldens updated (gap top =
      the natural; 0.25 units from an accidental center = still the
      accidental; accidental radius < natural radius).
    * **Two-tone lattice:** each cell ring gets a paper-cream halo (3 pt,
      α 0.55) under the dark stroke, so cells stay legible over dense ink;
      ACCIDENTAL rings render last (on top, slightly darker) — black keys
      sitting on the keybed. "Reverse the order of the accidental" realized
      as z-order.
    * **The lift ring is REMOVED** (the §3.4 "drop sets + surfactant ring"
      and #21's home-cell placement are superseded): the user — the clear
      drop at note-off "is really ruining the experience; the joystick's
      disappearance is feedback enough". A lift now simply stops the feed;
      nothing is stamped. Goldens updated (zero rings at any lift velocity,
      zero per echo).

## Step 22 (Android, on the Linux box)

42. **The play surface is a Kotlin `View` hosted in Compose, not a
    `pointerInput` modifier.** Compose's pointer API carries pressure and
    tool type but not `AXIS_TILT` / `AXIS_ORIENTATION`, which the S-Pen
    posture gate and the azimuth tail-stir booster (#40) both need — and
    `onHoverEvent` is where `ACTION_HOVER_MOVE` arrives for the hover ghost.
    So `PlayOverlayView` and `ControlStripView` are Views (the iOS
    `UIView`s' siblings, event for event) mounted with `AndroidView` inside
    the same `Box` as the `SurfaceView`. Compose still owns the chrome and
    the settings dialog. Consequence, deliberate: the overlay keeps the FULL
    canvas bounds and the strip floats over it at the top-left (§8 rev /
    #31), so a touched cell and its loopback drop stay exactly aligned.

43. **S-Pen velocity calibration is normalized-pressure based, not UIKit
    force units.** `MotionEvent.getPressure()` on the Wacom EMR digitizer
    (`sec_e-pen`, ABS_PRESSURE 0..4095) is normalized to 0..1, so the iOS
    formula (`96 + (force − 1)·15.5` in units where 1.0 = an average finger
    touch) has no meaning here. Android maps `t = (p − 0.15)/(0.75 − 0.15)`
    clamped to [0, 1], velocity = 96 + t·31 — the same SHAPE as #38: a
    baseline tap plays at the finger default (96), a hard press is 127, and
    sub-baseline readings clamp UP to 96 (touch-down pressure is sampled
    before contact force builds). **The two constants are the one thing in
    this step that a human hand must confirm** — `adb shell input stylus`
    synthesizes pressure 1.0, which only exercises the top of the curve
    (velocity 127, verified in the byte log). Flagged for the user's device
    pass; they are two named constants in `PlayOverlayView.kt`.

44. **The BLE-MIDI PERIPHERAL is a hand-rolled GATT server, and its packet
    queue must drop from the NEWEST end.** Android's `MidiManager` implements
    only the BLE-MIDI *central* role (that is how the ROLI reaches us), so
    §5.4(c) needs an explicit `BluetoothGattServer` on service
    `03B80E5A…` with the `7772E5DB…` characteristic + CCCD, BLE-MIDI 1.0
    framing (header timestamp-high, then per message a timestamp-low byte),
    one notification in flight per link, the next sent on
    `onNotificationSent`. Two findings, both measured against the Linux
    central (BlueZ 5.83's MIDI GATT profile, which publishes the link as an
    ALSA sequencer port):
    * **A bug this shipped with for one build:** every `send()` built its own
      packet, so the 80-message MCM/RPN0 handshake (dispatched one message at
      a time) queued ~79 single-message packets; the backlog cap then
      `pollFirst()`ed the OLDEST — silently eating the master MCM and the
      first four members (capture: "RPN 0 = 48 on 11/15 members", MCM
      missing). Now messages COALESCE into the tail packet while a
      notification is in flight (an 80-message burst becomes one or two
      packets at MTU 517), the safety cap drops from the NEWEST end so the
      head's handshake and note events always survive, and a link that takes
      nothing clears the queue instead of spinning the backlog into the void.
    * Timestamps inside one packet must not go backwards (BLE-MIDI 1.0), so
      appending to the tail packet is gated on the same timestamp-high byte
      and a non-decreasing timestamp-low.

45. **USB gadget MIDI: the peripheral port is a `TYPE_USB` device with NO
    host-side `UsbDevice`.** When the user flips the system USB mode to MIDI,
    Android publishes the class-compliant gadget as a `MidiDeviceInfo` named
    (localized) "Android USB Peripheral Port"; host-mode MIDI devices carry
    `PROPERTY_USB_DEVICE`, the gadget does not. Either signal identifies it.
    Status for the §5.4 surfacing (active / charge-only / unsupported) comes
    from the sticky `android.hardware.usb.action.USB_STATE` broadcast, whose
    extras are `connected` plus one boolean per ACTIVE gadget function
    (`midi`): mode on with no port after ~3 s means the OEM has no ALSA MIDI
    gadget. Measured on the SM-X906B: the Linux host lists the tablet within
    ~150 ms of the flip (`amidi -l` → `hw:4,0 SAMSUNG_Android MIDI 1`), the
    port opens, and `nativeSinkAppeared` re-sends the handshake — the
    mid-session USB-mode-flip case of #28, on Android.

46. **The Phase-4 host half lives ON the AMidi poller thread** (§5.2 /
    DECISIONS_2 #33), which is now more than a poller: it owns `hostmpe_t`,
    the strip engine, the three per-transport limiters and the byte log, and
    it is the only thread that calls `sumi_push_midi`. The UI thread reaches
    it through a command queue (the iOS serial `midiQueue`'s sibling, #14);
    touch-down, pen-down and the strip-state read are SYNC hops (the voice id
    must answer before the overlay can track the touch — microseconds); its
    1 ms poll cadence became a condvar wait so a posted command wakes it
    immediately. Outbound WIRE writes go the other way: a JNI upcall
    (`NativeBridge.outboundWrite(sink, bytes, len)`) because the endpoints
    are Java objects (`MidiInputPort`, the `MidiReceiver` of the
    `MidiDeviceService`, the GATT server) — the limiters stay native, so the
    policy has one implementation for both shells.

47. **The shell owns a params SNAPSHOT, and a cold start re-sends the
    loopback handshake.** The probe is instance-free precisely so hit-testing
    runs on the UI thread (#2), so the host-owned params fields
    (sim_scale, layout, slide_mode, pinch_variant, bend_mode, ripple_bake,
    press_mode) now live in a mutex-guarded snapshot the UI thread reads and
    the render thread applies — `nativeLayoutProbe` / `nativeLatticeSweep`
    answer from it with no queue round trip. Fallout found on device: with
    Play mode PERSISTED, `nativeSetPlayMode` fires before the surface exists,
    so the MCM/RPN0 push reached a null instance and the normalizer never got
    its mode flip. The render thread now calls `play_instance_ready()` right
    after `sumi_create`, which re-sends the config if Play mode is already
    effective (the transports had theirs; only the loopback was missing).

48. **The §4.6 field dump must run on a FRESH app start.**
    `sumi_debug_run_field_script` queues the canonical seven passes onto
    whatever the field already holds — it does not reset. Dumping after a
    play session compared at max |Δ| 6.0 (aux) against the Metal fixture,
    which looks exactly like an orientation break and is nothing of the kind.
    From a cold start the dump reproduces the step-14 numbers EXACTLY —
    max 1.513672e-02, mean 3.757610e-04, the values recorded in DECISIONS_2
    #30 — so the v0.4 core is unregressed on GLES3 under the documented
    mobile tier (max ≤ 2.5e-2, mean ≤ 1e-3; the strict desktop 1e-2/1e-4
    default still fails by construction on the Adreno's fp16 lerp profile).

49. **Two-byte messages must ship as two bytes.** iOS hands CoreMIDI complete
    messages and lets it frame them; on Android both wire paths
    (`MidiInputPort.send`, the BLE framer) take RAW bytes, and
    `hostmpe_msg_t` always carries three. Channel pressure (0xD0) and program
    change (0xC0) are two-byte messages: a third byte would be parsed by the
    host as running-status data — a phantom pressure/note after every
    pressure message. The wire encoder trims them; the loopback is unaffected
    (`sumi_push_midi` takes the triple).

50. **Evidence tooling for a tablet whose transports land on THIS box.**
    * `tests/midi_capture_alsa.cpp` — subscribes to every matching ALSA
      sequencer port and logs `t_s,port,port_name,status,d1,d2` with one
      CLOCK_MONOTONIC stamp, so arrival-time DIFFERENCES between transports
      are meaningful with no device clock sync (that is how "USB beats BLE"
      is asserted: matched Note Ons, median BLE − USB = **+32.5 ms**). It
      also watches the System Announce port, so a sink appearing MID-capture
      (the USB mode flip) is captured too.
    * `tools/midi_asserts.py` — the emit-order / no-finger-CC74 /
      channel-steal / handshake / sustain-balance asserts over both the
      device byte log and the Linux capture (the iOS step-16 assert script,
      generalized). Note the sustain assert: the strip's ANNOUNCE repeats
      CC 64 = 0 by design, so the invariant is "every ON is followed by an
      OFF and the log ends released", not a 1:1 count.
    * `tools/pen_trace.py` — a LIVE S-Pen legato trace: reconstructs
      sounding pitch (note + bend/171) per stroke and asserts same-channel
      bend→On→Off overlaps, continuity across crossings, monotonicity, and a
      released end. Pen releases are logged as src 4 (`nativePenEnd`) so a
      stroke reads whole.
    * The full headless suites run ON-DEVICE: `tests/hostmpe_tests.cpp` and
      `tests/normalizer_tests.cpp` compile into `libsumi-shell.so` with
      `main` renamed by a `COMPILE_DEFINITIONS` (`main=hostmpe_tests_main`),
      and `nativeRunSelfTests` redirects stdout/stderr into app files.
      Result on the SM-X906B: **1,559 + 14,997 checks pass** — the same
      counts as the desktop ctest run, same code, arm64.

51. **The USB gadget's rawmidi buffers while nothing is subscribed.** The
    first ALSA capture after connecting delivers the whole backlog in one
    burst at subscribe time (458 messages stamped inside one millisecond),
    which reads like a broken clock. Drain once (a short capture to
    /dev/null) before any timing measurement — the evidence runs do.

52. **The live pen trace's continuity assert is SPEED-RELATIVE, and it does
    not apply to PIANO_GRID.** Writing the analyzer produced two corrections
    worth keeping, both found by measuring on device:
    * **A fixed cents threshold measures the pen's speed, not the engine.**
      The seam at a crossing (sounding pitch after the retrigger versus just
      before the crossing's bend) is bounded by how far the tip moved between
      two touch events, because the bend precedes the Note On and the ±0.65 st
      hysteresis holds the crossing until the pen is into the new cell. A
      hand-speed chroma sweep seams at **4.5 cents** (bound 12.3); the same
      swipe scripted at ~40 st/s seams at 51.8 (bound 100). The assert is
      therefore 3× the stroke's own median per-event pitch step (floor
      0.05 st) — an engine discontinuity fails at any speed, a fast synthetic
      sweep does not. Jankó, slow: 19 crossings of a whole tone each, seam
      **17.8 cents** against a 36.9 bound.
    * **On PIANO_GRID continuity is structurally unreachable, and that is the
      design.** The in-cell bend spans ±0.5 st (half a key along the half-key
      diagonal, #29) while a natural→natural crossing is a WHOLE TONE, so
      ~1–1.6 st of the step has to arrive as a jump: measured **1.63 st** on
      a slow horizontal sweep whose note steps were [1, 2] — the white-key run
      D-E-F-G-A-B, which is #41's "a horizontal glissando passes
      natural→natural without grazing accidentals" heard as pitch, and
      PHASE4 §7's quantized piano glissando. `tools/pen_trace.py` takes
      `--layout`: the seam is asserted on chroma/Jankó and reported on piano,
      where the guard is instead that no crossing OVERSHOOTS the note change
      it stands for (all lattices).
    Recorded because the first version of the analyzer hid both facts behind
    one loose 1.35 st threshold — which would have passed a genuine
    discontinuity on the two lattices where continuity is the contract.

53. **The UI thread's synchronous hop onto the MIDI thread is BOUNDED.**
    Touch-down and pen-down need the allocated voice id before the overlay can
    track the touch (#14's iOS `midiQueue.sync`), so they block the UI thread
    on the play queue. Unbounded, that is an ANR waiting for a wedged or
    already-torn-down MIDI thread — and the `engines_ready` guard has a
    check-then-post race by construction. `play_post_sync` now waits 250 ms
    and returns false (the caller proceeds with "no voice", i.e. exactly the
    saturation path), which is four orders of magnitude of headroom over the
    real cost: the queue drains every ~1 ms poll iteration and each body is
    microseconds. The queued lambda carries a shared abandon flag, because
    every call site captures by reference — a timed-out call must not run
    `fn` later against dead stack.

54. **Review batch: an independent read of the Step-22 diff found real
    defects, including two false-green asserts in the evidence tooling.** All
    fixed; recorded because several are the kind that only a second pair of
    eyes finds, and two of them mean earlier green lines were worth less than
    they looked.
    * **`nativeSurfaceDestroyed` could block the UI thread forever** — the
      §5.4 contract inverted. The render loop's outer wait predicate did not
      include `release_requested`, so a destroy arriving while the thread was
      parked WITH NO SURFACE (which is exactly the state after a failed
      `attach_surface`: no ES3 config, `eglCreateWindowSurface` or
      `sumi_create` failure) was never observed: ANR, force-stop. The
      predicate now covers it and a release with nothing attached is
      acknowledged on the spot. This bug predates Step 22 — it was latent in
      the step-14 loop.
    * **Lost wakeup in the render-thread command queue**: `shell::post`
      pushed under `q_mu` and notified `state_cv` while the waiter evaluated
      its predicate under `state_mu`, so a notify landing between
      "predicate false" and `wait()` was lost. The signal is now taken under
      `state_mu`. (The play half already had this right.)
    * **`AMidiDevice` double-free** for any device with more than one output
      port: `AMidiDevice_fromJava` yields ONE reference, and teardown released
      it once per PORT. Exactly one port entry now owns the release. Alongside
      it: **removed devices never left the poller** — no `nativeRemoveMidiDevice`
      existed, so `AMidiOutputPort_receive` kept being called on a dead port
      every millisecond and a replug appended a second set. Kotlin now passes
      `MidiDeviceInfo.getId()` in and calls the removal.
    * **`MidiInputs` never unregistered its `DeviceCallback`.** With
      `launchMode="singleTask"` the process outlives a back-out, so a relaunch
      left two callbacks live, each with its own `openedIds` — the next device
      to appear was opened TWICE and every incoming message was parsed and
      pushed to the loopback twice (doubled notes, doubled occupancy, doubled
      byte log). Also leaked the destroyed Activity. Related, same shape:
      `onDestroy` did its cleanup only `if (isFinishing)`, so a non-finishing
      destroy (locale/fontScale change, "don't keep activities") left a held
      voice sounding on every sink forever; the cleanup is now unconditional
      and only the process-global native shutdown is gated. The thermal
      listener is removed too.
    * **BLE flow control was one global in-flight counter for N links.** A
      central that dropped (or unsubscribed) mid-notification never acked, and
      the counter only reset when the subscribed set emptied — so with two
      centrals, one leaving killed the pipe for the other for the rest of the
      session. The count is now re-clamped on every membership change and a
      250 ms watchdog resumes a pipe whose ack never came. And the safety
      valve no longer discards the never-dropped class: it drops CONTINUOUS
      packets newest-first and touches a packet carrying notes only when
      nothing else is left (#44 fixed the head, this fixes the tail). MTU
      selection now considers only SUBSCRIBED devices, defaulting to 23 for
      one that never negotiated.
    * **Evidence tooling — two false greens.** (1) The sustain assert was a
      no-op: `stuck = (v == 127)` in a loop **assigns** instead of
      accumulating, so it only ever restated `cc64[-1] == 0`; a session that
      held the pedal throughout with one trailing 0 printed `ok`. It now
      tracks the held state and fails on an unanswered ON. (2) `capture` mode
      **passed on an empty file** — every check is conditional on ports
      derived from the rows, so zero rows asserted nothing and printed ALL
      ASSERTS PASS, which matters because #51 has us running a throwaway
      DRAIN capture before every timing run. Row and port presence are now
      asserted, including that `--usb`/`--ble` actually matched. Added while
      there: the §5.3 rate policies are now ASSERTED via `--policy`
      (≤100 Hz per voice-dimension, ~300 msg/s global) rather than printed;
      the `# dropped_loopback_messages` line the shell writes is now read and
      asserted zero; and "every Note On preceded by a bend" gained the assert
      it was named after — a STRIKE must carry a CENTER bend (§3.1/§5.1's
      in-tune attack), only legato retriggers carry a cell offset.
    * Smaller: `sumi_dropped_midi_count` read under the producer mutex; the
      per-sink counters made atomic; `attach_surface` no longer leaves a live
      surface with no current context; `files_dir` assigned inside the init
      guard and the play half's session state reset on re-init (a stale
      `play_effective` swallowed the new session's transport handshake); the
      teardown panic's messages now reach `midi_log.csv`; `nativeRunSelfTests`
      is once-per-process (the suites keep file-static counters, so a second
      run reported doubled counts and could never pass again); the virtual
      device's client count made Compose-observable; the 26,400-call probe
      sweep moved off the draw path.

55. **The USB-MIDI gadget is BIDIRECTIONAL, which gives the channel-steal
    mechanism a scripted external source.** The peripheral port Android
    publishes in MIDI mode has one input and one output port, and the shell
    already opens every device with output ports for ingest — so the Linux box
    can SEND into the tablet over the same cable it receives on, and those
    bytes reach `hostmpe_observe_external` through AMidi exactly as a hardware
    controller's would. Verified on device: an MCM plus a held four-note chord
    on member channels 2–5 from `amidi -p hw:4,0,0 -S ...`, then six touches —
    all six allocated to members 6–11, zero steal violations. This does not
    replace the DONE gate's ROLI-over-BLE run (the wording names the ROLI),
    but the mask, the merge point and the allocator are now proven against
    live external notes rather than only in the headless suite. Practical
    note learned the hard way: toggling the USB gadget function KILLS adb when
    adb rides the same cable, so any USB-mode work runs over `adb tcpip 5555`.

56. **FLAGGED QUESTION (core geometry, frozen — not changed): the piano
    grid's narrow accidentals stop reading as narrow past aspect 1.5873, and
    the Tab S8 Ultra in landscape sits 1% past that line.** Raised by the user
    ("the piano grid doesn't look like the newer version"). Diagnosis, with
    the Android shell cleared: a headless render straight from the core, using
    the shells' own 220×120 probe sweep, is cell-for-cell identical to the
    tablet screenshot (84 cells, same voids, same sizes) — Android draws
    exactly what the probe returns, and the core does carry #41 (hit-testing
    accidentals are 0.6 white-key units; the E–F gap in the accidental row
    probes to the natural below, note 89 = F, so the dead zones really are
    gone).
    * The knob radius is `0.5 · min(key width in aspect-corrected units,
      octave-pair height)` (#29's R_max bullet). The 0.6 narrowing therefore
      only survives while `0.6 · (0.84/7) · aspect < 0.80/7`, i.e. **aspect <
      1.5873**. Measured: iPad Air 11 landscape (1.439) → accidental R 0.0518
      vs natural 0.0571, **91%**, visibly smaller; Tab S8 Ultra landscape
      (1.602) → both 0.0571, **100%**, identical; the same tablet in portrait
      (0.624) → **60%**, dramatic. One build, three looks — which is exactly
      why it reads as "the Android one wasn't updated".
    * The golden pins `accidental radius < natural radius` at **aspect 1.0
      only** (`normalizer_tests.cpp`), so the aspect-dependence is untested.
    * Options for the user, none taken here (rule: core stays frozen, flag
      instead of coding): (a) scale the accidental's R_max by 0.6 of the
      NATURAL's radius rather than re-running the min against the octave-pair
      height — one line, makes the proportion aspect-independent, changes a
      travel bound and the deadband scale on accidentals; (b) decouple the
      DRAWN ring from R_max in both shells and draw accidentals at 0.6 —
      shell-only, honest visually, but then the ring stops meaning "your
      travel bound"; (c) leave it — the lattice is a hint, R_max is a feel
      decision, and the hit-testing (which is what plays) is already #41.
    * Second, related: #41 removed the dead zones, but the lattice still shows
      VOIDS at E–F, B–C and the row ends, because it draws one circle per
      cell and the white-key-top area belongs to the natural centred a row
      below. Those spots play (the natural sounds); they just do not look
      playable. Shell-side drawing question, same three-way choice.

57. **RESOLVES #56 (core, one line): the piano grid's accidental footprint is
    a SIMILAR rectangle, so the 0.6 proportion is aspect-invariant.** Applied
    on the Linux box at the user's direction (the core's frozen rule yields to
    an explicit instruction; #56 had flagged it and stopped). `probe_piano_grid`'s
    hit region was never the problem — it works in white-key units, so the
    natural-to-natural glissando was correct on every screen. The knob was:
    `cell_radius = 0.5 · min(cell width · aspect, cell height)` with the
    accidental's width scaled by 0.6 but its HEIGHT left at the full octave
    pair, so the narrowing survived only while the width was the limiting
    dimension — below aspect (0.80/7)/(0.6·0.84/7) = **1.5873**. Fix:
    `ch_norm = key_w * (1 - 2·PIANO_INSET_Y) / 7` (one line, `layouts.cpp`),
    which makes the footprint similar and the inscribed circle scale with it
    whichever dimension governs. Measured against the rebuilt library, ratio
    now **0.600 at 1.19 / 1.44 / 1.60 / 1.78 / 2.16 / 0.62** — one look
    everywhere, where before it ran 0.63 → 0.91 → 1.00 → 1.00.
    * **The golden now pins the ratio at seven aspects**, four of them past
      the old crossover. It had asserted only `accidental < natural` at
      aspect 1.0 — the one place the defect could not show — which is why a
      shipped build read "not updated" on a 16:10 tablet. Negative control
      run: with the old line restored the extended golden fails at every
      aspect (0.630, 0.756, 0.907, 1.000, and the bare `<` at 16:9 and
      beyond); with the fix, 15,037 checks pass and ctest is 4/4.
    * **The feel change, measured, is 45% — not the 40% the radius suggests.**
      hostmpe's absolute deadband floor (#16, 0.006 canvas-height) takes a
      proportionally bigger bite out of a smaller knob: 10.5% of R on a
      natural, **17.5%** on an accidental, so an accidental's usable travel
      lands at **55.3%** of a natural's (0.02828 vs 0.05114 canvas-height).
      That governs pressure, the 0xA0 swirl half-axis and stylus CC74; bend
      is untouched (identity beyond the circle, #10). Accepted as honest — a
      smaller key has shorter travel — and the floor is `hostmpe`, not the
      core, if black keys ever feel sticky.
    * Verified on device after reinstalling: the lattice reads as a keyboard
      (small accidental rings nested between the naturals), and the pen
      glissando still plays C6→D6→E6→F6→G6→A6→B6 with **zero accidentals
      grazed** — the hit region was untouched, as intended.
    * **The Mac must NOT re-apply this.** The other agent offered the same
      one-line fix; it is done here, so that side should pull rather than
      patch, or the next merge conflicts on `layouts.cpp` and
      `normalizer_tests.cpp`.

58. **The S-Pen's barrel button is a sustain pedal (user request, Android
    first — iOS has nothing equivalent wired).** Numbered 58 to leave 57 for
    the core piano-grid R_max fix being applied on the Mac. Implementation:
    the button drives the SAME `hostmpe_strip_t` sustain engine as the §8
    palette's pad — not a second CC-64 emitter. Consequences, all of them the
    reason to do it this way: the momentary/toggle setting governs the pen
    exactly as it governs the pad, the pad's display mirror follows what the
    pen did (the overlay calls back into the host, which re-syncs it), the
    message is master-channel by construction, it rides the never-dropped
    class, `hostmpe_strip_announce` re-announces it after every MCM re-sync,
    and a panic clears the pen's pedal along with everything else.
    * **Two event paths, one idempotent setter.** `ACTION_BUTTON_PRESS` /
      `ACTION_BUTTON_RELEASE` (with `actionButton` = `BUTTON_STYLUS_PRIMARY`
      or `_SECONDARY`) is the documented path, and it arrives through
      `onTouchEvent` while the tip is down but through `onGenericMotionEvent`
      or `onHoverEvent` while the pen only hovers — so all three call the same
      handler. On top of that, a `buttonState` transition seen on ANY stylus
      event is honoured, because OEM stacks vary in whether they dispatch the
      explicit actions. `setPenButton` acts only on a CHANGE, so the two paths
      can never double-fire one press.
    * **A pen that leaves the digitizer releases a momentary pedal**
      (`ACTION_HOVER_EXIT` with no pen touching): you cannot hold a pedal with
      a pen that is not near the glass, and a stranded CC 64 is the one thing
      §8 names as unacceptable. In TOGGLE mode the engine ignores the release,
      so a deliberate latch survives the pen being put down — which is what
      makes the toggle setting worth having for the pen.
    * Play mode only, like the strip itself: sustain has no meaning without
      notes, and in Marble mode the overlay is gone.
    * Verified on device through the plumbing: a simulated click (the new
      `--es penButton click|down|up` debug intent) emits CC 64 = 127 then 0 on
      the master channel, tagged as strip traffic, and the byte-log asserts
      pass ("strip traffic on the master channel only", "sustain never
      sticks"). The REAL button press could not be scripted — `sendevent` on
      `/dev/input/event8` is denied to the shell user — so the OEM delivery
      path is the user's one-click check; the pen device does report
      `BTN_STYLUS`, which is what maps to `BUTTON_STYLUS_PRIMARY`.
    * Flagged for the Mac: iOS's sibling is the Pencil Pro squeeze
      (`UIPencilInteraction`, `.squeeze` phase on M2+ iPads with a Pencil
      Pro), which would give the same pedal to the iOS shell. Not implemented
      here; recorded so the parity gap is visible rather than discovered.

59. **ROLLED BACK, IN FULL — the black-key glissando experiment. Recorded so
    the next person does not repeat it.** The user asked for a glissando
    between two accidentals; two attempts were made and BOTH were withdrawn
    by the user, the second with "the touching surface needs to be the same as
    the cells — you break the entire purpose of the app with these random size
    modifications." The tree is back to #41 + #57 and nothing of this entry
    ships.
    * What was measured, and stands as fact: a stylus sweep along the
      accidental row plays the FULL CHROMATIC SCALE — C C# D D# E F F# G G# A
      A# B — because every 0.4-unit gap between two 0.6-wide accidentals is
      the natural's top (#41). A pianist expects the black-key run.
    * Attempt 1 (core): widen the accidental's HIT region so adjacent
      accidentals meet, leaving the drawn knob at 0.6. It worked — the sweep
      played C C# D# E F F# G# A# B — but it decoupled the touch region from
      the drawn cell, which is the thing the user identified as breaking the
      instrument: **the circle you see IS the cell you touch and the joystick
      it generates.** An intermediate 0.1-unit lane was tried first and
      rejected on measurement: a moving pen crosses 35 px in a couple of
      samples, so whether the passing natural sounded depended on hand speed.
    * Attempt 2 (shell): draw the piano grid as keys tiling their row, so the
      lattice would show the contiguity. Withdrawn as a deviation from PHASE4
      §6, which the working rules say wins: a cell is drawn ROUND, at the
      radius of the joystick it generates.
    * **The trilemma, stated for whoever picks this up.** With round cells you
      may have any two of: (a) the touch region equals the drawn circle,
      (b) accidentals visibly smaller than naturals, (c) adjacent accidentals
      contiguous so a black-key glissando works. #41 + #57 chooses (a) + (b).
      The untried third option is (a) + (b) + dead space: let the black row
      refuse OUTSIDE an accidental — touch still equals the drawn circle, and
      because a dead zone SUSTAINS (a pen only retriggers on a probe hit,
      #39) a slide would give a clean pentatonic C# D# F# G# A#. The cost is
      that tapping between two black keys does nothing, and #29's dead zones
      return where #41 removed them. It needs the user's decision, not an
      agent's — which is where this stopped.

60. **PRE-EXISTING BUG, found by the user and fixed: on the piano grid alone,
    a natural's drawn cell hung HALF A ROW below its own touch region.**
    Reported as "the touch squares are not aligned with the cells, they are a
    little bit upper, so the bottom of a cell is dead or is occupied by its
    down neighbour" — an accurate description of the geometry, and the reason
    this layout kept feeling wrong.
    * **Measured before the fix** (aspect 1.60, note 60): the cell is DRAWN at
      y 0.4714..0.5857 (centre 0.5286, r 0.0571) while the probe gives that
      note y 0.4430..0.5570 (centre 0.5000). Half a row (0.0286) of offset.
      The bottom quarter of every natural's circle played the octave BELOW on
      screen, and the playable strip above it was not drawn at all. The chroma
      grid measured 0.0000 offset, which is why this was piano-only.
    * **Cause:** `layout_piano_grid` centred a natural on its own drawn ROW,
      but its playable region is the octave PAIR — its row plus the white-key
      tops above it (#41) — and `cell_radius` has been the PAIR's half-height
      since #29 ("a key's playable footprint is one key wide by one octave
      tall"). Centre and radius were describing two different rectangles.
      Accidentals were always right: their region IS one row, and they are
      centred on it (measured offset 0.0001).
    * **Fix, one line:** a natural's y is the pair's centre (`row`, not
      `row + 0.5`, in 14ths). After: drawn 0.4429..0.5571 against a touch
      region of 0.4430..0.5570. On device, tapping the top edge, the centre
      and the bottom edge of C4's circle now all play C4 — the bottom edge
      used to be the octave below.
    * **Consequence, stated plainly:** the semitone axis follows the geometry
      (generic #7 shortest-neighbour rule), so C→C# is now half a key over and
      HALF a row up rather than a whole row. The piano grid's `semitone_step`
      drops (0.0829 → 0.0665 at aspect 1.0), which means a given drag bends
      ~20% further on THIS layout. That is the corrected geometry rather than
      a tuning choice, and both goldens were updated to it. Naturals' drops
      also land half a row higher — the lattice moves with them, which is the
      alignment being fixed.
    * **The invariant is now pinned, not just the numbers:** the golden scans
      the vertical line through C4's centre, and asserts that the region which
      probes to note 60 has the same centre as the drawn cell and the same
      height as its diameter. A future refactor cannot drift them apart again.
    * **Left alone deliberately:** an accidental's circle is ~10% taller than
      its one-row region (0.0686 vs 0.0567 at this aspect) because its
      radius comes from 0.6 of the pair height (#57). It is centred correctly,
      it reads as a black key overlapping the whites, and shrinking it to the
      row would change #57's clean 0.6-at-every-aspect ratio to 0.5. Not the
      reported bug; flagged rather than touched.

61. **The glissando corridor: a natural shrinks to the accidental's size and
    sits flush with its old BOTTOM, freeing the strip above it for the black
    keys (the user's design, given as an instruction).** This is what #59 was
    reaching for and got wrong twice — the difference is that the cell and the
    region that plays it stay the same thing throughout, which was the
    condition all along.
    * **Geometry.** A natural now owns the octave pair's BOTTOM
      `PIANO_NATURAL_H` = 0.6 rather than all of it; its cell centre is the
      middle of that band, so its bottom edge is exactly where it was and its
      cell still matches its touch region (#60's invariant, re-verified:
      drawn 0.4886..0.5571 against a touch region of 0.4888..0.5570). Both
      cells are now 0.6 of a pair TALL, so where height governs — every
      landscape aspect — the natural and the accidental are the SAME SIZE,
      which is what the user asked for. WIDTH still separates them (0.6 of a
      key against a full one), so on tall screens the accidental is the
      smaller of the two.
    * **The corridor.** The strip above the natural band, off any accidental,
      is deliberately EMPTY. A pen sliding through it sustains rather than
      sounding a white key (a dead zone makes no call, #39), so the black keys
      play as the run a pianist expects. Measured on device: a corridor sweep
      gives **C#4 D#4 F#4 G#4 A#4** — five accidentals, ZERO naturals — where
      the same gesture used to give the full chromatic scale. The natural band
      below is untouched: **C4 D4 E4 F4 G4 A4 B4**, zero accidentals.
    * **This supersedes #41's white-key tops.** The uncovered part of the
      accidental row was the natural's, which is what made that slide
      chromatic. The naturals lose nothing: their own band still tiles
      completely (asserted across 70 sample points), and the E-F / B-C gaps
      and row ends are now part of the corridor.
    * **Known and accepted:** tapping in the corridor off a black key does
      nothing, and a glissando must START on a key — a stroke that begins in
      dead space never allocates a voice, so nothing sounds for its whole
      length. Both follow from "the cell is the touch region"; neither is a
      bug to chase.
    * Goldens moved with the geometry: C4's landmark position, the C→C# axis
      (now 0.9 of a row apart), the natural's R_max (a full key wide by 0.6 of
      a pair), the corridor's emptiness at three x positions, the natural
      band's complete tiling, and the cell-size formula for both kinds at
      seven aspects. 15,117 checks, ctest 4/4.

## Back on iOS (after the Android port)

62. **The Pencil Pro's SQUEEZE is the S-Pen barrel button's twin (user
    request, translating Android #58).** `UIPencilInteraction` on the play
    overlay; `didReceiveSqueeze` (iOS 17.5+) drives the SAME strip sustain
    engine as the panel's pad: `.began` -> `hostmpe_strip_sustain_press`,
    `.ended`/`.cancelled` -> release, then `syncStripMirrors()` so pen and
    palette never disagree. Transition-only (a squeeze's `.changed` stream
    cannot double-fire), Play-mode only (`isHidden` guard), and the sustain
    MODE setting still decides momentary vs latch. Pencil 2 fallback: the
    double-tap (`pencilInteractionDidTap`) LATCHES the pedal, since a tap has
    no hold. One pedal, two platforms, one engine.

63. **The iOS byte log adopts Android's src taxonomy** (0 external, 1 finger,
    2 session config, 3 strip, **4 stylus**): pen bytes were tagged as finger,
    which made `tools/pen_trace.py` blind on iPad logs and `midi_asserts.py`
    report the pen's legitimate CC74 as "finger CC74" (the §3.3 stylus-only
    rule reads as violated). `playTouchEnd` gained `isPen` so a pen's release
    is tagged too. Both platforms' logs now feed the same analysers.

64. **Evidence tooling on iOS: in-app capture + log flush** (settings ->
    Evidence). `startCaptureBurst` writes N full-screen PNGs to Documents on
    a delay (so the sheet can be dismissed and the instrument played) via
    `drawHierarchy(afterScreenUpdates: true)` — the render-server path, the
    only snapshot that can carry CAMetalLayer content; `flushLogsNow` writes
    the byte/latency/session logs mid-session. Everything is pulled with
    `xcrun devicectl device copy from --domain-type appDataContainer
    --domain-identifier com.vibetuned.midi-sink --source Documents`, which is
    how the step-21 device evidence was gathered.

65. **FLAGGED QUESTION (core, unchanged — user's call): a sustain press WIPES
    THE CANVAS in every mode.** `sumi_voice_mapper_normalize` maps any CC 64
    rising edge, on any channel, to `SUMI_VEV_PAPER_DIP` -> a RESET pass.
    That is §2.4's CLASSIC-keyboard mapping ("CC 64 -> paper dip"), but it is
    not gated by mode, so the §8 strip's Sustain pad — and now the Pencil
    squeeze and Android's S-Pen button — dip the paper mid-performance.
    Proven headlessly: MCM (MPE mode) + CC64 = 127 -> PAPER_DIP -> RESET
    queued. The user's own iPad log carries two such presses. Proposed
    one-line gate: honour the dip only when the normalizer is NOT in MPE
    mode (or only outside Play mode), leaving §2.4's keyboard behaviour
    intact. NOT changed here: it is a spec'd mapping and a musical decision,
    and it affects both platforms identically.

66. **Echo suppression: the shell must not consume its own output (user-
    requested after the finding).** Measured on a real iPad session:
    **99.5% of "external" MIDI (14,549 of 14,629 messages) was our own
    output mirrored back**, median round trip 0.3 ms. Consequences, both
    real: `hostmpe_observe_external` marked OUR OWN channels externally
    held — the §5.1 mask can then starve the allocator into silent drops —
    and the loopback painted every note twice (two drops per strike, the
    counter advancing twice). The shared guard lives in hostmpe so both
    shells inherit it: `hostmpe_echo_record` on every byte that actually
    LEAVES the device (iOS hooks `MidiOutputs.emit`, the single delivery
    point — it covers the limiter's drained batches too) and
    `hostmpe_echo_is_ours` at the device input, before the mask, the byte
    log and the loopback. A match inside a 300 ms window is CONSUMED, so a
    device legitimately repeating the same bytes is never swallowed and at
    most one echo is dropped per emission; `hostmpe_echo_dropped` surfaces
    the count in the session status line. Why Android never saw it: its
    sinks are a USB gadget, a MidiDeviceService virtual device and a BLE
    peripheral — none of which iOS's virtual-source/IDAM/network topology
    mirrors back into the app's own input scan (#25/#27 are the same family
    of bridging surprises).

67. **The sustain pedal no longer wipes the canvas (user decision, resolves
    the #65 flagged question).** `CC 64 -> paper dip` is §2.4's CLASSIC-
    keyboard mapping — a plain keyboard has no held-note semantics here, so
    its otherwise-unused pedal dips the paper. In MPE that pedal is a REAL
    musical control (the §8 strip's pad, the Pencil squeeze, the S-Pen
    button), and dipping mid-performance wiped the marbling under the
    player's hands. The mapper now honours the dip only when the input mode
    is not MPE; the classic path is untouched (unit-tested both ways).
    Because the dip is a feature, not just a side effect, iOS gains a
    deliberate **"Paper dip (fresh sheet)"** control in settings — Android
    already has `nativeTriggerDip` and should surface the same button.

68. **FLAGGED (behaviour, unchanged — user's call): with the #40 bend booster
    engaged, a cell CROSSING is no longer pitch-continuous.** Found by running
    `tools/pen_trace.py` over a free-play iPad session: 13 of 115 strokes trip
    the tracer's overshoot assert, and the cause is measurable — **69% of the
    401 crossings carried a bend beyond a half-cell (up to 2.88 st)**, which
    only the multiplier can produce (raw geometry is bounded by the ±0.65 st
    hysteresis). Because the in-cell offset FLIPS SIGN as the reference cell
    changes, a boosted crossing moves the sounding pitch by up to the note
    step plus twice the boosted half-cell — the tracer measured a crossing
    whose note went +2 while the pitch went −2.4. #39's "the retrigger lands
    where the pen already is" therefore holds exactly at scale ×1 and
    degrades as the boost rises. Options if it bothers the player: (a) leave
    it — deep vibrato plus a simultaneous slide is an extreme gesture and the
    jump is arguably the sound of it; (b) suppress crossings while the boost
    is engaged (deep vibrato means holding a note, not gliding) by scaling
    the hysteresis with the boost; (c) emit crossings at scale ×1 and let the
    boost resume after. NOT changed here: the user is playing with the
    booster daily and has not reported a seam. The tracer's other 12 failures
    are its scripted-sweep monotonicity assumption meeting free playing, not
    defects.

---

# Part IV — Phase 5: Packaging, Release, Web & Documentation (steps 23–33, formerly `_work/DECISIONS_4.md`)

Ambiguities resolved during Phase 5 — the desktop harness becoming the
product, the release spine and its five lanes, the WebGPU seam and the marble
web, the documentation site, and the Step-33 feedback batches on every
platform (the scoped unfreeze: core 0.5 → 0.9). Prior history: Parts I–III
above (references written as `DECISIONS_3 #n` mean Part III). Entries here are
referenced elsewhere as `DECISIONS_4 #n`. Where an entry says `PHASE5 §n` it
means the Phase-5 spec as it stood during the phase — now folded into
`PROJECT_SPEC.md` §9 (PHASE5 §n ≈ §9.n); `ROADMAP_4` means `ROADMAP.md`
Part 4. Where these entries and the spec conflict, the later entry is the
record of what shipped. Entries #73–#77 were written on the Linux box while
#78–#80 were written on the Mac (the two machines numbered in parallel; the
Mac's three were renumbered at the fold).

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

55. **iOS shows its MIDI inputs, and rescans at 1 Hz as well as on
    notification.** Feedback: "the iPad sends MIDI but does not receive it
    from USB". The code connects every CoreMIDI source (wired, IDAM host,
    network, Bluetooth) to one input port and pushes every MIDI 1.0 word to
    `sumi_push_midi`, so the failure is somewhere the shell could not show:
    CoreMIDI never listing the device, the connect failing silently, no bytes
    arriving, or the bytes being consumed by the #66 echo filter. Fix,
    host-only: the Settings "MIDI" section gains the desktop's inputs list —
    every connected source by display name, a live "received N messages ·
    last SS D1 D2" counter, a "skipped" count when CoreMIDI lists more sources
    than we connected, and "Rescan now"; every `MIDIPortConnectSource` status
    is logged with the driver owner (the MidiOutputs rule). Hotplug also
    polls at 1 Hz like the desktop harness (DECISIONS #25): CoreMIDI delivers
    setup notifications on the run loop current at the process's FIRST
    `MIDIClientCreate`, and a notification path is a single point of failure
    for a device plugged in after launch. The iPad is a USB HOST: a
    class-compliant controller on the USB-C port appears as a source and needs
    no entitlement; a Mac tethered over USB appears as the IDAM host source,
    and what the Mac sends to its "iPad" port arrives there. Android is not
    symmetric: with the system USB mode flipped to MIDI (the gadget, §5.4(b)
    primary sink) the tablet is a peripheral and cannot host a controller at
    the same time; the peripheral port is bidirectional and `MidiInputs`
    opens it, so a host's stream does reach the canvas. Spec §5.4 iOS list
    ("forwards CoreMIDI packets") is unchanged; not a core change.

56. **The same settings on every platform.** Feedback from both tablets: "we
    cannot change the colour", and other rows missing. PHASE5_SPEC (Step 23)
    named the iOS sheet the contents spec *including* palettes and the CC map
    editor, yet the sheet never had them — the desktop window (#2) and the
    web panel (#21) grew past it. Rule from here: the desktop settings window
    (`desktop/src/settings_ui.cpp`) is the contents spec; every shell shows
    every row where the platform has the concept. Added to iOS and Android:
    **Layout & look** — palette (Sumi black / Indigo / Ochre), viscosity, ink
    feed, paper roughness, and tempo + roll speed on the two piano rolls, the
    desktop's ranges; **Vortex profile** (Exponential / Rankine), which the
    two-finger twist now stirs with too, as the desktop's right drag does
    (both shells had EXPONENTIAL hard-wired; the guide's "Rankine by default"
    line was wrong and is corrected); **Stylus wake** row on Android (the
    iOS row shipped with #53); **Ripple** — amount and wavelength sent as the
    routed CCs (102/103 by default) through the MIDI path exactly like the
    desktop's sliders (iOS: on the serial midiQueue as source 2; Android: a
    `play_send_cc` hop onto the MIDI thread, logged as session config,
    loopback only, and replayed at instance creation because `push_midi`
    drops without an instance), plus the frame angle; **CC map** — the
    desktop's table (any CC, channel or "any", nine dimensions; add, remove,
    restore defaults), persisted as `ch:cc:target;…` in UserDefaults /
    SharedPreferences with "" = the default map (the core's
    `install_default_cc_map` + the 102/103 handles, the desktop's
    `app_settings_default_routes` verbatim); restoring re-maps that list
    explicitly since `sumi_clear_cc_map` does not reinstall the core's map.
    Android's Compose is foundation-only (no Material), so continuous values
    are «‹ value ›» step rows at the desktop sliders' useful grain rather
    than sliders. Deliberately NOT mirrored: the web panel's CC map (#21:
    Web MIDI hands the browser its ports) and Android's full-resolution
    toggle (the thermal listener owns sim_scale, DECISIONS #31). Host-only;
    the core is untouched. Android is written, not compiled here (handoff).

57. **`--window <w>x<h>` is a public desktop flag.** Support needs to
    reproduce reports "only at this resolution"; the window was hard-coded to
    1280×720 and no lab-bench flag changed it. Accepted without `--dev` (a
    user is asked to type it, like `--version`), in screen points, clamped to
    320×240 … 16384×16384; the window stays resizable and the framebuffer is
    scale × size on HiDPI. Host-only.

58. **Desktop gets "Hide the title bar" and "Fullscreen" (Settings › Window),
    with `--no-titlebar` / `--fullscreen` flags and the platform fullscreen
    chord.** The canvas is a GLFW window, so its title bar is GLFW's
    decoration — there is no SwiftUI WindowGroup or hand-built NSWindow to
    style. Two mechanisms, chosen per platform for what a user means by "no
    title bar": macOS gets SwiftUI's `.hiddenTitleBar` look through the
    existing Cocoa glue (`NSWindowStyleMaskFullSizeContentView` +
    transparent, title-less title bar: traffic lights, top-strip dragging
    and edge resizing all survive — `metal_layer_glue.mm` is the one
    Objective-C file and already holds the NSWindow); Windows and Linux flip
    `GLFW_DECORATED` at runtime (borderless, so no frame to drag or size by —
    `--window` and fullscreen cover placement, and Ctrl , still opens the
    settings, which is why the settings window keeps its own frame).
    Fullscreen is `glfwSetWindowMonitor` on the monitor holding the canvas's
    centre (work-area test), at that monitor's current video mode, with the
    windowed geometry remembered and restored on the way back; the
    framebuffer callback resizes the core as for any resize, so the field
    survives the switch. Chord: Control+Command+F on macOS, F11 elsewhere —
    the second key binding a release build keeps beside the settings chord.
    Both flags SET the persisted settings (a user who launches `--fullscreen`
    once expects the next launch to match the checkbox they see). Verified on
    the Mac: `--fullscreen` fills the LG ULTRAGEAR+ at 3008×1269 with the
    windowed 1280×720 restored on exit; `--no-titlebar` persists and
    reloads. Host-only; the core is untouched.

59. **No title bar is the default, not a setting.** Supersedes the "Hide the
    title bar" checkbox, INI key and `--no-titlebar` flag of #58 (user: "the
    no titlebar should be the default not an option"). The canvas always
    opens without one: macOS through the #58 Cocoa path at window creation
    (edge to edge, traffic lights kept, drags by its top strip, resizes at
    its edges); Windows and Linux with `GLFW_DECORATED` off as a creation
    hint — a borderless window with no frame to drag or size by, moved with
    the Win / Super + arrow keys and sized with `--window` or fullscreen.
    The settings window keeps its frame, and Ctrl , / ⌘ , still opens it, so
    nothing becomes unreachable. Fullscreen (#58) stays a setting with its
    flag and chord. An old `hide_titlebar=` key in settings.ini is ignored.
    Flagged for the Windows and Linux verification: if a borderless canvas
    turns out unmanageable on a desktop there, the fallback is a per-platform
    default, not a setting.

60. **The input dialect is a setting — MPE by default — and MPE / Wind
    each absorb what made people ask for the other two.** Feedback: the
    §2.5 auto-detection should be a parameter ("MPE as default, classic or
    wind as option"), users see no difference in Classic, and Wind should
    accept MPE because IMU-equipped wind controllers add an expression
    layer. Shipped, all four shells: an **Input** row (MPE · Classic
    keyboard · Wind), persisted (INI `input_mode`, localStorage, UserDefaults,
    SharedPreferences), applied with `sumi_set_input_mode`; no shell ever
    selects `SUMI_INPUT_AUTO` — the heuristic stays in the core for the ABI
    and its tests, unused. Two mapper changes (core, Step 33 unfreeze; ABI
    unchanged, 0.7.1) make the default livable: (a) in MPE mode a note on a
    NON-member channel — the master, or a plain keyboard sharing the bath —
    is a classic per-(channel, note) voice, so chords on channel 1 no longer
    collapse onto one channel voice; member channels keep newest-steals
    identity, the master bend stays the global shear and CC 64 stays musical
    (#67 of Part III). Classic therefore remains for two things only: a
    keyboard sending on a channel inside the member zone (2–16), and the
    sustain-pedal paper dip. (b) Wind mode reads the expression layer on its
    single brush (slot 0): CC 74 → slide, poly pressure → swirl, and a bend
    on a MEMBER channel (an MPE wind controller — Sylphyo-class) → glide with
    the ±48 member range; a single-channel bend stays the global shear;
    breath (CC 2/7/11 or channel pressure) keeps owning the width, and
    legato keeps migrating. `member` is now `(mpe || wind) && in_zone`.
    Tests: `test_mpe_master_channel_keyboard`, `test_wind_expression_layer`
    (ctest 4/4). Spec §2.5 ("auto-detection ... overridable") is stale: the
    default is the MPE setting; §2.3/§2.4 gain the layer described here.

61. **Palette morph travels the whole ring.** Feedback: "black to blue only?
    or can we get ochre?" The composite blended the active palette toward
    the next only, so CC 31 from Sumi reached Indigo and never Ochre. Now
    morph 0..1 covers two transitions from the active palette: 0 = active,
    ½ = the next, 1 = the third (Sumi → Indigo → Ochre; from Indigo: Indigo →
    Ochre → Sumi). Monotonic (a full sweep ends on the third palette rather
    than returning home), every palette reachable from one controller, the
    palette picker still chooses where the ring starts. Shader-only
    (`composite.glsl`); the §4.6 field fixtures are deformation fields and
    are untouched. Core 0.7.1 together with #60.

62. **CC 64 never dips the paper.** Feedback: "remove the paper dip CC 64,
    it does not make sense there." Supersedes §2.4 / DECISIONS_3 #67's
    classic-only mapping: the sustain pedal is the synth's in every input
    mode, and a fresh sheet is only ever the host's deliberate action
    (`sumi_trigger_paper_dip`, the settings' Paper dip). In the mapper CC 64
    now falls through to the CC map like any controller (unmapped by
    default, routable). `SUMI_VEV_PAPER_DIP` stays as the ABI path's event;
    the tests that used CC 64 to reach it build the event directly
    (`test_sustain_never_dips` covers all three modes). Spec §2.4 and §3.3
    ("CC64 rising edge (classic mode only)") are stale.

63. **Wind mode is MPE plus a wake between notes; the wandering brush is
    retired.** Feedback: the brush "is not visually pleasant … breaks with
    long plays", wind should "work exactly as the MPE mode but with a wake
    from the previous note to the new one", using only operators, and the
    breath drops were too weak. Now: one voice (slot 0 whatever the channel),
    every note a strike drop of the MPE radius; breath (CC 2/7/11 via the
    INK_FLOW route, or channel pressure) is the UNBOUNDED §4.4 feed like MPE
    press (the width clamp `WIND_WIDTH_*`, the thin touch-down and the
    migrate tine are gone); channel pressure honours `press_mode` as MPE
    does. On a legato change the mapper emits a `VOICE_MIGRATE` — now "wake
    the voice's drop to (x, y)": the §4.3.4 wake with the drop's current
    radius as the rigid tip (floor 0.006, the tablets' lightest pen), profile
    and spread from `wake_profile` / `wake_spread`, sub-stepped ≤ a/4 and
    capped at 256 steps, mirroring `sumi_add_wake`; the event's ax/ay carry
    the aspect-corrected displacement because the lowering has no aspect and
    every echo of a note moves alike — then a silent `VOICE_END` and the
    new note's `VOICE_BEGIN`. So a phrase is a chain of drops threaded by
    their wakes: nothing but operators, no per-mode geometry. Tests:
    `test_wind_mode_wake_legato` (radius, ≤ a/4 sub-steps summing to the
    move, tip = old radius, new band, unbounded growth past 0.056,
    press_mode), `test_wind_expression_layer` updated. Core 0.7.2 (ABI
    unchanged). Spec §2.3 ("wandering ink brush", "width") and §4.4 ("Wind
    mode is the exception to unbounded growth") are stale; the operators'
    index lists the wind legato under Wake, not Tine.

64. **Two more piano rolls: from the right and from the bottom.** User
    request. `sumi_layout_t` grows additively — `SUMI_LAYOUT_ROLL_H_RIGHT = 6`
    (pitch → y, now-line at x = 0.88, the sheet drifts −x) and
    `SUMI_LAYOUT_ROLL_V_BOTTOM = 7` (pitch → x, now-line at y = 0.88, the
    sheet rises) — the mirrors of 3 and 4, same inset, same speed, the field
    motion always away from the now-line; the shader's ingress branch is
    direction-free so nothing else changes. The semitone axis comes from the
    generic neighbour rule. Names everywhere become "Piano roll (left / top /
    right / bottom)" — the now-line's edge, which is what a performer asks
    for. ABI 0.8.0 (enum addition only; the params struct is unchanged, so
    0.7 hosts load fine and the core falls back to fifths for ids it does not
    know, as before). Golden positions + a drift-direction test for all four.
    Spec §3.4's layout list is stale (six → eight).

65. **"(playable)" on the tablets' layout names.** Chromatic grid, Jankó and
    Piano grid read "(playable)" in the iOS and Android pickers — the three
    lattices Play mode accepts, which the Mode row's footnote used to be the
    only hint of. Not on desktop or the web: they have no Play mode, and the
    label would promise one. The guide's layouts page already marks them.

66. **The bend-driven ripple is four times more sensitive.** Feedback: the
    ripple "is too weak with the MPE input; 3 to 4 times stronger". The v0.4
    law saturated the amplitude at |±6| semitones, so a real ±0.5-semitone
    vibrato breathed ~8%. Now |±1.5| saturates (a ±0.5 vibrato breathes a
    third); the ripple's ceiling (`SUMI_RIPPLE_AMP_MAX`, shared with the
    CC 102 slider) is unchanged, so this is the MPE path only. The stilling
    property (A → 0 when the note re-centres) is a group property and
    survives; `--ripple-group-test` / `--ripple-permanence-test` unaffected.


## Step 33 — Windows verification

67. **The Windows canvas keeps its title bar — #59's per-platform fallback,
    taken with the concrete failure it asked for.** Measured on the Windows
    11 box: a `GLFW_DECORATED`-off window carries neither `WS_CAPTION` (no
    mouse drag, no Alt+Space → Move) nor `WS_THICKFRAME`, and **Windows Snap
    requires the latter — Win + arrow keys do NOTHING** (verified with a
    framed control window snapping fine in the same session), so #59's "moved
    with the Win + arrow keys" assumption does not hold on Windows. What
    still worked: Win+Shift+arrows (whole-monitor hops), taskbar
    minimize/restore, Alt+F4, Ctrl , and fullscreen — but a window that can
    never be freely placed or snapped is unmanageable on a desktop. The
    creation hint is now Linux-only; Windows creates the canvas decorated
    (drag, snap, Alt+Space, minimize buttons all back — Win+Right verified
    snapping after the change). macOS (#59's Cocoa path) and Linux are
    untouched; README and guide/desktop.md state the difference. A borderless-
    with-snap window is possible on Win32 (keep WS_THICKFRAME, subclass the
    wndproc for WM_NCCALCSIZE) but is a hack UNDER GLFW's own style tracking —
    declined for a settings-window product; revisit only if the author wants
    the borderless look back on Windows.

68. **Layouts 6 and 7 did not survive an INI reload — the settings loader
    still clamped `layout % 6` (fixed to `% 8`).** Found by the Step-33
    checklist item 5: `layout=6` (Piano roll, right) reloaded as 0 (fifths)
    and `layout=7` (bottom) as 1 (chromatic grid) on EVERY desktop platform —
    batch 10 (#64) grew the picker and the core enum but missed the one
    modulus in `app_settings.cpp`. The Mac verified 6/7 in-session through
    the picker, which never crosses the loader; a restart was the missing
    test. Shell-only, one constant; after the fix both rolls reload and
    their drift away from the now-line is the Step-33 Windows evidence
    (`roll_layout_6/7.png`). The macOS and Linux lanes inherit the fix on
    their next pull — nothing platform-specific in it.

69. **The Airwave map is symmetric hands, by the author's direction after
    playing it ("they are hard to use"): each hand stirs its own water, ABI
    0.9.0.** The #50 layout scattered unrelated dimensions across the hands;
    the author's redesign makes both hands the same instrument — **Raise =
    strength, Glide = centre X, Slide = centre Y** — the left driving the
    exponential/Rankine vortex, the right a new **Lamb-Oseen swirl** control
    (`SUMI_CTL_SWIRL_STRENGTH/X/Y`: the same dt-scaled agitation as the
    vortex, emitting the §4.3(7) swirl pass at the #49 gesture's full-pull
    rate, core r_c 0.15). **Both centre-Y dims are REVERSED at emit** (CC up
    = up on screen — texture y grows down, and a raised hand lowering the
    stir read wrong). **Grasp = the pinch** (`SUMI_CTL_PINCH_SADDLE` left at
    the vortex centre, `SUMI_CTL_PINCH_CROSS` right at the swirl centre),
    delta-driven exactly like the CC 74 route so a squeeze-and-release nets
    out and the un-retraced residue bakes in. **Tilt = the ripple**
    (wavelength L 28, amount R 29). **Flex 30/31 is deliberately FREE** — the
    author: it cannot be played without activating the others. Viscosity,
    roughness and palette morph lose their Airwave routes (settings sliders;
    the editor rebinds them). sumi_ctl_t grew additively (9-13, COUNT 14) →
    **0.9.0**; new mapper test (`test_global_ctl_swirl_and_pinches`: swirl
    trio with reversed Y, delta-in/delta-out pinch, crossed pair) plus the
    updated goldens; suites 18 455 checks green, §4.6 field gate bit-stable
    (ctl dims never enter the fixture script). Shell mirrors, README,
    devices/settings pages and the chart JSON updated (chart_check 32/32);
    iOS/Android name tables + defaults updated but NOT compiled on this box
    (the standing handoff pattern). Swirl/pinch rates are first-cut constants
    (SWIRL_CTL_RATE 3 rad/s, PINCH_K_SCALE shared with CC 74) — tune on the
    author's report.

70. **The canvas keeps its title bar on every platform.** Supersedes #59's
    borderless default and #67's Windows-only fallback. After the Windows
    verification the author settled it for all three desktops: people want to
    move the window around, and Fullscreen (#58: Settings › Window, F11 /
    ⌃⌘F, `--fullscreen`) is what they use for the display. The macOS
    hidden-title-bar call and the Linux `GLFW_DECORATED` hint are gone; the
    Cocoa glue `sumi_macos_set_titlebar_hidden` stays in
    `metal_layer_glue.mm`, unused, should a per-platform look ever come back.
    The Linux handoff no longer asks the box to judge a borderless canvas.
    Host-only; README, guide/desktop.md and the settings reference say so.

71. **A persisted CC map that is the stock map of an older version follows
    the redesign.** The desktop INI, iOS UserDefaults and Android
    SharedPreferences persist the CC map whole, so #69's symmetric-hands
    layout never reached an installation that had run before it — the
    Windows evidence INI (`settings_ini_after_tests.ini`) still carried the
    #50 routes after the change, and so did the author's Mac. Fix on all
    three shells: on load, a map equal AS A SET to one of the earlier default
    maps (the pre-#50 imagined numbering; the #50 measured layout) is
    replaced by today's defaults; anything that differs is the user's and is
    kept untouched. The INI additionally gains `ccmap_version=3` (absent
    reads as 1) so a future redesign has a cheap gate; the tablets compare
    sets on every decode, which is what "" already meant. Logged on the
    desktop as `[settings] CC map was the stock map of an older version -
    upgraded …`. Android written, not compiled here.

72. **`release.sh` accepts a single-architecture bundle for a `*-local` dry
    run.** The README's documented local command failed on the author's Mac
    with "not universal — missing x86_64": the plain `build/` is arm64 only,
    and the script hard-required both slices. The requirement is right for a
    release DMG (the lane configures `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`),
    wrong for the dry run whose whole point is proving the DMG mechanics on
    any Mac. Now: universal → `…-macos-universal.dmg` as before; a `*-local`
    version with one slice → a warning and `…-macos-<arch>.dmg`; any other
    version without both slices → the error, with the configure line to fix
    it. Nothing in the lane changes.


## Step 33 — Linux verification

73. **Leaving fullscreen on X11 re-asserts the windowed geometry for a
    second — mutter hands it back one title bar lower and shorter.**
    Checklist item 2 measured, on Ubuntu 25.10's GNOME (Xwayland session
    of the canvas): F11 filled the 5120×2160 monitor holding the canvas and
    the INI toggled `fullscreen=` live, but `glfwSetWindowMonitor(nullptr,
    x, y, w, h)` brought a 1280×720 canvas at y=755 back as **1280×683 at
    y=792** (through 1280×757 and 1280×720 on the way — the WM re-applies
    its 37 px frame extents to the client geometry GLFW asks for, then to
    its own result). macOS and Windows restore exactly (Step 33 evidence);
    the mechanism in #58 is right, the platform disagrees. Shell fix in
    `desktop/src/main.cpp` (`settle_window_geometry`): after leaving
    fullscreen, for one second, whenever the window's size or (X11 only —
    Wayland has no positions) position differs from the remembered client
    geometry it is set again; verified twice in a row, exact restore, no
    fight with the WM once it agrees. Not a core change. Wayland's F11
    round trip could not be driven from a script (no input injection
    reaches a Wayland surface); the flag path (`--fullscreen`) resizes the
    core to the monitor there, and the size-only settle applies.

## Step 33 — Android verification (Linux box)

74. **The Android native library is linked for 16 KB pages
    (`-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`).** The first run on a
    Pixel 9 Pro (Android 17) opened a system warning: "not compatible with
    16 KB pages — `lib/arm64-v8a/libsumi-shell.so`: LOAD segment not
    aligned". NDK r27 still emits 4 KB-aligned LOAD segments unless the
    toolchain switch is set (r28+ defaults to 16 KB); `readelf -lW`
    confirmed `0x1000` before and `0x4000` after, and the warning is gone.
    Google Play requires 16 KB support for anything targeting Android 15+
    since November 2025, so this is a release blocker fixed, not a
    polish. One line in `app/build.gradle.kts` (the CMake arguments); the
    Compose runtime's `libandroidx.graphics.path.so` was already aligned.
    AGP 9.x packages the libraries uncompressed and page-aligned on its
    own. Not a core change.

75. **The Android control strip is a setting, hidden by default on phones,
    shown by default on tablets.** Author's request on the Pixel 9 Pro: the
    floating strip (300×86 dp, top-left) covers a fifth of a phone's lattice
    in Play mode, "not like tablets". `showStrip` (SharedPreferences) defaults
    to `smallestScreenWidthDp >= 600`; the CONTROL STRIP section gains "Show
    the control strip" above the sustain-latch row, and the strip's
    `AndroidView` is GONE unless Play mode is effective AND the setting is
    on. Hidden, the S-Pen barrel button still holds the pedal (the strip's
    sustain engine runs whether the view is attached or not) and the
    wheels' CCs keep their last values — the strip is a display of state,
    not its owner. Verified on the phone: chromatic grid in Play mode comes
    up without the strip; the toggle shows it (Pitch / Mod / CC 23 / CC 24 /
    Sus) and hides it again. iOS has no phone target, nothing to mirror.

76. **Wayland: the canvas and the settings window overlap at launch, and
    that is accepted (author's call, option b).** GLFW cannot position or
    focus a Wayland toplevel, so GNOME centres both windows and the settings
    window — created second — lands over the middle of the canvas; the
    author's "clicks do nothing" on the Linux box were clicks landing in the
    settings window (`docs/evidence/step33`, Wayland section: the injected
    click moved the Ripple *Amount* slider, the `--dev` mouse log saw no
    canvas button). The alternative — re-mapping the canvas after launch so
    it is stacked last — would hide the settings window behind the canvas
    instead, with no way to raise it from the app (Wayland refuses focus
    requests as well). Left as is: a click on the exposed canvas raises it,
    the settings window is one Alt+Tab away, X11/macOS/Windows keep the
    side-by-side placement. `--dev` now logs every canvas mouse button.

77. **Android re-sends the persisted ripple sliders at startup.** #56's
    design: the ripple amount/wavelength ride the routed CCs through the
    MIDI path, Kotlin sends them when the rows move, and the JNI replays
    the last value per CC when the core instance comes up (`cc_replay`),
    because `push_midi` drops without an instance. The replay list is
    filled only by what Kotlin SENT in the running process — and
    `onCreate` never sent the persisted values, so every cold start showed
    the sheet's numbers with none of them in the water (the Pixel's
    `midi_log.csv` after a relaunch had no CC 29/28 as source 2; the
    Android Step-33 handoff line "everything persists and the ripple
    values are re-sent" was the check that caught it). One line:
    `sendRipple()` after `nativeSetCcMap` in `onCreate`, before the surface
    exists, so the values land in `cc_replay` and go out with the session
    config at instance creation. The desktop has no such gap (its INI
    values are applied by `app_settings_apply` on the live instance); iOS
    is the Mac session's to re-check for the same pattern. Also from this
    session, at the author's request: the CANVAS section (the two paper-dip
    buttons) leads the Android sheet — "the most used feature" — with
    LAYOUT & LOOK second; iOS keeps its order until its owner mirrors it.

78. **Canvas (paper dip, save print) is the first settings section on every
    platform.** The author, closing the app's Step 33 list: the dip is the
    control a performer reaches for most, so it heads the desktop window, the
    web panel and the iOS sheet; Android already had it first (the Linux box,
    same reasoning). Order only; nothing else moves.

79. **The web's icons are the desktop's rounded square.** The docs site's
    favicon, touch icon, header logo and social card, and the marble page
    (which had no icon at all), were the plain square of `images/midi-sink.jpg`
    while every other platform shows the softened corners — the user: "round
    this, we should do the same for the web". `tools/gen_icons.py` gains a
    `site` target: favicon-32/180 and the 512 logo with the desktop's
    `CORNER_RADIUS_FRACTION` and alpha corners, `web/site/favicon-180.png`
    linked from the marble page's head, and `og.png` at the 1200×630 ratio
    share previews expect with the rounded art composited on the washi cream
    (previews drop alpha to black, so the card cannot keep transparent
    corners). Regenerable with `--only site`.

80. **The gallery lists the five real performances.** Author-supplied captions
    replace the three "recording pending" placeholders: Everything In Its
    Right Place (Windows D3D11 + ROLI Piano + Airwave, Airwave Player Pedal
    Board Rhodes, piano grid), Autumn Leaves (Android + Travel Sax + joystick,
    GarageBand saxophone, chromatic grid, wind mode), La Guaracha (iPad,
    GarageBand harp tuned as an arpa chiquitana, Jankó), Canon in D (iPad,
    Bösendorfer 280 SF2, piano roll), Ali Paşa (iPad, EMRE bağlama SF2, circle
    of fifths — the Jaffer tribute). The manifest gains `synth` (a tag) and
    `based` (label + URL, rendered "Based on …" — the piece each performance
    follows). All five embed their videos (the author supplied the three
    remaining ids in order: Autumn Leaves CJXT1IwcI-A, La Guaracha
    wD_wSZv09-s, Canon in D DWGBWi4C98o). Ali Paşa keeps its "Jaffer
    tribute" title and note — the Latte animation's tune, the one Jaffer set
    a single stylus to — and the gallery page's tribute section.
