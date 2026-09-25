# DECISIONS

Implementation decisions and resolved ambiguities, in five parts:
**Part I** covers the v1 build-out (spec v1, roadmap steps 1–7);
**Part II** covers spec v2 (steps 8–14); **Part III** covers Phase 4 — the
Touch & Stylus MPE Play Surface (steps 15–22, v0.3 → v0.4); **Part IV**
covers Phase 5 — packaging, release, web and documentation, and the Step-33
feedback batches (steps 23–33, core 0.5 → 0.9); **Part V** covers Phase 6 —
the Medium: the Anod operators, the one ABI break (core 1.0.0 → 1.1.0), the
Anod composite, palettes, presets and prints, every shell, and the other
GPU backends (steps 35–46). Entry numbering restarts in
each part — references of the form `DECISIONS.md #n` (code comments,
evidence, git history) mean Part I, `DECISIONS_2 #n` / `DECISIONS_2.md #n`
mean Part II, `DECISIONS_3 #n` / `DECISIONS_3.md #n` mean Part III,
`DECISIONS_5 #n` / `DECISIONS_5.md #n` mean Part V, and
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

81. **The version is injected under bash on every runner, and CMake refuses
    a mangled one.** The first `v1.0.0` run failed at the Windows gate's
    "About must read the tag" check: the binary printed `midi-sink 1`, and the
    configure log shows CMake saw `SUMI_APP_VERSION=1` (`plist 0.0.0`). The
    step ran under the Windows runner's default shell, PowerShell, with
    `-DSUMI_APP_VERSION=1.0.0` unquoted; every release-candidate tag before
    it (`0.5.0-rc.N`, a `-rc.N` suffix) had passed the same step untouched, so
    the mangling bites exactly on a bare `X.Y.Z` — the shape of every real
    release. Two fixes: the configure steps of the gates matrix and the
    Windows lane run under `shell: bash` with the version quoted from an
    environment variable (the other lanes already run bash), and the root
    CMake fails configure when an injected `SUMI_APP_VERSION` is not
    `vX.Y.Z[-pre]`, printing the value it received — the failure moves from
    the version check after a full build to the first line of configure.
    No code change; the tag has to be re-cut after the workflow fix (a
    release is the promotion of a build that passed every gate).

82. **The documentation site deploys from `main`; the web app on it from the
    newest stable tag.** Post-1.0 the author asked to "untangle the docs from
    the release and update the docs with the main", so the store links — the
    App Store and Google Play listings exist only now that they are public —
    land on the live site without a tag. #22 refused a push-to-main deploy
    because it "would publish a site whose `/marble/` and version line
    disagree with the released artifacts"; that objection is met by pinning
    both to the tag rather than to the push. `pages.yml` is the ONLY workflow
    that deploys Pages and composes one tree: `/` from the checked-out main
    (`SITE_VERSION` = the newest stable tag, so the footer still names the
    release users can install); `/marble/` REBUILT from the newest
    `vX.Y.Z` tag (`git -c versionsort.suffix=- tag --sort=-v:refname`, so a
    release candidate never outranks its release) with the release web
    lane's emsdk pin and the quoted `SUMI_APP_VERSION` of #81, cached per
    tag with `actions/cache` so a docs push costs no wasm build — rebuilt
    rather than unpacked from the release asset because the token of a push
    workflow cannot see a DRAFT release's assets and `v1.0.0` sat unpublished
    for eleven days while its web build was live; `/marble/rc/` from the
    newest pre-release tag while one is newer than the stable tag, so RC web
    builds stay previewable without replacing `/marble/` (the Phase-5 tag
    deploy DID replace it — the `rc` apt suite's discipline now applies to
    the web too); `/apt/` from every published release, the whole of
    `publish-apt.yml` moved here and that file deleted — one deployer, or
    two workflows overwrite each other's tree. Triggers: a push to main that
    touches `site/` or the notes it renders; `workflow_run` on a successful
    `release` run of a tag (a new tag reaches `/marble/` the moment its gates
    and lanes are green — dry runs and failed runs change nothing);
    `release: published`; `workflow_dispatch`. The release `web` lane keeps
    only the `dist-web` asset and the spine loses its `pages`/`id-token`
    permissions. Flagged against PHASE5 §9.5 ("built by the same release
    workflow") and #22's deploy path, which this supersedes; the frozen URLs
    of #22 are untouched, and `/marble/rc/` is a new one. Consequence the
    author accepted by asking: a guide page merged to main ahead of the tag
    it describes disagrees with the demos and the downloads until that tag
    exists. Found while doing it: the `v1.0.0` GitHub release is still a
    DRAFT, so `publish-cask`, `publish-winget` and the apt deploy have never
    fired — the tap has no cask, winget-pkgs has no manifest and
    `/apt/midi-sink.asc` is a 404 while the README and the install page
    advertise all three; publishing the release is the human act that opens
    them (Step 34).


# Part V — Phase 6: Medium (steps 35–46 — 44a iOS, 44b web, 45a Linux, 45b Android, 46 Windows; formerly `_work/DECISIONS_5.md`)

Ambiguities resolved during Phase 6 (the Anod operators, the one ABI break,
the Anod medium, palettes / substrate / presets / prints, the four shells,
the three other GPU backends, and the fixes the author asked for after).
References written as `DECISIONS_5 #n` mean this part. The specs of the phase
(`MEDIUM §n`, `QOL §n`, `INSTRUMENT §n`, `SOUND §n`) are the `specs/` set as
it stood; `MEDIUM_SPEC.md` and `CONTEXT.md` were removed at the phase close
and their content, corrected to what shipped, drafted for `PROJECT_SPEC.md`
in `specs/TO_PROJECT_SPEC.md` (#89) — where an entry here and that spec
disagreed, the entry is the record of what shipped.

## Step 35 — Phase opening & the conservation gate (macOS)

1. **The medium is named Anod.** The author's decision of 2026-09-21, closing
   the `MEDIUM §1` iterate point (candidates were Anod, Biri, Ichifu).
   Ichisuke Fujioka is honoured in the documentation's acknowledgments
   regardless of the name — an homage sits better there than carried by a
   product name (the spec's own suggestion). The ABI value is
   `SUMI_MEDIUM_ANOD = 1`, landing in step 41.

2. **Four phases, one public release, version 2.0.0.** The `specs/` set is
   four programs (medium, sound, instruments, publish) and the author split
   them into Phases 6–9 in that order; only Phase 9 releases to the public
   channels. Each earlier phase ends with a pre-release tag
   (`v2.0.0-alpha.N` — the spine already accepts any `X.Y.Z-pre`, drafts a
   pre-release and keeps the lanes proven) that the author installs on every
   device; Phase 9 runs `v2.0.0-rc.N`. Nothing reaches a stable channel
   before step 66.

3. **The ABI break is one step (41) and carries the probe's state argument
   two phases early.** `sumi_layout_probe` gains INSTRUMENT §1's
   `const sumi_layout_state_t*` and `sumi_cell_info_t` gains `flags` in the
   same bump that adds `medium` and `sumi_set_palette`, although no stateful
   layout uses them until Phase 8 (shells pass zeros). One break, then
   additive growth only — `libsumi` is **1.0.0** from that step, the settled
   ABI the deferred SDK needs.

4. **Operator families for the Anod set.** Wave torsion is a third vortex
   PROFILE in the core (the vortex passes already rotate by a profile of r)
   with its own page in the operator book — the `MEDIUM §2.1` iterate point
   closed as "profile in the core, standalone in the book". Chladni and the
   spark shear are passes of the ripple's shear family at both insertion
   points (live and bake). The burst lives in the wake's sub-stepped family
   (`core/src/displacement.cpp`).

5. **Fingering CCs (Phase 8, decided now so the chart and the specs can
   settle).** Valves CC 110 / 111 / 112 (≥ 64 = pressed) on the MASTER
   channel — global state, recorded where a DAW records the mod wheel. The
   slide is 7-bit **CC 113 with normalizer smoothing**, not a 14-bit pair:
   the pair's MSB would land in CC 0–31, the Airwave's block (DECISIONS_4
   #50), and 128 steps over six semitones is ≈ 4.7 cents per step, under the
   ~5-cent just-noticeable difference — the arithmetic is the justification.
   CC 102–119 are undefined in the MIDI specification and already carry
   midi-sink's own controls (102/103 ripple).

6. **Presets share one host-side serializer** (pure C, beside `hostmpe`, step
   43), so the four shells cannot drift; the core stays stateless about
   files.

7. **Per-device default presets are OFFERED, never auto-applied** (`QOL §6`
   iterate point).

8. **Replay files record gestures too** (`QOL §5` iterate point: yes), so pen
   performances replay complete, and they carry FRAME BOUNDARIES: playback
   drives the scripted clock through them (step 61). Re-bucketing recorded
   bytes by wall time is the documented anti-pattern and the negative test.

9. **New-feature documentation lands in step 63 and merges with the release
   tag.** The live `/marble/` is the newest stable tag (DECISIONS_4 #82), so
   an operator page merged early would embed a scene the released wasm does
   not know. Guide fixes ship from `main` at any time; pages for new
   operators, layouts and Voxo are drafted in the step's evidence folder.

10. **The palette curve is fixed per medium in 2.0** (`QOL §1`: the
    "advanced fold" is deferred); **the theremin is a `flags` bit** on the
    cell info, not a radius sentinel (`INSTRUMENT §5`).

11. **The fretboard generalises to `SUMI_LAYOUT_STRINGS`** with three FIXED
    tuning presets — standard guitar, a whole-tone tap grid, all-fourths — as
    a params enum over fixed arrays; user-editable tunings stay deferred with
    microtonal. Wicki–Hayden stays its own layout (not a string layout).
    **"Harpejji" is Marcodi's trademark:** the docs may say "inspired by
    tapping instruments such as the Harpejji"; the word never enters an enum,
    a setting label or a product name.

12. **The phase invariant.** `tests/fixtures/field_512_metal.bin` stays
    bitwise on Metal from step 35 to step 66: new operators add passes, media
    change the composite, layouts change the probe — none touches an existing
    pass. A step that believes it must change the fixture stops and records
    the decision first.

13. **The four-part gate is one `--dev` command per operator, and its (b)
    is measured per class.** `midi-sink --dev --soak <operator|all>` (in
    `desktop/src/dev_tools.cpp`, beside the step-19 pinch soak it generalises)
    drives the nine v1.0 passes through their REAL routes on the fixed 512²
    scripted clock — the glide tine by note bend, the two pinches by CC 74
    under `slide_mode = 1`, the wakes as stylus segments of ≤ a/4 per frame,
    the ripple bake by its amplitude CC, the swirl by channel pressure under
    `press_mode = 1`, the two vortex profiles by the mod wheel — with one
    stream shape for all (a 0.5 Hz wobble at 120 Hz, at most one pass per
    frame) and prints four verdicts plus a machine-readable `SUMMARY` line
    that `tools/soak_report.py` tabulates. **(a)** is the class declaration
    (`MEDIUM §2`) with its one-line det J = 1 argument. **(b) for EXACT
    operators:** 500 strong (+k, −k) gesture pairs at one centre hold ink
    mass (Σ phase) within **±5%** AND return the pre-image to within **4
    texels** mean displacement — mass is the coarse guard (a broken pair
    loses everything: the negative control reads −99.7%) and the pre-image
    return is the sharp one, the observable that tells a non-inverting pair
    from two different area-preserving passes (the negative control leaves it
    204 texels away; the v1 references sit at 1.5–2.7). **(b) for SUB-STEPPED fields:** the class
    promises first-order area preservation per step, so that is measured —
    one a/4 sub-step on an identity field, pre-image Jacobian det > 0.5
    everywhere outside the swept capsule and mean within 2·10⁻³ of 1 (the
    Stokeslet test's own bar, DECISIONS_4 #53; the capsule is excluded as in
    the flick test because the doublet's slip surface is a genuine tangential
    discontinuity, not a fold — measured across it the doublet reads −1.44,
    outside it 0.73). The ±D pair drift is printed beside it, not gated:
    one-sub-step pairs already lose 4.7% over 500 pairs because every step
    resamples the slip surface, and a long stroke per pair would be forty
    passes against the exact operators' two. **(c)** the stream never grows
    mass by more than 0.5%. **(d)** per-pass mass loss ≤ 2× the glide-tine
    control under the identical stream shape, from a FRESH copy of the same
    scene (erosion is front-loaded — sharp structure fades fastest — so a
    control run on the field the operator already worked would inflate the
    baseline), gated only at ≥ 3000 passes (default 6000, the #33 window)
    because of #15 below; a shorter run prints (d) as information.

14. **The (c) negative control uses the v1 tine's clamp legacy, not a core
    switch.** The roadmap asked for "the pinch soak with the ingress mask
    disabled"; the mask lives in the core and the core is frozen, and a debug
    switch would be a core change for a test. The pinch's pre-ingress
    fabrication (+9.5% over 12 000 passes) stays history's red run (#33); the
    gate's own red uses the mechanism #33 kept fixture-pinned in the v1
    operators: ink laid on the left edge and tines dragging it inward
    duplicate the clamped edge column every pass — **+14.0% in 300 passes**.
    The other two reds: a (+k, −k) pinch pair whose −k sits at an offset
    centre (204 texels, −99.7%); an over-stepped wake stream — a stroke of
    fifteen tip radii every frame, fifteen internal sub-steps, over the full
    6000 window — for (d), which is the class rule (≤ a/4 PER FRAME at
    gesture rate) stated as a failure. A chaotic pinch schedule was tried
    first and dropped: its erosion is front-loaded and dilutes below 2× over
    any window long enough for the control to be meaningful (1.4× over 3000
    passes at k = 0.3). `--soak-negative` asserts that each part fails.
    Flagged against the roadmap's literal wording.

15. **The resampled medium is not mass-neutral, in both directions, and the
    (b) mass bar is ±5%, not #33's ±0.5%.** Measured while calibrating, on
    Metal: the incumbent tine's strong ±pairs (z = 0.05, ~25 texels at the
    line) GAIN mass steadily after the first ~200 pairs, +0.76% at 500 — all
    of it interior (the 16-texel edge band contributes exactly zero, so this
    is not the clamp legacy): 6 900 units leave the initially inked texels
    and 7 550 appear in the water around them, a net gain of 650 at the
    ink/water boundaries under bilinear gather. The gain scales with the
    boundary length a pass moves: the ripple bake, an exact whole-canvas
    shear of only 12.8 texels at full amplitude (it carries the ingress rule,
    so this is not clamp either), gains +2.69% over the same 500 pairs while
    its pre-image returns to 2.6 texels — every ring edge on the canvas moves
    on every pass. The pinch pairs, exact by construction (#33), drift
    −0.43%/+0.15% with a 2.3-texel pre-image wander; the swirl and both
    vortex profiles stay within ±0.5% (1.5–2.7 texels). A FRESHLY laid drop gains mass over its
    first few hundred glide passes (−1.2·10⁻⁵/pass "erosion" at 300 passes,
    i.e. growth) before the steady fade of ~1.2·10⁻⁵/pass sets in; #33's
    control never saw this because it ran on a field aged by 6000 pinch
    passes. So: the exact-class mass window is ±5% — it has to clear a
    whole-canvas exact shear, and Chladni, the spark and Chirikov are all of
    that family (the references: tine +0.76%, ripple +2.69%, a broken pair
    −99.7%); the pre-image bar is 4 texels (references at 1.5–2.7) and is
    the criterion that actually decides; (d) is gated only in the steady
    window. The
    mechanism behind the gain is not established here — the gather of a
    det = 1 map conserves Σ in exact arithmetic, so it is in the filtering
    precision or the half-float representation of the phase — and it is
    recorded as a Phase-6 question, not chased in this step: the gate needs
    the medium's floor, not its explanation. Flagged against the roadmap's
    ±0.5%.

16. **The ripple soaks through its CC route.** The bend-driven bake drifts
    the ripple phase a little per pass ON PURPOSE (#36 permanence: an
    excursion never retraces exactly, so vibrato leaves a mark), which makes
    bend-driven ±pairs non-inverting by design. The amplitude CC (102,
    `bend_mode = 0`, `ripple_bake = 1`) keeps φ fixed — the composing-back
    group the operator's exactness claim is about — so both the pairs and the
    stream run through it. A future operator whose route drifts a parameter
    by design has the same choice to make, and this entry is the precedent.

17. **The crossed-tine pinch is exact per pass but NOT sign-reversible
    through its ABI, and it fabricates under torture — recorded red in the
    baseline table, flagged as a Phase-6 fix candidate.** The gate's first
    full run: `pinch-cross` (b) leaves the pre-image 92 texels adrift and
    GAINS 25% of its mass over 500 ±k pairs, while the saddle passes. The
    reason is in `sumi_deform_crossed_pinch` (`core/src/displacement.cpp`):
    the variant is a composition of two perpendicular infinite-line tines
    T₁∘T₀, and a negative k reverses BOTH drags but keeps the ORDER — the
    inverse of T₁∘T₀ is T₀⁻¹∘T₁⁻¹, and crossed shears do not commute, so the
    sign flip leaves a second-order kick-drift residual every pair (the same
    order-matters fact `MEDIUM §2.2` states for Chladni). That residual
    walks ink to the canvas edges, and the tines carry the v1 edge-clamp
    legacy (#33 kept it fixture-pinned) — hence the fabrication. Two
    consequences: DECISIONS_4 #69's "a squeeze-and-release nets out in exact
    math" holds for the saddle only; and the fix — emit the tines in reversed
    order for k < 0 — is a one-line core change that would make release
    retrace exactly, which the author must weigh against the look (#69 also
    calls what the release does not retrace "marbling"). Not touched in this
    step (core frozen; the gate needs the baseline, not the fix); the (c)
    and (d) columns are green for the variant. The gate now also asserts that
    the stream MOVED the field (> 100 texels changed) inside (c), the
    step-19 soak's own sanity check, so a mis-wired route can never pass
    trivially.

## Step 36 — Wave torsion, the proof brick (macOS)

18. **Torsion is the third vortex profile, and the pass grew without moving
    the fixture.** `SUMI_VORTEX_TORSION = 3` — value 2 stays the gesture-only
    Lamb–Oseen, which `sumi_add_vortex` reroutes to the swirl pass, so the
    CC-routed profiles are 0, 1 and 3 and every picker skips 2. The vortex
    shader's uniform block gained `k` and `phase` and its branch tests
    torsion FIRST (`profile > 2.5`), then Rankine, then the exponential
    default, so the exponential path's expression is byte-identical to
    v0.9's; `tests/fixtures/field_512_metal.bin` stays bitwise on Metal
    (max|d| 0) and the web tier reads max 9.8·10⁻⁴ / mean 7.9·10⁻⁹, inside
    its documented tolerance. The queue payload `sumi_deform_vortex_t` gained
    `k, phase` (zero for the other profiles; the §4.6 field script sets them
    explicitly so the uniforms stay bit-identical). `sumi_version` → 0.10.0:
    additive — the enum value, two ctl dims, one params field.

19. **k and φ are flavour controls, the ripple's precedent.**
    `SUMI_CTL_TORSION_K = 14` maps 0..1 onto 2π·4 … 2π·40 radians per canvas
    height (4 to 40 rings across the sheet; rests at 0.5) and
    `SUMI_CTL_TORSION_PHASE = 15` onto 0 … 2π (rests at 0); `SUMI_CTL_COUNT`
    is 16. Unmapped in the core like the ripple's dims; the desktop's stock
    map adds CC 104/105 as its handles (map version 4 — an INI still carrying
    the version-3 stock map upgrades, #71's mechanism) and the CC-map editor
    lists the two names. Every vortex route reads the mapper's SMOOTHED
    values at emit time — the gesture route (`sumi_add_vortex`) through
    `sumi_voice_mapper_torsion_kphi`, the CC-routed vortex and the sweep in
    the mapper itself — so a slider move never jumps the pattern.

20. **The engine's first episode: the note-on torsion sweep, opt-in.**
    `params.torsion_sweep` (default 0) arms, on every VoiceBegin, a per-voice
    time-driven emitter: φ = φ_ctl + ω·t with ω = 2π·1.5 rad/s, amplitude
    RATE·e^(−t/τ) with RATE = 1.2 rad/s and τ = 0.6 s (the integral, 0.72 rad
    at the crests), reach 3× the strike radius floored at 0.05, over after 4τ.
    Every frame emits that frame's rotation INCREMENT (rate · envelope · dt)
    as a torsion pass at the voice's current centre(s) — the delta rule, so
    two strikes add and a frame the budget refuses merges its increment into
    the next. The episode runs whether or not the note is still held (a
    discharge dies on its own clock; the loop handles it before the
    `active` check) and a new note in the slot re-arms it. Measured
    (`--torsion-test`): a marker 0.12 from the strike swings 0.087 rad, the
    largest 10-frame step is 0.038 rad against a 0.15 bound (a whole pattern
    at once would read ~0.7), the angle at 3 s equals the angle at 4 s to
    10⁻⁴ although the note was released at 2 s, and a second strike re-arms
    with the same bounded steps. Test-design note recorded: a second strike's
    own drop pushes an outside marker RADIALLY (√(r² + R²)), so its net
    rotation is not comparable to the first's — the re-arm is checked by
    swing and step, not by accumulation. The strike still lays its drop; the
    sweep rides on top until the Anod binding table (step 42) makes it the
    strike.

21. **The gate learned two things from its first new operator.** (i) Its
    (c) criterion — "mass never grows past 0.5% over 6000 passes" — was met by
    every v1 operator only because their erosion outran the medium's
    boundary gain (#15). The torsion at its default wavelength gains
    3.8·10⁻⁶ per pass (+2.26% over the window), ALL of it interior (edge
    band exactly 0), an order below edge-clamp duplication (4.7·10⁻⁴/pass in
    the negative control): the medium at high spatial frequency, not
    fabrication. (c) is now "growth ≤ 0.5% over the window, OR a growth rate
    ≤ 5·10⁻⁵ per pass", and the line prints the edge/interior split so the
    reader sees where the mass appeared; the negative control trips both
    forms. (ii) Pair magnitudes now follow one convention — about 25 texels
    of displacement at the ink (tine z = 0.05, pinch k = 0.3, one a/4 wake
    step, rotations ~1 rad at R = 0.25, torsion 0.5 rad on a 39-texel
    wavelength at R = 0.5, its k set through CC 104 before the scene) — a
    pair at a pathological scale (a radian across a 23-texel wavelength,
    7.8 texels of drift) measures the resampler, not the operator. Even so
    an oscillatory exact field wanders more than a smooth one (5.45 texels
    against 1.5–2.7 while its markers return to 3·10⁻⁴ rad), so the exact-
    class pre-image bar moves from 4 to **8 texels**, 25× under the 204 of a
    non-inverting pair. Chladni, the spark shear and Chirikov are all
    oscillatory whole-canvas shears: expect the same numbers, and treat a
    smooth operator that reads above 3 as a question.

22. **The `torsion` scene ships in the marble app and the web gate's sweep,
    not yet in the docs' check.** `web/site/scenes.js` gained the scene (A,
    k, φ, R, the sweep flag and the pace), the web host a `torsion_sweep`
    parameter id, `tools/web_gate.mjs` the name (12/12 scenes run clean on
    the new wasm). `site/scripts/check.mjs` requires every scene it lists to
    be embedded by a page, and the page is drafted in this step's evidence
    (`torsion.mdx`) to land at step 63 (#9) — listing the scene now would
    break the docs build. The web host's own settings panel (the profile
    picker) is step 46's; the scene calls the profile by value.

## Step 37 — Chladni lattice (macOS)

23. **The Chladni operator is the Taylor–Green cellular flow on the layout's
    cell lattice, split into two exact diagonal shears.** The step arrived
    here through four designs in review, the first of them committed: (1) a
    separable kick-drift x₁ = x + a·cos(k_y·y), y₁ = y + b·cos(k_x·x₁) with
    its ratio from the interval between the two lowest voices — the roadmap's
    text; (2) the same with every cell centre a node; (3) a channel profile
    with every cell a still island; (4) this. The author's objection to (1)
    and (2) was legibility ("the layout will be hard to see with only the
    waves"), to (3) that the breathing "looks kind of bad" — a quadrature of
    shears wobbles, it does not form a figure — and, decisively, the physics:
    with det J = 1 Liouville forbids any change of density, so no operator of
    this engine can GATHER ink the way sand gathers (the author's derivation
    of the ponderomotive potential V = ¼mω²W² with the nodal lines as its
    minima is in `chladni.md`; settling there is dissipative — grains
    oscillate across the trough forever without friction — and a sheet with
    positions only cannot carry the momentum the conservative version needs).
    What the engine CAN do is stretch: iterate an area-preserving flow whose
    separatrices are the lines wanted, and the ink is drawn out along them
    (Aref & Ottino's chaotic advection). For a plate mode W = cos(k_x x)·
    cos(k_y y) the nodal lines W = 0 are the cell boundaries, and the
    Taylor–Green stream function ψ ∝ W — an exact Navier–Stokes solution,
    the founding rule's welcome guest — has an eddy in every cell (neighbours
    counter-rotating) and its separatrices exactly on W = 0: the fluid
    streams the ink along the lines where the sand would settle. ψ = cos u·
    cos v is not separable, but ½[cos(u−v) + cos(u+v)] is a sum of two waves
    each depending on one DIAGONAL coordinate, and the flow of such a term is
    a pure shear along the direction where that coordinate is constant —
    (k_y, k_x) and (−k_y, k_x) — with magnitude ½Ψ·sin(·): each is exact, so
    one step of the flow is two exact passes (`SUMI_DEFORM_CHLADNI` with a
    `stage`), det J = 1 at any amplitude, the kick-drift splitting of a
    symplectic integrator. CLASS EXACT. The exact inverse of a step is both
    shears negated in REVERSED order (#17); `sumi_add_chladni(psi, balance,
    s_x, x_0, s_y, y_0)` takes a negative psi as that inverse, and the mapper
    and the gesture share `sumi_chladni_emit_step`. The "simultaneous" form
    stays the headless NEGATIVE (`test_chladni_kick_drift_order`). On the
    GPU one step and its inverse leave the interior pre-image within a
    fraction of a texel of where it was (the whole-field figure is larger
    only by the ingress bands, fresh water by design).

24. **Bake only, steadily driven — no live path, no quadrature.** The
    author's call: "remove the live and bake and only do bake". The flow
    writes into the field while the stir control is up, the vortex's pattern
    — rate × dt every frame, never an absolute, one step = two passes under
    the budget — with Ψ = rate/(k_x·k_y) so that `SUMI_CHLADNI_RATE` (1.5
    rad/s at ctl 1) is the cells' rotation rate. Nothing breathes: the
    quadratures of the earlier designs (cos ωt, sin ωt) only wobbled the
    sheet, and their delta-driven bake left residue by construction. The
    composite lost its Chladni block (the live path of design 3) and the
    print path has nothing to zero; `params.chladni_bake` and
    `chladni_channel` are gone. `sumi_version` stays 0.11.0: none of it had
    shipped.

25. **The layout is the plate — with a cell-size knob, and no Faraday.**
    `sumi_layout_cell_lattice` (`core/src/layouts.cpp`, internal) reports
    each playable layout's cell pitch and first centre — the Jankó's stagger
    and the piano grid's accidentals sit at half-cell offsets, so those two
    report the HALF pitch along x and every cell is an eddy; the layout lives
    in normalized space and the pass in aspect-corrected space, so x converts
    through the aspect normalize() last saw. Each note's drop, at its cell
    centre, spins in place (the elliptic point; the Rankine core's look); the
    cell corners are the saddles; the ink between is stretched along the
    boundaries into the figure that outlines the grid. `params.chladni_cell`
    (0.5..1.5, default 1) scales the lattice pitch about the layout's first
    cell centre — one eddy per cell at 1, four at 0.5, a cell and a half at
    1.5 — the author's "cell size" slider. The layouts without drawn cells
    take, at the author's instruction, the largest IMAGINARY square cell that
    does not touch a neighbour's: the circle of fifths its octave-ring
    spacing (0.032 canvas heights — pitch classes on a ring are 0.52 r apart,
    at least 0.052 on the innermost ring, so the rings bind), centred on the
    circle; the rolls one semitone of their pitch axis (0.88 of the canvas
    over 128 notes = 0.0069, three and a half texels at 512 — the eddies are
    at the texel scale there and the flow reads as fine shear; the octave
    would be the legible alternative, the author's to pick), anchored on the
    note positions and the now-line. A `chladni_faraday`
    switch (the lattice half a cell over: eddies on the corners, the
    figure's lines through the cells) was built and measured — the fixed
    points swap type exactly — and then REMOVED at the author's request on
    closing the step: the real inverse Chladni effect is boundary-layer
    acoustic streaming, which the author wants to think through properly on
    another day rather than approximate with a phase shift; recorded here as
    the author's deferred idea, with the half-cell shift in this entry's
    history as the cheap version it is not.
    `SUMI_CTL_CHLADNI_A` (16) is the stirring rate and `_B` (17) the balance
    between the two diagonal waves (weight 1 − 2B on the second: 0 the cells,
    ½ a single diagonal wave, 1 the cells reversed); CC 106/107 on the
    desktop, stock map v5. Measured on the chromatic grid through the public
    probe after ninety stirred frames: the cell centres and corners stay
    fixed while the boundary midpoints move; a ring round a centre TURNS
    and a ring round a corner STRETCHES; the same holds on a 16:9 field; at
    cell size 1.5 the mapper's pitch is 1.5× the probe's and the probe's
    centre cell is still a lattice centre; at 1 the mapper's pitch equals
    the probe's to 10⁻⁴ and its centres fall on the probe's to 10⁻⁷ of a
    pitch (the numbers are in the step's evidence).
    Flagged against the roadmap's step-37 text (the separable pair at both
    insertion points, the interval ratio), superseded by the author's
    decisions in review; the author's `chladni.md` records the physics.

26. **Test observables that survived the first run, recorded for the next
    operators.** (i) A pair's residual is measured over the INTERIOR (a margin
    of the displacement plus a texel) — the ingress bands are fresh water by
    design and dominate a whole-field mean. (ii) A lattice's wavenumber is
    read by the dominant Fourier component of a displacement profile, not by
    zero-crossing counts, which the distortion of many composed passes can
    push off by one (the row read 7 crossings for 3 waves). (iii) A test
    that compares two renditions of a scene must start both from a fresh
    sheet — the first run's dip check laid its first scene over the previous
    part's rings. (iv) The quadrature's phase is the mapper's clock since the
    instance was created, so a bake test cannot assume which shear is strong;
    it checks whichever carries more than two texels. (v) In a two-dimensional
    kick-drift only the cell CENTRES — where both shears vanish — are fixed
    points of every pass; along a row's centre line the perpendicular shear
    still moves things and the composed passes bend the rest, so nodes are
    measured at points (the 84 cell centres against the 66 corners), never
    along lines — the first measurement along lines read a ratio of 0.5 where
    the point measurement reads 0.013. (vi) Node spacing is HALF a
    wavelength, so an inversion is a quarter-wavelength phase shift. (vii)
    A flow's fixed points are classified by what they do to a RING of texels
    around them — an eddy rotates it, a saddle stretches it (mean |log r'/r|)
    with no net rotation — never by "tangential versus radial": after a
    radian of turning a ring point's chord has a large radial component, and
    the first classifier read 1.4 : 1 where the rotation reads 0.33 rad
    against 0.00. (viii) Neighbouring eddies COUNTER-rotate, so a rotation
    averaged with its sign over the lattice is zero by construction — the
    second classifier read exactly 0.00 before the per-ring absolute value.

27. **The `chladni` scene ships in the marble app and the gate's sweep, not
    yet in the docs' check (#22's rule).** A chord on the chromatic grid —
    its drops are the eddies' centres — stirred for a chosen number of frames
    at a stir, a balance and a cell size; the web host gained one parameter
    id and the `chladni` cwrap (a lattice of the gesture's own); the
    settings-panel controls are step 46's. The desktop settings window
    gained a "Chladni" section: stir and balance as CC sliders on the routes
    and the cell-size slider.

## Step 38 — Viscous multipole burst (macOS)

28. **The derivation holds, numerically, and the literature check is on
    record before any wording.** `tools/multipole_verify.py` (the sibling of
    `stokeslet_verify.py`; its output in `docs/evidence/step38/`) checks
    every link of MEDIUM §2.3 in pure Python: the m-th multipole heat kernel
    ω_m ∝ s^{m+1}e^{−s} sin(mθ)/r^{m+2} (∂_z^m of the Gaussian) solves the
    vorticity diffusion equation; its stream function is ψ_m = (K(m−1)!/4)
    γ_m(s) sin(mθ)/r^m with the spec's cutoff γ_m(s) = 1 − e^{−s}Σ_{k<m}s^k/k!
    — the mode-m Green's function and the recurrence γ(m+1,S) + S^m e^{−S} =
    m·γ(m,S) (m = 0 is Lamb–Oseen, m = 1 the Stokeslet of DECISIONS_4 #53);
    the time integral is ∫₀ᵗ γ_m(r²/4ντ) dτ = (r²/4ν) Φ_m(S) with Φ_m(S) =
    ∫_S^∞ γ_m/s² ds, and by parts Φ_m = γ_m(S)/S + Γ(m−1,S)/(m−1)! — the
    remainder is E1 for m = 1 (the logarithmic kernel that keeps the dipole
    special) and an exponential polynomial for m ≥ 2, so Φ_m is elementary
    and equals the spec's form (1/S)[1 − e^{−S}Σ_{k≤m−2}(1 − k/(m−1))S^k/k!]
    to 10⁻¹³; Φ_2 = χ = (1 − e^{−S})/S, the Stokeslet's own χ. The blob
    kernel Ψ = A_m (a/r)^{m−2} sin(m(θ−θ₀)) [Φ_m(r²/ℓ₁²) − Φ_m(r²/ℓ₀²)] gives
    d = ∇⊥Ψ in closed form (d_r, d_θ in the shader header), divergence-free
    to 10⁻⁹, zero at the origin; near the core the quadrupole is the pure
    hyperbolic strain λ(x′, −y′) with λ = A₂(1/a² − 1/ℓ²) → 1.359 D/a — the
    pinch is its r → 0 limit, as §2.3 says — and order m is the harmonic
    polynomial Im(z^m), |d| ∝ r^{m−1}. **One precision on the spec:** the
    "cos(mθ)/r far field" is exactly the QUADRUPOLE'S diffused zone a ≪ r ≪ ℓ
    (measured slope −0.997); order m decays there as cos(mθ)/r^{m−1} and
    beyond ℓ every order falls as 1/r^{m+1} (the potential multipole times
    the age). Not a conflict — §2.3's sentence is about the quadrupole, the
    primary voice — but the page draft says it the general way. **The
    normalisation** is the lobe displacement AT r = a on the ejection axis
    (D); the roadmap's "peak lobe displacement at r = a" is read that way
    because the true peak sits at 1.07–1.45 a and is 0.4–9% above D for the
    quadrupole (1.4–1.6 a and 20–34% for m = 3): the spec's point — the
    stagnation origin makes centre-normalisation meaningless — stands, and
    r = a is the clean anchor. **The literature** (`literature.md`): the
    velocity fields of the viscous multipoles are classical (Voropayev &
    Afanasyev's Stokes-approximation multipoles; Chan & Chwang's unsteady
    2-D singularities; the Hermite modes of Gallay–Wayne and Uminsky–Wayne–
    Barbaro), the time integration is Jaffer's move (arXiv:1810.04646, m =
    0), and the displacement form for m ≥ 2 we did not find stated. The
    docs say "method after Jaffer, extended here"; never "new" or "first".

29. **Class sub-stepped; the wake's a/4 rule generalises to "peak
    displacement ≤ β_m × the pass's current core".** The criterion behind
    the wake's a/4 is |∇d| ≤ 0.25 (the inverse lookup's det ≥ 0.5). For the
    burst the API amplitude is the wrong yardstick: an aged pass of order
    m ≥ 3 acts at r ~ ℓ₀ where d ∝ r^{m−1} dwarfs the lobe at r = a —
    max|∂d| per (D/a) reaches 194 for m = 4 at ℓ₀ = 11a — while normalised
    on the pass's own peak displacement and its current core ℓ₀ the gradient
    is bounded for every order and age in the table: 1.83, 2.29, 2.39, 3.04,
    3.15, 3.58, 3.27 (d_max/ℓ₀) for m = 2..8, hence β_m = 0.137, 0.109,
    0.105, 0.082, 0.079, 0.070, 0.076, shipped with a margin as 0.13, 0.10,
    0.10, 0.08, 0.075, 0.068, 0.072 (`sumi_burst_budget`). At the budget the
    inverse-lookup det stays ≥ 0.98. The peak lies on the ejection axis
    (verified for every row), so the C side finds it by a log-spaced scan
    and a golden-section refinement (`sumi_burst_peak`); the **greedy
    march** (`sumi_burst_step`) takes the largest ℓ′ whose increment is
    within budget, by bisection in ℓ² — the peak grows monotonically with ℓ′
    because Φ_m falls with S. A quadrupole of D = 2a over age 4 marches in
    thirteen pieces, each ≤ its budget (headless test). On the GPU one
    budgeted pass reads det min 0.844 everywhere, mean 1.00000; the pair
    (+D, −D) at D = a/4 leaves max 0.76 texel, mean 0.018 — the class's
    first-order residual |∇d|·d, informational as the wake's. **expm1:**
    GLSL has none; the shader carries the equivalent — the small-S series of
    the plateau DEFICIT 1/(m−1) − Φ_m below S = 1 and the closed form above,
    the difference of two near-core values taken between deficits, never
    between two plateaus (the Lamb–Oseen small-r lesson, verbatim). The
    shader reproduces the double reference along the axis at a/2 … 4a to
    0.023 texel.

30. **The gesture IS the strike with its lifetime; the age and the release
    are parameters; D is the linearised amplitude.** `sumi_add_burst(x, y,
    a, D, θ₀, m)` registers an EPISODE in the mapper (32 slots; a full table
    replaces the episode nearest its end): the age grows as ℓ² = a² +
    (ℓ_end² − a²)·t/life — the spec's ℓ² = a² + 4νt with 4ν set by the
    release — and each frame the increment since the last emitted age goes
    out as budgeted passes, the first in the next `sumi_update`.
    `params.burst_age` (ℓ_end/a, 1.5..12, default 4: 92% of the eventual
    displacement of the quadrupole, 47% at 1.5, 99% at 12) shapes the burst
    without scaling it, since D is measured over the burst's own age;
    `params.burst_life` (0..4 s, default 0.8; 0 = at once) is the release;
    `params.burst_order` (2..8, default 2, at the author's request) is the
    order a strike takes when the gesture passes m = 0 — the desktop's U key
    and, at step 42, the strike route until the pitch-class → m table
    overrides it per note. `m` clamps to 2..8 — 8 because the budget table
    (#29) and the shader's bounded loops stop there, and the higher orders
    keep their motion within ~1.6 cores anyway (d ∝ r^{m−1} at the core);
    θ₀ is in the canvas frame (y down); D < 0 is the first-order inverse. **The emission floor is the field's quantum:** the
    coordinates live in half floats, whose spacing in the outer half of the
    canvas is 2⁻¹¹ canvas heights (4.9·10⁻⁴), and a pass that moves a
    texel's source by less than half of that rounds back to where it was.
    With a 2·10⁻⁴ floor the release's tail vanished pass by pass (the lobe
    stalled at 73% of D, measured); at one quantum (5·10⁻⁴, merged until
    the increment's peak reaches it) the tail lands within 0.35 texel of D —
    8% of a 4-texel strike lost to the medium's quantisation, recorded. One
    episode takes at most 24 passes a frame; a pass-budget refusal merges by
    construction (the age bookkeeping IS the pending accumulator). **D is
    Eulerian:** one pass applies the closed-form field exactly; a strong
    strike composed of many passes follows the FLOW, and the quadrupole's
    core strain integrates to e^λ with λ = 1.36 D/a — at D = 2a the material
    moved four times the linear prediction. Physical (the flow of a strain
    field is exponential), documented in the ABI comment, and the reason the
    tests stay at D ≤ a/4. The strike route waits for the binding tables
    (step 42); the desktop bench fires it with U (Shift+U: m = 3) at the
    cursor, its axis toward the canvas centre.

31. **Test observables for the burst family, recorded.** (i) The field
    stores each texel's SOURCE, so st − (u, v) is the displacement of the
    material now at the texel and its radial part is positive when ejected
    — the sign the spec's θ₀ promises. (ii) Read displacements on the −x /
    −y side of a centred burst: u < 0.5 there, where the half-float quantum
    is 0.125 texel at 512 (0.25 on the + side), and the residual of a
    quantised increment is halved. (iii) A pair test cannot catch a
    per-piece amplitude error in a sub-stepped operator — both signs scale
    alike and cancel regardless; the amplitude's test is one pass against
    the closed form. (iv) A far-field law is a property of ONE pass; a
    composed strong strike is the flow, not the field, and a 1/r reading
    taken on it is wrong by e^λ (the first draft of the check read 3.0 for
    a predicted 2.03 at D = 2a). (v) An episode's end is the mapper's
    count (`sumi_debug_burst_count`), not the field: the last sliver below
    the floor closes without a pass.

32. **The gate: the strike stream at gesture rate.** `--soak burst`: the
    pairs at D = a/4 (two pieces each way, at once), the (b) det on one
    budgeted pass everywhere (no body, no capsule), and a stream of one
    quadrupole strike every third frame with a three-frame release — 2000
    strikes over the 6000-frame window at about one pass a frame, the
    wake stream's density, so (d)'s per-pass fade compares like with like.
    Green: (b) det min 0.812 everywhere, mean 1.00000; (c) growth +0.00%,
    interior −5688, route alive (71 539 texels moved); (d) 9.30·10⁻⁶/pass
    against the tine's 1.15·10⁻⁵ (×0.81). The pairs, informational, fade
    −17.6% over 500 pairs of FOUR passes (two pieces each way at D = a/4)
    against the wake's −4.7% over pairs of two: per pass twice the doublet
    pair's, the first-order residual reshuffling the boundary each pair.
    Recorded, not gated — the class's (b) is the det (#13).

33. **The `burst` scene ships in the marble app and the gate's sweep, not
    yet in the docs' check (#22's rule).** Two strikes on the clusters, A
    along θ₀ and B a quarter turn on, with D (of the core), the core, θ₀,
    the order, the age and the release as sliders; the web host gained
    three parameter ids and the `burst` cwrap; the desktop settings window a
    "Burst" section (age, life and order, with the note that the strike
    route arrives at step 42). The page draft `burst.mdx` carries the lineage
    line and the author's note from the roadmap verbatim, marked for the
    author to trim and sign.

## Step 39 — Spark shear & the composed strike (macOS)

34. **"Shears invert for any profile" is now a test, on the GPU and in
    double — and so is its converse.** The spark shear pass (`deform.glsl
    spark_fs`, `SUMI_DEFORM_SPARK`) is one stage of MEDIUM §2.4's piecewise
    kick-drift: in the frame rotated by θ₀ about the strike, stage 0 slides
    each row by A·w(y)·f(y), stage 1 each column by B·w(x₁)·f(x₁), with f a
    stack of triangle waves at k, 2k, 4k, 8k (weights 1, ½, ¼, ⅛,
    normalised so |f| ≤ 1; `params.spark_stack` the depth, 1..4, default 3 —
    the spec's stack out of a magic constant) or of piecewise-linear hash
    noise (`params.spark_profile` = 1), and w a Gaussian window ACROSS the
    shear (band 0 = none). A shear x′ = x + g(y) inverts as x = x′ − g(y)
    whatever g is, so each stage is exact for any profile and its kinks are
    creases, legally. Measured (`--spark-test`): a step of 15-texel kicks on
    a 43-texel base moves the interior pre-image 7.35 texel; the step then
    its exact inverse leaves 0.212 texel (max 2.10, at the kinks — the
    resampler's), for the noise profile 0.009 (max 0.35); the same-order
    sign flip leaves 6.17 and 1.60 — NOT an inverse, the Chladni lesson
    (#17) again. Headless the same in double: the inverse to 10⁻¹², det J =
    1 to 10⁻⁴ through the kinks, both profiles. **The exact inverse of the
    step (A, B) is the step (0, −B) followed by the step (−A, 0)** — reversed
    order and negated — and the gesture makes it expressible by skipping a
    zero amplitude: no sign convention, two calls. One stage alone is a pure
    shear: no row moved in y (0.000 texel), each row slid rigidly (in-row
    spread 0.039 texel), the largest row 11.7 texel. The noise is an integer
    hash (lowbias32), bit-identical on Metal, GL and WebGPU.

35. **The wavenumber is a flavour ctl, and CC 74 is prepared to drive it.**
    `SUMI_CTL_SPARK_K` (18; COUNT 19) maps 0..1 to a BASE wavenumber of 2π·2
    .. 2π·24 per canvas height — two waves across the height (a thick
    channel) to twenty-four (fine streamers) — with the octaves stacking
    above it, so the finest wave at the top of the range is 5 texels at 512
    and the mid default a 39-texel base. The first draft ran to 2π·64: with
    three octaves the finest wave was 2 texels and the default 4 — the
    profile aliased at the harness's resolution and an exact 15-texel kick
    on it had slopes of several texels per texel. **`slide_mode` 2** routes
    a member channel's CC 74 to this ctl (the latest voice's slide wins; the
    aux modulation and the pinch stay on modes 0 and 1 — one consumer), not
    the default: the "slide-mode-style Anod default" the roadmap asks to
    prepare, for the binding tables to switch on (step 42). The desktop
    stock CC map gained the CC 108 handle (v6; v5 migrates), the settings a
    "Spark" section (shear, decay, octaves, profile, frequency as a CC
    slider on the route) and the slide radio its third option.

36. **The shear is an episode: kicks that decay as e^{−t/τ}, spent as
    kick-drift steps at the field's quantum.** `sumi_voice_mapper_add_spark`
    registers a strike (32 slots; a full table replaces the episode nearest
    its end) with A = B = `params.spark_shear`·r (default 0.6 of the strike
    radius), τ = `params.spark_tau` (default 0.25 s, over at 4τ), the window
    2r across each shear, k from the SPARK_K ctl at the strike, φ drawn per
    strike from a small LCG so consecutive strikes crease differently. Each
    frame the exact increment A(e^{−t₀/τ} − e^{−t₁/τ}) joins the pending kick,
    which goes out as one step (two exact passes) once it reaches the
    half-float quantum (#30); the last sliver flushes when the episode
    ends. Successive steps do not commute — each is exact, the composition
    is exact, but the emission granularity is part of the look, bounded
    below by the quantum and above by the frame — so the total kick is
    A(1 − e^{−4}) exactly (headless: to 10⁻⁶) while the figure it draws
    depends on how it was dealt. Emitted amplitudes decay; pairs are
    stage 0 then stage 1 with equal kicks.

37. **The composed strike, and its class by inheritance.** `sumi_add_spark
    (x, y, r, D, θ₀, layer)` is the drop (the Joule blast — radial outflow
    is divergence, so the engine's oldest exact operator does it; the layer
    as `sumi_add_drop`'s, so the soak can strike with clear water), the
    burst of core r and lobe displacement D along θ₀ (order
    `params.burst_order`, the m = 0 rule of #30), and the shear episode
    along and across θ₀. The drop lands in the gesture's frame, the two
    episodes from the next update. `sumi_add_spark_shear(x, y, band, A, B,
    k, φ, θ₀)` is one step of the shear as a gesture (the stack and the
    profile the params'). CLASS: the shear declares EXACT and soaks as
    `spark-shear`; the composition declares SUB-STEPPED BY INHERITANCE — the
    roadmap's strictest-member rule, the burst — and soaks as `spark` under
    the burst's numbers. Measured on one composed strike (clear water, r =
    0.05, D = 0.0045): the whole composition's field differs from the
    drop's alone by 1.79 texel over r..3r; the Jacobian is read with the
    shear off — the drop then the burst, the sub-stepped member inside the
    exact one's field (#38) — and stays first-order (see the summary), the
    drop alone reading 0.812 in the same region.

38. **Test observables for shears with kinks and for compositions with a
    drop, recorded.** (i) Finite differences cannot measure an EXACT
    kick-drift shear at its kinks, at ANY slope: analytically det = (1 +
    A f′·B g′) − A f′·B g′ = 1, but on the resampled field the two
    difference quotients straddle a kink unequally (one spans 2h, the other
    2h·B g′) and the stencil reads 1 ± 2·A f′·B g′ — 0.46 at a 0.45
    texel-per-texel slope, −6.7 at 2.7 — on a map whose Jacobian is
    identically one. The Jacobian read of a composition therefore switches
    the shear OFF and reads the sub-stepped member (the drop then the
    burst); the shear's exactness is the inverse test's business, which is
    exactly the strictest-member rule made operational. (ii) A drop's rim is
    a singularity of the pre-image: inside, the identity; outside,
    sqrt(d² − r²), whose slope d/sqrt(d² − r²) stays above 1.5 texel per
    texel until d = 1.34 r; and the passes that FOLLOW a drop carry the rim
    with them — a texel inside whose source lies across the rim reads the
    compressed exterior, so the jump moves inward by their displacement (det
    −4.7 at 0.87 r, the drop alone reading 1.000 there). Exclude r − 8
    texels .. 1.4 r, not ±3. (iii) A shear
    band that reaches the canvas edge has an INGRESS SEAM — fresh water
    beside a row that sheared out — which the stencil reads as a fold; keep
    an edge margin of the kick's reach. (iv) A kick-drift step's inverse
    residual concentrates at the kinks (max 2.1 texel against a mean of
    0.21 for the triangle stack): the mean is the invariant to gate, the
    max the resampler's.

39. **The gate, and what a drop does to it.** `--soak spark-shear` (exact:
    pairs of a 15-texel step and its reversed-order inverse in a 0.2 window;
    a stream of small steps whose kick wobbles in sign — chaotic advection
    under a jagged shear): green — pairs hold mass −0.59%/+0.00% with a
    pre-image dev of 3.60 texel, growth +0.00%, erosion 1.83·10⁻⁵/pass
    against the tine's 1.15·10⁻⁵ (×1.59: kinks every few texels fade
    faster than smooth shears, within the bar). `--soak spark` (sub-stepped
    by inheritance): the Jacobian read on one composed strike — the drop
    then the burst, the shear off (#38) — det min 0.750, mean 1.00004,
    inside the wake's numbers (0.734 / 0.766). **The first stream run read
    mass 0 at 500 pairs and at 6000 passes** (pre-image dev 145 texels, (d)
    ×14.5): not erosion — every clear-water strike is an exact expansion
    that pushes the ink outward, and 1500 of them at one spot push it off
    the canvas, which no mass observable can tell from loss. The gate has
    excluded drops "by nature" since #13; the composed strike inherits that
    exclusion for its blast. So `SUMI_DROP_NONE` (3) joined the drop layers
    — `sumi_add_drop` lays nothing, `sumi_add_spark` fires the burst and
    the shear episodes without the blast — and the `spark` pairs and stream
    run blast-less: composed strikes every fourth frame, the axis turning,
    a three-frame burst release and a ten-frame shear episode. Green:
    growth +0.00% (route alive, 260 893 texels moved), erosion
    1.85·10⁻⁵/pass against the tine's 1.15·10⁻⁵ (×1.61) — the same order as
    the shear's own ×1.59, the burst's alone being ×0.81: the kinks are
    fine structure and the medium fades fine structure first (#15). The
    blast-less pairs, informational, read −9.53% and 20.6 texel: a −D strike
    negates the burst only, both strikes' shear episodes add, and the
    shear's own inverse is `spark-shear`'s business. Results in
    `docs/evidence/step39/SUMMARY.md`. Scene `spark` (the marble app and
    the gate's sweep, not the docs' check — #22): A shows the composition
    up to a chosen stage — the drop, then with the burst, then the whole
    spark — and B the whole spark a quarter turn on; sliders for the
    radius, the burst's D, the shear kick, the frequency (CC 108), the
    decay, the octaves, the profile and the axis. The web host gained four
    parameter ids and two cwraps; the desktop bench the Z key (the composed
    strike at the cursor, its axis toward the centre). Page draft
    `spark.mdx` in the evidence folder, for step 63.

## Step 40 — Chirikov standard map, the boss gate (macOS)

40. **The scaled standard map is two exact shears, and its inverse undoes
    the drift first.** `SUMI_DEFORM_CHIRIKOV` (`deform.glsl chirikov_fs`)
    is one stage of MEDIUM §2.5's kick-drift: the kick y₁ = y + A·sin(k(x −
    x_c) + φ), a y-shear; the drift x₁ = x + ε·(y₁ − y_c), an x-shear. In the
    torus variables X = kx, Y = kεy this is X′ = X + Y′, Y′ = Y + K sin X
    with **K = A·k·ε** the step's chaos parameter (Greene's threshold K_c ≈
    0.9716). The ε-scaled drift is the spec's: x₁ = x + y₁ on a non-wrapping
    canvas is a canvas-scale shear; ε keeps it a shear at usable sizes, and
    the kick amplitude follows as A = K/(k·ε). CLASS EXACT (det J = 1 at any
    K); the exact inverse is the drift undone first, then the kick (reversed
    order, negated — #17). Measured (`--chirikov-test`): one step at K = 0.5
    (a 41-texel kick amplitude) moves the central band's pre-image 32.9
    texel and the step then its inverse leaves 0.042 (smooth shears: the
    resampler's floor, ten times below the spark's kinks); the pass matches
    the closed form x = P.x − ε(P.y − y_c), y = P.y − A sin(k(x − x_c) + φ)
    to 0.12 texel. Headless: the inverse to 10⁻¹⁶, det J = 1 to 10⁻¹⁰, the
    same-order sign flip a residue of 0.16. `sumi_add_chirikov(x, y, K,
    periods, ε, φ)` is one full step as a gesture (k = 2π·periods per canvas
    height along x; K < 0 the exact inverse; |K| clamped at the gesture
    ceiling `SUMI_CHIRIKOV_K_GESTURE_MAX` = 2, where the medium stops
    rendering the map at all — the sweep, #42).

41. **"Delta-driven K" resolved: a throw of δ is one step at δ²·K_max, and
    the wheel down retraces.** The spec wants K from the mod wheel / breath
    as deltas, never absolutes. A step with the kick scaled by δ and the
    drift left whole is not a delta — a wheel at rest would still shear the
    canvas every frame — so both shears scale with δ: the identity at δ = 0,
    the full map at δ = 1, and the step's chaos parameter δ²·K_max
    (`params.chirikov_kmax`, 0..2, default 1). The consequence is the
    instrument's: a wheel eased over m frames is m steps at K_max/m² — the
    kick-drift splitting of the PENDULUM flow, integrable, smooth sheets —
    while a wheel THROWN is one hard kick, chaos. The depth into chaos is
    the wheel's speed, and the smoother (`smoothing_ms`) is the first cap
    on it. The route (`SUMI_CTL_CHIRIKOV_K`, 19; COUNT 20; unmapped in the
    core, CC 109 in the desktop stock map v7, the mod wheel under the Anod
    table at step 42) keeps a delta tracker like the pinch's; a throw below
    0.02 of the range accumulates. A NEGATIVE δ applies the exact inverse
    step, so the wheel down undoes the wheel up step for step (headless: a
    one-frame throw at K_max 2 is one step at the ceiling then the
    remainder 0.17, then nothing; at K_max 0.5 one step at 0.5 and, on the
    way down, one inverse step, drift first). On the GPU a throw of the
    control moves the band 42.6 texel through the smoother's run of gentle
    steps and the control home retraces to 2.86 — the second-order residue
    of steps that commute only to first order. The map is centred where the
    vortex is (the VORTEX_X/Y ctls): the same hand steers, and the Anod
    table gives the mod wheel to the map where Sumi gave it to the vortex.
    `params.chirikov_periods` (1..8, default 2) and `params.chirikov_eps`
    (0.05..1, default 0.5) are the geometry.

42. **The boss gate: the erosion sweep, its table, and the ceiling.**
    `--soak chirikov-sweep`: one full step of the map every other frame
    for the gate's 6000-frame window (one pass a frame, the tine control's
    density), the gate's scene and voice, per K ∈ {0.25, 0.5, 0.75, 0.9716,
    1.25, 1.5, 2.0}; periods 2, ε 0.5, the centre (0.5, 0.5). **Read over
    the whole window, (d) is RED at every K, including 0.25 — and the
    checkpoints say why:** 45–60% of the scene's ink is gone by frame 1000
    at every K, then the mass settles. That is not resampling erosion. The
    torus wraps and the canvas does not (#43): every rotating orbit
    advances in x by its momentum each step and marches off the side — at
    |y − y_c| = 0.2, the scene's reach, 51 texels a step — so the drift
    flushes the scene's rotating material in the first few hundred steps,
    at ANY K, and what stays librates or sits near the centre line. **Read
    after the flush** (frame 1000 as the base), the erosion of what stays
    is the gate's business, and it is monotone in chaos above threshold:

    | K per step | flushed by 1000 | erosion/pass after the flush | × tine | (d) |
    |---|---|---|---|---|
    | 0.25 | 58% | 2.2·10⁻⁵ | ×1.88 | green — a small separatrix (Y-reach 2√K = 1): little librates, the rest still drifts slowly and leaks |
    | 0.5 | 47% | 1.2·10⁻⁵ | ×1.02 | green |
    | 0.75 | 42% | 1.2·10⁻⁵ | ×1.00 | green |
    | 0.9716 | 44% | 1.7·10⁻⁵ | ×1.50 | green — Greene's threshold |
    | 1.25 | 52% | 2.0·10⁻⁵ | ×1.78 | green |
    | 1.5 | 57% | 3.4·10⁻⁵ | ×2.97 | RED |
    | 2.0 | 69% | 3.4·10⁻⁵ | ×2.94 | RED |

    (the glide-tine control 1.15·10⁻⁵/pass; the visible ink kept in the
    central rows 67 / 96 / 109 / 102 / 87 / 56 / 29 %, the boundary length
    ×1.2–1.8 then ×0.7 at K = 2 — the filaments finer than a texel average
    into gray). **The ceiling:** `SUMI_CHIRIKOV_K_CEIL` = 1.25 — the last
    tabulated K where (d) holds after the flush, above Greene's threshold,
    so a hard throw reaches chaos; the gesture's hard limit 2.0. **The
    author's call (MEDIUM §2.5's [ITERATE]), with the numbers:** (i) keep
    1.25 — a throw crosses into chaos, the medium erodes what stays at
    under twice the tine, the flush is accepted as the operator's
    geometry; (ii) K_c = 0.9716 — the transition itself is the ceiling, ×1.5;
    (iii) 0.75 — the tine's own rate, sheets only; (iv) a drift profile
    bounded away from the centre line (an exact shear still, the standard
    map near y_c, the far rows no longer marching) would end the flush at
    the price of the textbook map's geometry — a design change for the
    author, not made here. The whole-window (d) is recorded RED for every K
    in the log, on purpose: the gate is not bent, the reading is
    explained.

43. **The torus wraps, the canvas does not — what the KAM transition looks
    like on a sheet.** Every rotating orbit of the standard map advances in
    X by its Y each step; on a torus that is motion around the cell, on a
    canvas it is a march to the side edge and out (the drift at |y − y_c| =
    0.225 — the separatrix's reach at K = 0.5 — is 58 texels a step). Only
    librating material, inside the island, stays for good. Measured after
    60 steps (`--chirikov-test`): rings centred on the hyperbolic point
    keep 53% of their ink mass at K = 0.5 — the librating half stays, the
    rotating half streams away along smooth sheets — and 16% at K = 1.5,
    where the chaotic sea flushes the inside as well; rings on the elliptic
    island keep 101% at K = 1.5. And a lesson about the separatrix itself:
    material ON the hyperbolic point stretches exponentially at ANY K
    (the point is hyperbolic below threshold too) and dissolves into gray —
    17% of the visible area left at K = 0.5, 0% at 1.5 — so "smooth
    sheets" are what lies AWAY from the separatrix, and a boundary-length
    or visible-area reading of rings on the fixed point is the separatrix's
    signature, not the transition's. The witness that survives on a canvas
    is what stays: the mass kept. The scene `chirikov` shows exactly this —
    rings at the hyperbolic point and on the island, the same step
    iterated, K on a slider through 0.9716.

44. **The scene, the bench, the settings.** Scene `chirikov` (the marble
    app and the gate's sweep, not the docs' check — #22): K per step,
    periods, ε and the iteration count; the web host gained three parameter
    ids and the `chirikov` cwrap. Desktop: `--chirikov-test` (4 checks),
    `chirikov` in the conservation gate through its delta route — green:
    500 pairs of a K = 0.5 step and its exact inverse hold mass −0.11% /
    +2.66% with a pre-image dev of 6.06 texel (the torsion's oscillatory
    family, under the 8-texel bar), the wobbling wheel's stream grows
    +0.40% (6.7·10⁻⁷/pass) and erodes nothing (−6.8·10⁻⁷/pass, the medium
    gaining): the route's deltas are gentle steps of an integrable flow, and
    chaos is reached only by throwing, each throw's step capped at the
    ceiling — `--soak chirikov-sweep` for the table, the I key (one full step at the cursor at
    K max; Shift+I its inverse), a "Chirikov" settings section (K max,
    periods, drift, and the throw as a CC slider on the CC 109 route — its
    changes ARE the throws), INI keys, the name "Chirikov throw". Page
    draft `chirikov.mdx` in the evidence folder, for step 63.

## Step 41 — The ABI event: libsumi 1.0.0 (macOS; the Mac compiles iOS in-step)

45. **The one break, and why the probe's state ships before any stateful
    layout.** `sumi_version` reads **1.0.0**. Five things changed in one
    bump, and the header carries the migration note above `sumi_version`:
    (1) `sumi_layout_probe` gained `const sumi_layout_state_t* state` after
    `aspect` — INSTRUMENT §1's struct verbatim (`buttons`, `slider`,
    `reserved[2]`, 16 bytes), NULL or zeros = stateless, which every layout
    shipping today is; (2) `sumi_cell_info_t` gained `flags` at its end
    (`SUMI_CELL_CONTINUOUS` = bit 0, the theremin's; 0 today) — the
    INSTRUMENT `[ITERATE: sentinel vs flags]` resolved as flags, as the
    roadmap fixed; (3) `sumi_params_t` gained `medium` at its end
    (`SUMI_MEDIUM_SUMI` 0, `SUMI_MEDIUM_ANOD` 1); (4) `sumi_set_palette` and
    `SUMI_PALETTE_CUSTOM` (#46); (5) `sumi_layout_t` 8..12 named and
    RESERVED — TRUMPET, TROMBONE, WICKI, FRETS, THEREMIN, the INSTRUMENT
    spec's names. The probe's state and the cell's flags land two phases
    before Phase 8 uses them because the arc allows ONE break (#3): a
    stateful layout added later would otherwise force a second signature
    change on every shell, and the cost of carrying an unused pointer and an
    unused word until then is nil. Every call site moved mechanically — the
    desktop bench, the headless suite (25 calls), the C11 ABI test, the web
    shim, the iOS overlay (`nil`), the Android JNI (`nullptr`; it compiles on
    the Linux box as the first line of step 45) — and `hostmpe` needed
    nothing (it never probes). From here on, additive growth only.

46. **The palette POD, and its first consumer.** `sumi_palette_t` is QOL
    §1's model as data: 2..8 stops of linear RGB at ascending positions
    along the ink-depth axis, a depth curve (γ and a floor: u = floor +
    (1 − floor)·depth^γ), a per-drop drift (the aux selector shifts the
    sampled position by ±drift/2 — the built-ins' hue drift, generalised),
    the clear-water band's tone, and four reserved words. `sumi_set_palette`
    validates on the way in — counts clamped, positions forced ascending,
    RGB and the curve clamped, NaNs zeroed — and stores; the composite
    reads it only when `active_palette_id` is `SUMI_PALETTE_CUSTOM` (3),
    through a branch that leaves the built-in path textually untouched:
    the same washi, the same soak, the same ink-thickness probe — the
    identity guardrail (the user chooses the hues, the medium keeps its
    character). A sumi-like two-stop palette stands in until a host sets
    one. Measured (`--palette-test`): a red-to-black palette recolours the
    inked texels and not one paper texel, and palette 0 afterwards prints
    bitwise as before. NOT done here, on purpose: QOL's "the built-ins
    become presets in the same model, one code path" — that unification
    would touch the built-in arithmetic, and the composite gate (#48) holds
    it to bitwise; it belongs with the palette editor (step 46), where the
    presets are written out in the model and the gate is the guard. The
    Anod medium will read the same POD as a glow (step 43).

47. **`medium` is inert until the Anod composite; the reserved values are
    clamped, with a warning.** `sumi_set_params` clamps `medium` above ANOD
    to SUMI, `pitch_layout` at or above TRUMPET to FIFTHS with a WARN log
    (the reserved layouts are refused by the probe as well), and
    `active_palette_id` above CUSTOM to 0. Medium 1 renders exactly as
    medium 0 until step 43 lands its composite — recorded so no one reads
    the switch as broken. The desktop persists `medium` in the INI and
    shows no switch yet (the switch is step 43's UI); the web host gained
    the parameter id and exports `_sumi_set_palette` without a JS surface
    until the editor.

48. **The composite screenshot gate — the print of the canonical script as
    a fixture.** `midi-sink --dev --composite-dump <file>` runs the §4.6
    field script on the 512² scripted clock, dips, and writes the print
    (RGBA8, top-left origin on every backend); `tools/composite_gate.py`
    compares it bitwise against `tests/fixtures/composite_512_metal.rgba`
    and proves red on a corrupted copy, as the field gate does. The fixture
    was generated from the PRE-BREAK renderer (0.14.0) before any header
    changed, two runs bitwise identical (the print has no time-dependent
    input: the dip fade is 0 on the print path, the live ripple off, the
    grain a hash of position). After the break the 1.0.0 print is bitwise
    the fixture — with the composite shader carrying the new uniforms and
    the custom branch — so "medium 0 renders as before" is proved for the
    pixels, not only the field. The roadmap's "bitwise as 0.9.0" is read as
    "as the pre-break renderer": 0.9.0 was the last version when the
    roadmap was written; steps 36–40 grew it additively to 0.14.0, and
    every one of those was gated bitwise on the field. The gate joins the
    release spine's Metal gates alongside the field gate.

49. **What the shells did in-step, and what waits.** The Mac compiled the
    iOS shell against the 1.0.0 header (`xcodebuild … BUILD SUCCEEDED`) and
    its libsumi; the wasm rebuilt and passed the web field gate and the
    16-scene sweep; the Android JNI was edited mechanically and compiles at
    step 45's first line, on device — main is never red between 41 and 45
    by that verification, as the roadmap asks. The About strings read
    `libsumi 1.0.0` everywhere through `sumi_version`.

## Step 42 — The Anod medium (macOS)

50. **The composite branches per medium, and the Sumi branch is the 1.0.0
    print, bitwise.** `params.medium` reaches the composite as a uniform;
    medium 1 takes `anod_col`, medium 0 the 1.0.0 path wrapped untouched in
    an `else` — the composite gate (#48) reads max channel diff 0 against
    the pre-break fixture with the Anod branch and its uniforms beside it.
    Anod reads the field two ways. CHARGED material (phase ≥ 1, the ink
    re-read) glows by its STRAIN: the stored source coordinates of the
    stencil's neighbours give J (one-sided differences, the smaller kept —
    #56), and for an area-preserving map ‖J‖_F² − 2 = (λ − 1/λ)² =: σ² —
    zero for the identity and for pure rotation, positive wherever the
    sheet was stretched; the glow is 0.22 + 0.78·(1 − e^{−σ/anod_glow})
    (`params.anod_glow`, 0.2..5, default 1: the strain that glows; smaller
    is hotter), the base making a fresh, unstrained strike visible as
    charge. The charge phase bands the filament between the palette's core
    and halo by parity, aux drifts the hue per event as the ink's hue drift
    did (0.45·hue_t). WATER never glows by strain — it draws the field's
    deformed grid (#57). The substrate is near-black glass (0.010, 0.010,
    0.014 linear) with the washi's own simplex grain as a phosphor speckle
    at `paper_roughness` strength, sampled at st — SPEC §4.5's screen-locked
    invariant, composite side; the darkness and grain knobs are step 43's.
    The three Anod palettes — electric blue / violet, plasma orange,
    phosphor green — sit under the SAME ids 0..2 the sumi palettes use
    ("continuing the palette ids" read as the medium re-reading them: the
    id is the player's choice, the medium its family) and morph on the same
    ring; the custom palette (3) is read as a glow, the gradient sampled by
    g. The dip's "lift the paper" flash is shared: in Anod it is the
    photograph's flash. Measured (`--anod-test`, 9/9): the identity field
    prints the substrate alone at 512² and at 1920×1080 (mean 27.5/255, max
    29.5); the §4.6 script's 41 169 charged texels sit at 111.1 and their
    luminance correlates 0.99 with the CPU's 0.22 + 0.78(1 − e^{−σ}) read
    off the same field; a lone drop's interior carries the base glow (91.0)
    and the water round it the grid. The roadmap's "every ring boundary
    glows" resolved the other way: a drop's rim compression is WATER strain
    and water shows lines, not glow — the author's call of 2026-09-22 after
    seeing both. MEDIUM §3's "live insertion point" (torsion sweeps and
    Chladni quadrature riding the composite) is superseded: neither exists
    since #24 and #20 — both bake.

51. **The binding tables, as a resolution rule, and what the medium decides
    outright.** MEDIUM §4 ships as `eff_modes`: a mode set to
    `SUMI_MODE_MEDIUM_DEFAULT` (255, the new default of `bend_mode`,
    `slide_mode`, `press_mode`) resolves to the medium's column — Sumi 0/0/0
    (the 0.x behaviour, unchanged), Anod 2/2/2 — and an explicit mode is the
    user's override, exactly as today. New mode values, additive: `bend_mode`
    2 = the torsion's wavenumber and 3 = the spark's (±1.5 semitones span
    the ctl, the ripple law's reach, #66; last writer wins), `press_mode` 2
    = the torsion sweep FEED (pressure adds to the same pending rotation the
    note-on sweep spends, 1.2 rad/s at full pressure, with its own phase
    clock; the ink feed and its episodes stay quiet). Three dimensions have
    no mode param and the MEDIUM decides them outright: the strike (Sumi the
    drop; Anod the spark composition — the drop, a burst of core = the drop
    radius with lobes along the note's pitch axis and its order from
    `params.burst_order_by_class[note % 12]` (2..8, 0 = `burst_order`;
    default naturals 2, accidentals 3 — the table the author signs by eye),
    and the spark shear episode — per echo), the poly-pressure dimension
    (Sumi the Lamb–Oseen swirl; Anod the Chladni stir: the loudest active
    voice's pressure sets `SUMI_CTL_CHLADNI_A` unless a CC is mapped to it —
    the CC map overrides) and the mod-wheel dimension (`SUMI_CTL_VORTEX_
    STRENGTH`: Sumi the vortex; Anod the Chirikov throw's source, the vortex
    quiet, the tracker re-baselined at a switch so the switch is not a
    throw; CC 109 stays the Sumi-side handle). The master bend keeps its
    shear tine in both media — the `[ITERATE: scroll-compatible shear]`
    closes as "the tine already composes with the scroll; nothing was
    needed". The press feed's `[ITERATE]` ships as the sweep feed and the
    hour of playing decides it. Headless (`test_medium_binding_tables`):
    Sumi as before; Anod: a C♯ strike lands the drop, four burst pieces of
    order 3 and the shear's first step in one frame, the bend moves
    TORSION_K by 0.5 and lays no tine, the slide sets SPARK_K to 0.945,
    forty frames of pressure spend 39 torsion passes and no drop, poly
    pressure sets the stir to 0.79 and emits 78 Chladni passes and no swirl,
    the wheel throws 2 Chirikov steps and no vortex; with the three modes
    overridden to 0 in Anod the bend, slide and pressure behave as Sumi's
    while the strike, the stir and the throw stay Anod's. The desktop's
    mode radios became combos with "Medium default" first; an INI written
    before 1.1.0 keeps its explicit 0s, which in Anod means the Sumi
    behaviour until the user picks the default — recorded, not migrated
    (a stored 0 may be a choice).

52. **The seam mask is found, not stored — and now guards the charge alone.**
    A scroll seam — fresh water beside displaced content — is a discontinuity
    of the map, and a finite-difference stencil straddling it reads a jump
    of the scroll's size as strain. Fresh water cannot be MARKED in the
    field (the ingress rule writes identity coordinates with zero phase and
    aux, and the §4.6 fixture pins those bytes), so the composite finds it
    by its class: a texel at its own identity coordinates, within one ULP,
    with no phase. A first rule for the inner seams — the staircase of N
    translated bands N scrolls leave, whose steps are not fresh — compared
    the two one-sided jumps (a translation has a side with nothing, strain
    has both) and is superseded by the estimator itself, which keeps the
    smaller one-sided difference per entry (#56) and so reads a step as no
    strain without a rule. Since water never glows by strain (#57), the mask
    matters only where a CHARGED texel meets fresh water and its stencil
    straddles the seam: such a texel reads no strain. The earlier
    seam-column measurements (27.5 against a 27.5 substrate) stand as the
    water's; the identity checks at 512² and 1920×1080 now carry the claim.

53. **Live switching is a feature.** MEDIUM §1's `[ITERATE: live switch or a
    forced dip?]` closes as live: the switch is a params write, the
    composite is a READ, and the field is bitwise across Sumi → Anod → Sumi
    → Anod (0 samples differ, measured). The evidence the roadmap asks for —
    the same recorded session re-read in both media — is `anod_reread_sumi.
    png` / `anod_reread_anod.png` in the step's folder: the §4.6 script
    printed under each medium from the same bytes. The dip stays one key
    away (9 on the bench, the settings' button) for whoever wants a fresh
    sheet between media. The desktop's "Medium" section (a radio and the glow
    scale) and the A key switch it; the web scene `anod` lays a session in
    Sumi and switches.

54. **Prints, and the questions carried.** A dip in Anod prints through the
    same readback with the medium's composite — "the photograph of the
    discharge" is the same machinery, medium-styled by construction; no
    print code changed. MEDIUM §3's `[ITERATE: long-exposure look? strain
    accumulation buffer?]` is CARRIED to the Phase-9 beta with its question:
    the field already accumulates the whole map, so a strain buffer would
    be a second history of the same thing; whether a print wants a time
    integral of the glow is a question for the eye after the hour of
    playing, not for a step. The `[ITERATE: substrate design]` resolves as
    the phosphor speckle above, its knobs at 43.

55. **What waits for the author, and what this step did not do.** The Anod
    column of the binding table is UNSIGNED until the hour on the ROLI Piano
    + Airwave (`binding_table.md` in the evidence folder lays the table and
    the ITERATE ledger out); the order-by-class table is a proposal; the
    drop-edge glow is by design until taste says otherwise; the burst's
    order "revisited by ear once Voxo lands" is Phase 7's. `sumi_version`
    → **1.1.0**, additive: `anod_glow`, `anod_pitch`, `burst_order_by_class
    [12]`, the mode values 2/3 and `SUMI_MODE_MEDIUM_DEFAULT` (and, from the
    Chladni rework before step 43, `chladni_mode` — #62). The tablet shells' ctl-name
    lists and mode pickers are still the 0.x lists — their steps (44, 45).

56. **The half-float field cannot be differentiated at screen resolution,
    and the estimator that reads the charge.** The coordinates are half
    floats: their spacing above 0.5 is 2⁻¹¹, a texel and a quarter of a
    2560-wide window, seven tenths of a texel of a 1440-high one. The 512²
    bench hides this — its identity coordinates are exactly representable —
    and the author's window did not: the right half of the identity field
    glowed in vertical stripes (the quantum beating against the texel grid,
    a plateau every four or five texels read as ∂u/∂x = 0), two sparks drew
    a giant X (the real strain of exact shear bands, which run the whole
    canvas by construction — MEDIUM 2.4 — not an artefact), and a burst's
    far field printed as concentric arcs (its stored coordinate advances by
    one quantum every few hundred texels, and a central difference across
    such a step reads a ring of false strain). What survives on charged
    material: the stencil widens with the field (2·round(H/512) texels a
    side — 2 at 512, 6 at 1440 — keeping the rounding a fixed fraction of
    the step); the fresh test tolerates one ULP; each entry of J takes the
    SMALLER of its two one-sided differences (a step, like a seam, has a
    side with nothing; strain has both; and a triangle's kink keeps its
    slope where a central difference would cancel it); and the expected
    rounding bias of ‖J‖_F² (the variance of two uniform ±ULP/2 errors per
    entry, three times for the tail) is subtracted. Measured: the identity
    at 1920×1080 prints 27.5 with its lower-right quarter at 27.5 (a first
    build read 40+ there); the charge's correlation with the CPU replica
    rose from 0.86 to 0.99. What does not survive: any reading of WATER by
    strain — a smooth displacement of a few texels over hundreds is below
    the staircase, whatever the stencil. Tried and rejected on the author's
    window, in order: water at 0.35·g and 0.12·g (arcs, stripes, the X);
    water as substrate (clean, "but it kills the far fields"); a knee under
    which nothing glows (hides the genuine faint field too); water glowing
    only within 0.06 of the charge, above a knee — the GAS (twelve taps on
    two rings; clean, liked, and "not what I was looking for": it loses the
    far field). The gas is kept as an idea for a future medium, not this
    one — the author's call of 2026-09-22. A higher-precision field
    (RGBA32F) would make strain readable on water and would break the phase
    invariant's fixture (DECISIONS_5 #12); not proposed for this phase.

57. **Water draws the field's deformed grid — the far field the author
    asked for, on purpose.** The picture the author liked in the failed
    strain builds — lines converging on a drop like a magnetic field,
    reaching the canvas edge — was the deformed grid drawn by accident: the
    half-float staircase draws the iso-lines of the source coordinates
    every quantum; on a 2560×1440 window the quantum (2048 per canvas
    height) beats against 2560 and 1440 texels into a grid of pitch ~5
    texels whose bending is the displacement amplified ~4×, negative along
    x and positive along y (2048 − 2560 < 0 < 2048 − 1440), which is why
    one family converged on the drop and the other bulged round it — with
    straight stripes wherever the water rested, the only thing the author
    disliked. Drawn deliberately: two families of iso-lines of (position +
    gain·displacement), gains −4 along x and +3.4 along y (the accident's
    signs, kept because the author chose the picture they made; both
    negative would converge both families), at a pitch of `anod_pitch`
    canvas heights at rest (#58), one to two texels wide. The displacement
    is a VALUE the field holds to a fraction of a texel — its derivative
    was the problem — and it is read from a window along each family's
    axis (3·round(H/512) texels a side: 7 taps at 512, 19 at 1440; charged
    texels and taps past the canvas edge left out), so the staircase's
    sawtooth averages out to a few percent of a quantum and the lines stay
    smooth where a raw contour of the displacement would wobble by tens of
    texels (tried: iso-lines of |d| and of its direction printed as a
    blocky wheel). Lines show only where the averaged displacement exceeds
    a texel, fading in to three, so rest is glass and the field reaches
    exactly as far as the operator does (a drop's a²/2r: a small drop's
    lines end where its displacement drops under half a texel, measured
    0.00% of the water beyond r = 0.15 lit for R = 0.02); a family fades
    where its local pitch falls under three texels (a drop's rim, a spark's
    core), so the grid never aliases. Under it: a drop's field converges,
    a burst's lobes and a spark's jagged shears draw their own, the exact
    shears' whole-canvas bands read as what they are. Measured (512², pitch
    10 texels): 42% of the script's displaced water lit, 40% of a lone
    drop's 1.5–2.4 R annulus, 38% of a small drop's 1.5–3 R. KNOWN, for the
    author: a uniform translation is a displacement, so on the rolls the
    scroll lights the whole grid as straight lines streaming at the gain
    times the scroll speed — the physics of the reading, and "Grid lines:
    Off" on a roll is the remedy until a scroll-relative reading is asked
    for.

58. **`anod_pitch`: the grid's pitch, and 0 is off.** The number of lines is
    the knob the author asked for: `params.anod_pitch`, the grid's pitch at
    rest as a fraction of the canvas height (1/256..1/8, default 1/144 — 10
    texels at 1440, the pitch the author called lovely), 0 = no grid,
    NaN and negatives landing on 0. A fraction of the height, not a texel
    count, so a print at a higher resolution shows the same lines as the
    screen. Measured: at 0 no water texel lights (0.00%) and the charge's
    mean luminance is unchanged (111.1 both ways) — the knob touches the
    water alone. The desktop shows it as "Grid lines", lines per canvas
    height on a logarithmic slider from Off to 256 (under 8 reads Off); the
    INI key is `anod_pitch`; the web host's param 32 and the `anod` scene's
    P slider carry it. The gains, the line brightness (0.14) and the alias
    pitch stay shader constants — step 43's knobs if taste asks.

59. **The plate guide, and what the lattice turned out to be.** Before step
    43 the author asked to see the layout the Chladni operator was stirring.
    A dev-only overlay (the bench's N key; `sumi_debug_set_chladni_overlay`
    behind a `dbg_lattice` uniform of the composite, 0 on every shipped path
    — the print fixture stays bitwise, measured at each build) draws the
    plate over the print. Drawn first as the lattice's nodal lines ψ = 0 and
    its eddy cores, it showed that the operator's lattice was a SECOND
    derivation of the layout — `sumi_layout_cell_lattice`, a rectangular
    pitch and phase — beside the probe the shells draw their keys from.
    Measured against those keys at the author's aspect: exact on the
    chromatic grid (12 × 7); on the Jankó every key centre on a node but two
    eddies per key across (the half-column pitch its stagger forced); on the
    piano grid the accidentals on the nodes and the naturals a tenth of a row
    off (the two families are 0.9 rows apart, #61's tenth, and no lattice
    holds both); and on the fifths and the four rolls — which draw NO keys:
    the probe declines, Play mode is meaningless there — an invented square
    grid, the fifths' 0.032 ring spacing laid as some 1730 cells over a
    radial layout of 132. The author's finding, twice over: "you are using a
    grid, not the cells", and the cells meant are the DISPLAY cells — the
    circles the shells draw, the probe's centre and cell_radius swept and
    deduped (PlayOverlayView.rebuildLatticeIfNeeded, sumi_play.cpp). The
    guide now draws exactly those, from the same enumeration the operator
    uses (#60): naturals cyan and accidentals amber as the shells colour
    them, the odd cells (#60's checkerboard) with a fuller face. A first
    draft capped the list at 128 and showed "a little more than half" of the
    Jankó's keys — its six rows of forty-two, three echo rows a note, are
    252 cells; the cap is 320 (`SUMI_LAYOUT_MAX_CELLS`).

60. **The cells are the eddies: the Chladni stir rewritten without a
    lattice.** The author's call, 2026-09-22: "remove the lattice and put an
    eddy in each circle". `sumi_layout_cells` (layouts.cpp) is the one source
    of cells for the operator and the guide: the three key layouts' from the
    probe at every note position (the circles the shells draw), the fifths
    and the rolls the largest circle at each note that touches no
    neighbour's — half an octave ring (128 discs of radius 0.016 on the
    twelve spokes between r = 0.10 and 0.42) and half a semitone (128 discs
    of radius 0.0034 on the now-line). Each cell carries its family
    (accidental) and its parity on the layout's own checkerboard (chromatic
    grid pitch class + row, Jankó column + row, piano grid white-key index +
    octave, fifths index + octave, rolls note), so neighbours differ. THE
    PASS (`SUMI_DEFORM_CELLS`, deform.glsl `cells_fs`): the display discs are
    disjoint, so one pass turns every disc about its own centre — a texel at
    ρ = r/R by θ·(4ρ²(1 − ρ²))², a RING: zero at the centre, full at ρ =
    1/√2, zero at the rim with zero slope — and a texel in no disc stays. A
    rotation preserves r, so det J = 1 inside every disc and the map is
    continuous across the touching rims: CLASS EXACT, the inverse the
    negative pass; the note's drop rests where it fell and is wound from its
    edge, the water between the keys never moves, and the ink sheared at
    each rim is the figure — it outlines the keys themselves. Which disc a
    texel lies in is an INDEX MAP the renderer rasterizes from the cell table
    at the field's resolution (RGBA16F, up to four owners a texel for #62,
    −1 for none; rebuilt on a cells change or a resize) — one texture read a
    texel whatever the layout, which is why the Jankó's 252 cells cost what
    the grid's 84 do. The ring profile came from the author's "around the
    cell, not inside" (first drafted as a core bump (1 − ρ²)²); it also
    carries the displacement where it is largest (0.727·θ·R at ρ = √(5/9)
    against the bump's 0.286), which is #61's clock. `SUMI_CTL_CHLADNI_B`
    sets the odd cells' sense — 0 neighbours counter-rotate, ½ every other
    cell rests, 1 all turn the same way (2B − 1) — and `chladni_cell` scales
    the disc about its centre, capped at 1 in this mode: above 1 two discs
    would overlap, a texel could follow only one centre, and the cut between
    them would tear the field along a chord (the other way to grow is #62).
    The two-wave gesture `sumi_add_chladni` keeps its lattice (ABI
    unchanged): its emission was generalised on the way to any Bravais basis
    — the waves are the dual basis of (a1 + a2, a1 − a2), so a rectangular
    pair is Taylor–Green's cos(kx)cos(ky) and an oblique one a staggered
    lattice — and that form stays for the gesture and the web scene. TRIED
    AND REMOVED the same day, at the call above: a lattice FITTED to the
    probe's cells (Cartesian, with the Jankó's stagger as an oblique lattice
    and the piano grid's eddies midway between its two key families) and a
    POLAR product flow for the fifths (rings × sectors, sub-stepped, since
    exactness needs the wave arguments linear in (θ, r²/2) and octave rings
    are equal in r) — both worked and neither was the cells. SPEC: MEDIUM
    §2.2 describes a kick-drift lattice whose k_x : k_y come from the chord's
    intervals; step 37's review call made the layout the plate (#17–#22);
    this entry makes the keys the eddies. The spec and the entries disagree;
    the entry is what shipped, the spec is the author's to revise. Measured
    (`--chladni-test`, 7/7, the chromatic grid at 512² unless said): after
    150 stirred frames the 84 disc cores (ρ = 0.15) moved 0.00 texel and the
    66 corners 0.00 while the discs' rings (ρ = 0.745) moved 15.95; a ring
    at ρ = 0.7 round a centre rotates 1.45 rad and stretches |log r′/r| =
    0.09, a ring round a corner rotates 0.00 and stretches 0.00; the 84
    discs are the probe's 84 keys, same centres and radius 0.0350; cell size
    0.5 halves the radius and 1.5 leaves it; on a 16:9 field cores 0.19 and
    corners 0.19 against rings 5.44, rings rotating 1.07 rad. The exact soak
    (`--soak chladni`, 3/3): 500 (+θ, −θ) pairs hold mass −0.22 %/+1.21 %
    with a 5.16-texel pre-image drift, fabrication 0.00 % over 6000 passes,
    erosion 1.56·10⁻⁷/pass — a hundredth of the tine control. The desktop's
    "Cell size" and "Balance" help, the soak table's `chladni` row and the
    binding-table test (which counts the cells pass as the stir) follow.

61. **The emission floor, a third time.** On a fresh sheet the first disc
    build turned a ring 0.07 rad in 150 frames where the rate said 1.9: a
    frame's rotation at 1.5 rad/s moved the fastest texel of a disc by a
    quarter of the coordinate quantum, every coordinate sat exactly on the
    half-float grid, and every pass rounded straight back — nothing
    accumulated (where a drop's compression had put the values off the grid
    the rounding dithered, which is why curls showed on the scripted field
    and not on the identity). The same lesson as the burst's floor (#31)
    and the Anod water's staircase (#56): the stir now BANKS its rotation
    (`cells_pending`) and emits one pass when it carries at least
    `SUMI_CELLS_MIN_EMIT` = 10⁻³ canvas heights of peak displacement on the
    smallest disc — two quanta of the top half of the coordinate range —
    θ_pass = 10⁻³/(0.727·r_min), the engine handing r_min over with the
    cells; a remainder left when the stir stops is let go (it could not be
    applied). The stir stays exact; only its clock coarsens: at full stir
    on a 1440-high canvas a pass every frame on the chromatic grid (0.024
    rad), about every 50 ms on the Jankó (0.075) and every 60 ms on the
    fifths (0.087), while the rolls' five-texel discs would need about a
    radian a pass — the stir is not meaningful there. The ring profile's
    2.5× larger reach is what brought the Jankó from 8 visible steps a
    second to 20; the only true remedy is a 32-bit coordinate field, the
    quality flag the renderer already anticipates, which would break the
    §4.6 fixture unless gated — a roadmap decision, not this step's.
    Measured with the floor: the ring at ρ = 0.7 rotates 1.45 rad, the
    cores rest (#60).

62. **`chladni_mode`: SUMI_CHLADNI_FIELD, the author's "inverse Chladni".**
    Asked for as a second effect beside the exact discs: the rings of every
    disc covering a texel SUMMED into one displacement, d = Σ θ·w·bump·
    (−rel.y, rel.x). A radial swirl is divergence-free for any profile, so
    the sum is, and the finite step applies it to first order — the
    burst's class, SUB-STEPPED — which is what lets the discs grow past their
    keys (`chladni_cell` to 1.5) and OVERLAP, the water between the keys
    stirred by both neighbours: the "burst-like" version, switchable against
    the exact one and off by default. The index map carries four owners a
    texel for it (a corner at 1.5 lies in four discs; a fifth is dropped).
    `params.chladni_mode` (0 SUMI_CHLADNI_DISCS, 1 SUMI_CHLADNI_FIELD) is
    additive to 1.1.0; the desktop's Chladni section gains a "Mode" combo
    and the INI the key, the web host param 33; `sumi_debug_add_cells_pass`
    pushes one pass for the gate. THE BUDGET: a pass of θ on a disc of
    radius R has |∇d| ≈ 4.6·θ where the ring is steepest, twice that where
    two rings overlap, so the class budget |∇d| ≤ 0.25 wants θ ≤ 0.027 —
    which the floor (#61) meets on the chromatic and piano grids at 1440
    (0.016–0.024) and exceeds two to three times on the Jankó and the
    fifths (0.075, 0.087): there the mode runs over budget, which shows as
    fabrication, not folds, and the soak is the judge. Gated (`--soak
    chladni-field`, 3/3, the chromatic grid at 1.5): one budgeted pass of
    0.025 rad has pre-image det min 0.855 and mean 1.00000; 500 (+θ, −θ)
    pairs hold mass −0.42 %/+0.17 % with a 0.75-texel drift (informational
    for the class); fabrication 2.7·10⁻⁶/pass (+1.64 % over 6000; under
    5·10⁻⁵); erosion −2.73·10⁻⁶/pass against the tine control's
    1.15·10⁻⁵ — the mass grows slightly rather than fades. Kept as a mode
    and not a medium: the Anod gas (#56) waits for a medium of its own; this
    is the same plate stirred another way.

63. **One palette path, and the built-ins bitwise through it.** Step 43's
    first brick (QOL §1, ROADMAP_5's "one code path, bitwise-checked"). The
    composite no longer carries per-id colour tables: every palette — the
    medium's three built-ins, the curated presets, the custom slot — is one
    `sumi_palette_t`, and `pal_ink_at(depth, hue_t)` = mix(gradient(depth),
    accent, drift·hue_t) is the only colour path, for both media (Anod: the
    gradient sampled by strain is the charge's core, the accent its halo,
    the charge phase banding the filament between them — the step-42 tables
    as presets). What made the built-ins EXPRESSIBLE was one change to the
    model (#46): `hue_drift` is now the built-ins' own per-drop drift — the
    aux selector blends the sampled colour toward `accent_rgb` (new, from
    three of the four reserved words; the POD's size is unchanged at 172
    bytes) by drift·hue_t — where step 41 had it shift the sampled position
    along the gradient. A built-in is then a two-stop palette of one colour
    (the 0.x literals verbatim, `palettes.cpp`) drifting 0.45 toward its
    accent, and its arithmetic through the one path is the legacy's in the
    same order: mix(ink, ink, t) is ink exactly, mix(stopA, stopB, m) is what
    mix(pal_ink(id0), pal_ink(id1), m) was, 0.45·hue_t is 0.45·hue_t. The
    ring moved to the CPU (`sumi_palette_ring`): the engine hands the
    composite two slots, A and B, and the blend, computed with the shader's
    own float operations (t = clamp(morph)·2, ⌊t⌋ capped at 1, t − ⌊t⌋), so
    a rest position is bitwise and a morph position is too. Measured
    (`--palette-test`): the §4.6 script printed under all six built-ins of
    both media at rest and under a morph of 38/127, hashed (FNV-1a 64 over
    the RGBA8 print) and compared with hashes captured from the legacy
    tables the day before the change — eight of eight equal; the composite
    gate (#48) max diff 0; the Anod test's hue check unchanged. QOL's
    `[ITERATE: curve fully fixed per medium, or an "advanced" fold?]`
    resolves as FIXED in 2.0: the depth curve (γ, floor) is the palette's,
    the washi's soak and the strain glow are the medium's and no palette
    touches them — the identity guardrail as code; the fold is deferred.
    `[ITERATE: per-drop hue drift as a palette field or global?]` resolves as
    a palette field (drift and its target), which the built-ins needed.

64. **The preset library, and the ring with a custom slot.** The library
    lives in the core — the identity statement is the medium's, not a
    shell's: `sumi_palette_preset_count(medium)` and `sumi_palette_preset(
    medium, i, out, name)`, pure and instance-free. Indices 0..2 are the
    medium's built-ins (the ids `active_palette_id` names); from 3, curated
    additions a shell offers as starting points for the custom slot — a
    PROPOSAL the author signs by eye, as the order table: "Cobalt & amber"
    (the Okabe–Ito blue/orange pair, safe under deuteranopia and
    protanopia, as ink and drift, and as charge and halo), "Viridis" and
    "Cividis" as perceptual ramps — thin ink bright to pooled ink dark, dim
    charge violet to burning charge yellow. Six a medium. `sumi_get_palette`
    returns the stored custom slot (a shell's editor round-trips through
    the core's validation; measured: all twelve presets return byte-equal).
    THE RING WITH A CUSTOM SLOT (the roadmap's open decision): the custom
    slot joins the ring only while it is active — a built-in active id
    travels the medium's three built-ins as since 0.x (no change of feel for
    a mapped morph CC), the custom slot travels custom → 0 → 1 → 2 over the
    same CC span (t = clamp(morph)·3). Headless (`test_palette_presets_and_
    ring`): the ring's pairs and blends at 0, ¼, ½, ¾, 1 for every active
    id, the twelve presets ascending and in range, indigo's literals.

65. **The palette editor on the desktop, and what waits.** The settings
    window gains a "Palette" section: the active slot (the medium's three
    built-ins and Custom), the library with "Load into custom", and — while
    the custom slot is active — the editor: the stops as sRGB pickers over
    the linear model (2..8, "+ stop" inserts before the pooled stop midway,
    "− stop" removes the one before it; the first and last positions are
    pinned at 0 and 1, the middle ones slide), the depth curve and floor,
    the hue drift and its target, the clear-water tone (Sumi). Live: every
    change writes the slot to the core through `app_settings_apply`, which
    now carries the palette beside the params; the INI persists it
    (`pal_count`, `pal_stop_i` as "r g b position" in linear RGB,
    `pal_gamma`, `pal_floor`, `pal_drift`, `pal_accent`, `pal_clear`), the
    "palette" key accepts 3, the 7 key cycles four. The custom slot starts
    as Sumi black. The web host accepts id 3 (its editor is step 46);
    `_sumi_get_palette`, `_sumi_palette_preset_count` and `_sumi_palette_
    preset` are exported for it. `sumi_version` stays 1.1.0, additive:
    `accent_rgb`, `sumi_get_palette`, the two library calls. NOT here:
    substrate (QOL §2), presets and the serializer (§3), prints and the
    ledger (§4) — the rest of step 43, next.

66. **The substrate's knobs, composite-side and bitwise at their defaults.**
    QOL §2 as four additive params: `paper_tint[3]` (the washi's base tone,
    linear RGB, default the 0.x cream 0.900/0.868/0.790 verbatim),
    `fiber_scale` (the strands' spatial frequency as a multiple of 0.x's,
    0.5..2, default 1 — it multiplies the strand and segment-mask
    frequencies, so at 1 the product is 0.x's exactly), `anod_dark` (the
    glass's darkness, 0..1, default 0.5 = the step-42 glass: the base
    0.010/0.010/0.014 times 2·(1 − dark), which at 0.5 is times 1.0) and
    `anod_grain` (the phosphor speckle's strength, 0..1, default 0.5). Each
    replaces a literal with a uniform of the same float or enters as a
    multiplication by exactly 1.0, so the Sumi composite gate stays max diff
    0 and the eight built-in hashes (#63) unchanged — measured. Two calls
    inside: the Anod speckle no longer rides `paper_roughness` and its CC —
    MEDIUM §3 lists darkness and grain as the Anod substrate's own knobs,
    and a live roughness controller had no business in the glass; and QOL's
    `[ITERATE: expose fiber angle-drift amount?]` resolves as NO — the ±20°
    drift is the washi's identity, like the soak. All of it samples in
    screen space, so §4.5's screen-locked invariant holds by construction
    (the composite is the only reader). The desktop's "Substrate" section
    shows the medium's knobs: Sumi a tint (Cream / White / Toned as
    presets — 0.955/0.950/0.935 and 0.760/0.690/0.560 linear — or a
    picker), a paper preset (Smooth 0.25 roughness × 1.4 fibers, Washi 0.5 ×
    1.0, Coarse 0.8 × 0.7) over the roughness and fiber sliders; Anod glass
    darkness and phosphor grain. The presets are value tables the shells
    copy (44–46), not core data — there is nothing to render bitwise in
    them. INI keys `paper_tint` ("r g b"), `fiber_scale`, `anod_dark`,
    `anod_grain`; web params 34..39. Measured (`--palette-test`, the
    substrate check): the identity sheet's mean luminance 236.1 under the
    cream and 245.6 under the white tint; fibers ×2 keep the mean (236.1)
    and the texture (spread 10); the Anod glass 27.5 at 0.5 and 0.0 at
    darkness 1; the speckle's spread 4 → 0 at grain 0.

67. **Presets: one serializer, pure C, beside hostmpe.** QOL §3 as
    `presets/` — `sumi_presets`, a C11 static library with no dependency
    but libc and the core's header for its struct types: `sumi_preset_t`
    (the schema and the writer's `sumi_version`, a name, the input dialect,
    `sumi_params_t` whole, the custom `sumi_palette_t`, the CC map as
    (channel, cc, target) up to 64, the routed controls' values as (ctl,
    value) up to 32, the control strip's two latch-wheel CCs, the
    `sumi_layout_state_t` defaults), `sumi_preset_init` (zero, then the
    caller's defaults), `sumi_preset_write` (JSON, snprintf's size contract,
    always NUL-terminated), `sumi_preset_read` (a single-pass recursive
    descent over the text, no allocation, no DOM) and, in its own
    translation unit so a serializer-only consumer never links the core,
    `sumi_preset_apply` (params, input mode, palette, the CC map cleared and
    remapped; the controls, the strip and the layout state are the host's
    to send). THE SCHEMA RULE (`presets/SCHEMA.md`, the document the
    shells build from): a file carries `midi_sink_preset` (1) and the
    `sumi_version` that wrote it; a reader ignores keys it does not know,
    keeps its defaults for keys it lacks, drops an array's extra elements
    and keeps the rest of a short one; it refuses — leaving the target
    untouched — only text that is not a JSON object; the core clamps on
    apply, so a hand-edited file cannot wound the engine. Params are written
    by their C names from one field table (offsetof; the table is what the
    schema document is written from), floats as %.9g so they round-trip
    exactly, `\u00XX` for control characters, a `\u` beyond ASCII decoded
    as `?`. Headless (`preset_tests`, strict C11, 30 checks): every field
    survives the round trip byte for byte; a "newer" file with unknown keys
    at every level, a longer `paper_tint` and a shorter `burst_order_by_
    class` loads with what both understand; malformed, truncated, non-object
    and empty inputs are refused with the target intact; exponents,
    negatives, whitespace, an empty map, a clamped control value; the size
    contract with a buffer too small. The desktop: the session is written as
    `<config>/last_session.json` on every save and read BEFORE the INI at
    launch (the INI keeps the app's own flags — window, print folder, the
    hint — and, on the first launch after the upgrade, still supplies the
    whole legacy settings once); named presets are `<config>/presets/
    <name>.json`; a "Presets" section lists them with Load / Save as /
    Delete and exports or imports the same file by path (the harness has no
    file dialog). The harness's routed values (ripple, Chladni, spark,
    Chirikov CCs) travel as the `controls` list; a tablet's strip values
    will use the same list, its wheel assignments the `strip` object.
    QOL's `[ITERATE: preset-next from the strip?]` resolves as NOT in 2.0 —
    a performance feature for the instrument phase; the settings switch
    presets. `[ITERATE: schema versioning rule]` is the rule above.

68. **Prints at any size, and the ledger.** QOL §4. The field is
    resolution-independent by construction — every texel stores where its
    water came from — so a print at a target size is the composite over the
    SAME field at that size: `sumi_export_begin(inst, field, fw, fh, w, h,
    flags)` renders a field (a snapshot's data, or NULL for the field as it
    stands) into an RGBA8 target of w × h through the print pipeline,
    un-rippled like a dip, with the params and palette AS THEY STAND (the
    engine rebuilds the composite's visuals before the pass, so a shell may
    restore a dip's look and export in the same frame), and reads it back on
    the swapchain's one readback slot — asynchronous like the print, since a
    browser cannot block on a GPU map: `sumi_export_poll` returns idle, in
    flight, or done-and-copied; a dip, a field read and a second export are
    refused while one is in flight. `sumi_read_field` is public now (it was
    the §4.6 debug read): the field as it stands, RGBA16F, W × H × 8 bytes —
    what a shell keeps per dip. THE BOUND, stated honestly in the UI copy:
    detail below a field texel is interpolation; the true re-dip is replay
    (Phase 8). Measured (`--print-test`, 5/5): the export at the field's own
    size IS the dip's print — 0 of 1 048 576 bytes differ; the same field at
    4096 × 4096 box-averaged 8 × 8 lands within 0.88 counts of the 512
    print on average; a field kept before the dip re-exports after the dip
    bitwise as the live field did, at 512 and at 4k — the ledger's premise;
    a 9000-wide or a zero-height export and a second export in flight are
    refused. QOL's `[ITERATE: cap? 8k?]` resolves as 8192 a side
    (`SUMI_EXPORT_MAX_DIM`: a 268 MB target — the desktop's ceiling, a
    tablet shell will offer less). `[ITERATE: TIFF-16, demand-check first]`
    resolves as NOT NOW: no demand has been voiced, PNG is what the
    composite produces (RGBA8 through the print pipeline), and a 16-bit
    export would want a wider target — deferred, the check stays open.
    ANOD OVER ALPHA (`SUMI_EXPORT_ANOD_ALPHA`): the composite gains an
    `alpha_out` uniform (in the block's padding slot, 0 on every shipped
    path — the Sumi gate max diff 0 and the eight palette hashes unchanged);
    the Anod branch hands out its lit colour and coverage beside the opaque
    result, and an alpha export writes straight colour over alpha — the
    charge's glow and the water's grid, the glass at alpha 0 (measured: the
    resting band at 0 in 768 of 768 texels, every one of 41 169 charged
    texels lit; the plain export opaque everywhere). A Sumi export is always
    opaque: paper. THE LEDGER (desktop, `print_ledger.cpp`): the product's
    dip — the settings button — keeps the field as it stands (`sumi_read_
    field`, a GPU copy of a few milliseconds), the params and the palette,
    then dips; when the dip's print lands it becomes the newest entry's
    print (the "Save last print" button's source now) and a box-averaged
    thumbnail, drawn in the settings window as a GL texture; every entry
    re-exports at Screen / 2K / 4K / 8K wide (the height by its aspect,
    capped), Anod entries optionally over alpha, to a PNG written on a
    background thread with the size in its name. Memory-capped at eight
    entries or 384 MB (a 1440-high field is 29.5 MB), the oldest evicted
    first; the newest alone keeps its full print. The bench's 9 key dips
    raw, outside the ledger. `_sumi_read_field`, `_sumi_export_begin` and
    `_sumi_export_poll` are exported for the web (step 46). ROADMAP_5's
    step-43 DONE line "the ledger re-exports a dip at 4k" is the third
    check above.

69. **The glow: a bloom after the Anod composite, and the author's look.**
    The author wanted the discharge to glow more and drew the target with a
    three-stroke matplotlib render (`specs/spark.py`: a wide faint cyan, a
    neon mid, a white-hot core). In the engine that is a screen-space BLOOM
    after the composite — the same three layers by different means, and
    §4.5's screen-locked invariant by construction (it never reads the
    field). THE CHAIN (`bloom.glsl`, the renderer's `run_bloom`): the Anod
    composite rendered once more in linear light at half resolution
    (`linear_out`, an RGBA16F target), its emission kept above a threshold of
    0.04 with a soft knee of 0.04 (so the glass and its speckle stay dark
    and a faint grid line blooms a little), blurred down `anod_bloom_levels`
    octaves with the 13-tap downsample and back up with a 3 × 3 tent, each
    octave adding the one below (Jimenez, "Next Generation Post Processing
    in Call of Duty: Advanced Warfare", 2014); the composite then adds the
    result times `anod_bloom` (`bloom_in`) and passes the sum through a
    shoulder — linear below 0.8, easing toward 1 above, per channel — so
    the brightest filaments clip toward white while their halo keeps the
    palette's hue: the wide cold halo, the neon mid, the white-hot core. The
    print, the export and the alpha export bloom the same (the alpha export's
    coverage widens by the halo's luminance). Two additive params:
    `anod_bloom` (0..3; 0 is the composite as it stands — bitwise, the gate
    and the hashes measured at 0) and `anod_bloom_levels` (1..5). A first
    build dropped the whole composite: the bloom shader's sampler carried the
    composite's sampler's NAME and the two generated headers defined the
    same slot macro with different values — the composite bound its bloom
    sampler to the wrong slot and sokol refused the pass. Binding names are
    global across sokol-shdc headers; the bloom's are its own (`smp_bl`,
    `tex_src`, `tex_small`, `tex_add`). THE AUTHOR'S LOOK (2026-09-23), the
    Anod defaults from here: glass darkness 1 (black glass; 0.5 was the
    step-42 glass), phosphor grain 0.5, glow scale 0.20 (was 1: a hot, early
    glow), bloom 0.75 over 3 octaves (a halo of about a sixteenth of the
    canvas height). The four Anod hashes of the palette test were recaptured
    at these defaults (dc582051c8697b02, 38799f2d9596d4d8, e617110f48b3b5f7,
    fb3f669b2d234944 for electric blue, plasma orange, phosphor green and
    orange under morph); the Sumi four and the composite gate are untouched.
    The Anod test and the substrate check pin the step-42 glass and no bloom
    — they measure the strain reading and the knobs, not the look — and the
    alpha check pins bloom off, since the default halo legitimately lifts
    alpha over the water. Evidence: `anod_glow_default_blue.png` and
    `_orange.png`, the §4.6 script under the defaults. What waits: a
    separate weight for the grid in the bloom's source if the milky water at
    high strength bothers the eye, and the threshold and knee as knobs if
    taste asks — both constants today.

70. **The Anod binding table, signed in part: the bend stirs, the pressure
    tunes.** After the hour of playing (2026-09-23) the author kept MEDIUM
    §4's Anod column except two rows, swapped: the PER-NOTE BEND plays the
    Chladni stir and the POLY-PRESSURE dimension plays the torsion's and the
    spark's wavenumbers. A new mode value, additive: `bend_mode` 4 = the
    Chladni stir — the bend's distance from centre sets `SUMI_CTL_CHLADNI_A`
    (±1.5 semitones saturate it, the ripple law's reach as modes 2/3), its
    SIGN the stir's sense: a bend down turns every eddy the other way (the
    disc rotation is exact, so a bend down after a bend up is the inverse
    pass — a vibrato stirs back and forth and comes home), the last bend
    written wins across voices, a CC mapped to the stir overrides. The
    banked rotation (`cells_pending`, #61) is signed now and drains in
    either sense; the poly route never set a sense, so nothing before this
    could turn backwards. `eff_modes` resolves Anod's default bend to 4 (was
    2); the desktop combo gains "Chladni stir", the INI and the web clamp at
    4. THE POLY-PRESSURE DIMENSION in Anod (no mode param; the medium
    decides it, #51) sets both wavenumbers from their rest at mid-range up —
    k = ½ + ½ · the loudest voice's pressure — under the one-consumer rule:
    a wavenumber the bend owns (`bend_mode` 2 or 3), the slide owns
    (`slide_mode` 2, the spark's, which IS the Anod default) or a CC drives
    is left to its owner. So under the default column pressure tunes the
    torsion alone; with the slide overridden away it tunes both. Headless
    (`test_medium_binding_tables`, revised): Anod's +bend sets the stir to 1
    and emits 13 cells passes with no tine and the torsion's k untouched;
    poly pressure moves TORSION_K to 0.89 (the spark's stays the slide's
    0.945), emits no Chladni pass and no swirl; the overridden table moves
    both wavenumbers to 0.89; Sumi as before. Chirikov's parameters are
    still the author's to find; the rest of the column stands as #51 laid it
    (the press feed's `[ITERATE]` included). FLAGGED for the spec (the
    author's file): MEDIUM §4's rows "swirl (0xA0) — Chladni amplitude" and
    "per-note bend — torsion k / spark k" now read the other way round; this
    entry is the record. The tablet shells still carry the 0.x pickers
    (their steps, 44a/b).

71. **The Anod strike is the spark: a small charge, torn by the shear — the
    burst leaves the composition.** The author, after the hour: the spark
    composition floods the canvas; "the spark shear will be enough, or the
    drop needs to be really tiny". Rendered side by side (six velocity-100
    strikes on the circle of fifths, 1024², the bench's `--anod-strike-render`,
    kept as a lab tool): as shipped, each strike is a blob a fifth of the
    canvas high — the Sumi drop (radius 0.087 at that velocity), the burst's
    lobes and the shear's kick all on that radius; without the burst the
    blob is the same size, so the burst was not the flood; the drop at a
    third with everything scaled shrinks the strike to a compact jag and the
    burst becomes invisible beside the shear; the drop at a third WITH THE
    SHEAR ON THE FULL RADIUS draws the charge into long jagged streamers —
    a spark. That last one ships: a new additive param `anod_drop` (0.1..1,
    default 0.33), the Anod strike's charge as a fraction of the Sumi drop's
    radius, while `sumi_voice_mapper_add_spark` keeps the Sumi radius as its
    band and kick base (so `spark_shear` still reads in Sumi radii and the
    Sumi strike is untouched); the burst call left the Anod strike. What
    follows from the small charge: the pressure feed grows from it (a
    nominal radius a third the size), the torsion sweep's reach floors at
    0.05. `burst_order_by_class` stays in the ABI, clamped and serialized,
    documented as unused by the strike (a composition that brings the burst
    back has its table); the burst itself stays a gesture with its test and
    soak. In passing, the preset field table gained `anod_bloom` and
    `anod_bloom_levels` — #69 had left the glow out of the serializer
    (schema unchanged: additive keys, defaulted when missing) — and
    `anod_drop`; the desktop's Medium section has "Strike charge", the INI
    its key, the web id 42. Headless: `test_medium_binding_tables` measures
    the Anod strike's drop at anod_drop × the Sumi radius, no burst piece,
    the shear's first step in the same frame. Evidence: `anod_strike_
    before.png` (as shipped) and `anod_strike_after.png` (the default now).
    FLAGGED for the spec (the author's file): MEDIUM §4's strike row "drop
    + rotated quadrupole burst (+ spark-shear episode)" is now "a small
    charge + the spark shear"; #51's and #55's "order-by-class table the
    author signs by eye" is moot for the strike.

72. **The stir was dead on the desktop: a mapped, silent CC does not own a
    dimension.** The author: the Chladni stir shows nothing under a bend.
    Headless it stirred (13 passes in the binding test) — the test's mapper
    carries the core's default CC map, which has no Chladni handle; the
    desktop's default map (`app_settings_default_routes`, the INI's `ccmap`)
    routes CC 106 to `SUMI_CTL_CHLADNI_A`, CC 104 to TORSION_K and CC 108 to
    SPARK_K, and #70's routes deferred to "a CC mapped to the dim" — mapped,
    never sent, and the bend and the pressure never wrote. The rule was
    wrong: a mapping is a handle, not a claim. Now LAST WRITER WINS on those
    dims, as the ripple's CC 102 has always shared its slot — the bend
    writes the stir at each bend event, a CC 106 message writes it when it
    arrives; the poly-pressure route, which writes every frame, HOLDS the
    wavenumbers while any voice presses (above half a MIDI step of smoothed
    pressure) and gives them back where they were at the release, so a
    knob's setting survives a gesture. Two stillings the mode-1 ripple
    already had, given to the stir: the last voice lifted under mode 4
    zeroes the stir's target (a note lifted while bent — the ROLI's
    slide-and-lift — must not stir on), and a flip away from mode 4 zeroes
    it (`last_bend_eff`). Regression: the binding test maps CC 106 and 104
    as the desktop does before the bend and still expects the stir; a note
    lifted bent and a flip both leave the stir at 0; the wavenumber returns
    to 0.5 after the pressure. THE RENDERS (the lab's `--anod-strike-render`
    now writes three): `anod_stir_alone.png` — a second of full-rate stir on
    a fresh Anod sheet lights the whole lattice, 128 discs of radius 0.016
    for the circle of fifths, each a small swirl of grid; `anod_strikes_
    bend.png` — the same stir under six strikes bent +2 semitones for a
    second does not read: the spark shears have already displaced every row
    they cross, the grid is lit everywhere, and a rotation inside a 32-texel
    disc rearranges wrinkles nobody can tell apart. So the stir works and
    is invisible where the water is already wrinkled. FLAGGED for the
    author (a design question, not this fix): the spark shear translates
    whole rows across the canvas — exact in Sumi, where water is invisible,
    but in Anod every row it crosses lights the grid, which is both the
    "flood" of #71 and what hides the stir. A shear windowed along its
    length (a local spark: sub-stepped class, det ≠ 1, the soak gate to
    re-pass) would leave the water glass beyond the strike and the lattice
    would show under a bend as in the stir-alone render.

73. **The iPad shell holds ONE session, and the preset serializer is its
    storage (step 44a).** The 1.0 shell kept every setting as its own
    @AppStorage row and pushed params piecewise through a dozen "pending"
    fields; Phase 6 adds ~25 params, a palette, controls and presets that
    must round-trip with the desktop — a second source of truth per field
    would drift. So `SessionStore` (ios/Sources/Session.swift) holds what
    the desktop's `AppSettings` holds — one `sumi_params_t`, the custom
    palette, the CC map, the input dialect, the six routed controls — plus
    the strip's two latch-wheel CCs, and persists it through the pure-C
    serializer linked into the app (`import SumiPreset`, a module map beside
    `presets/include`): Application Support/last_session.json on every
    change (debounced) and on backgrounding, restored at launch; named
    presets at Documents/Presets/<name>.json. The canvas applies the session
    whole (`applySession`: params and palette byte-compared, the CC map,
    the input mode, each changed control as its routed CC through the sole
    producer, the strip assignments on the MIDI queue); the core's defaults
    reach the store once, right after `sumi_create`, as the values a preset's
    missing keys fall back to. The shell's own switches (Play mode,
    transports, the sustain latch, touch-size velocity) stay @AppStorage.
    MIGRATION: on the first 1.1 launch with no last session, the 1.0 rows
    that exist in UserDefaults (palette, viscosity, ink feed, roughness,
    tempo, roll speed, vortex profile, ripple angle / amount / wavelength,
    slide / pinch, press, bend, wake, input mode, CC map) are read into the
    session once; a row never touched keeps the core's default — so an
    untouched bend picker lands on "Medium default". The pitch layout, a
    non-persisted @State in 1.0, now restores with the session. The CC map
    learns the Phase-6 dimensions (targets 14–19, the desktop's names) and
    the desktop's default handles 104–109; a stored 1.0 default map reads
    as today's (the #71 rule, one more older default). THE PAGES (the
    desktop window's rows, names and ranges): "Medium & look" (the medium,
    Palette — the medium's built-ins, the library with "Load into custom",
    the stop editor with ColorPicker in sRGB over the linear model, depth
    curve / floor, drift and its colour, clear water — Substrate — tint and
    paper presets with roughness and fiber scale in Sumi; glass darkness,
    grain, bloom, reach, glow scale, grid lines and strike charge in Anod —
    Presets, Operators — Chladni stir / balance / mode / cell size, burst,
    spark, Chirikov); "Expression routing" (the three modes with "Medium
    default" first and every 1.1 value, the vortex's three profiles, the
    torsion sweep); the sheet opens at the medium detent (large available)
    so the canvas stays in view while a palette is edited live. THE LEDGER
    (ios/Sources/PrintLedger.swift) is the desktop's in Swift — six entries
    or 256 MB (an iPad field is ~31 MB), exports at Screen / 2K / 4K / 8K,
    Anod optionally over alpha, to Documents/Prints/*.png and the share
    sheet (Save Image puts it in Photos), plus "Save the newest print to
    Photos". THE COPY (QOL §6): "Dip the paper — keep the print" and "Clear
    the canvas — discard", with a line saying which keeps what; a clear's
    print is read and dropped on arrival so it can never become the next
    dip's. Documents shows in Files (UIFileSharingEnabled,
    LSSupportsOpeningDocumentsInPlace). The byte path is untouched: no
    change in hostmpe/, MidiSource, MidiOutputs, the play overlay or the
    strip view; `hostmpe_tests` and the normalizer suite pass. EVIDENCE: the
    author's own desktop session (Anod, black glass, bloom 0.75, the
    phosphor-green palette, spark shear 2) was copied into the iPad's
    Presets, loaded, and written back — byte-identical to the desktop file;
    the same six strikes on both at 2360 × 1640 print the same look (glass
    luminance 0.44 / 0.47, glow colour (101, 145, 106) / (101, 146, 107),
    the same tiles lit). The filament shapes differ: the iPad plays in real
    time on a 60 Hz clock and the harness steps a scripted 1/120 s, and the
    spark episodes integrate per frame — the look is the preset's, the
    strokes the performance's. FLAGGED for the roadmap (the author's file):
    ROADMAP_5 numbers this step 44 (iOS) with 45 Android and 46 web; the
    author works to 44a iOS, 44b web marble, 45a Linux, 45b Android, 46
    Windows — the evidence folder follows the author (`step44a/`). And its
    "About shows libsumi 1.0.0": About reads `sumi_version()` and shows
    1.1.0, the additive Phase-6 ABI (#55).
    THE PLAY SURFACE ON THE GLASS (the author, on the iPad): the joysticks
    and the control strip were drawn in black on the assumption of paper —
    invisible on Anod's black glass. Both now follow the medium: on Anod the
    marks are white (the joystick ring and thumb, the echo highlight, the
    hover ghost, the lattice over a dark halo) and the strip is translucent
    smoke with white marks; on Sumi, as in 1.0. Switched in `applySession`
    when the medium changes.

74. **The web marble joins the session model (step 44b).** The page kept a
    private settings object in localStorage; Phase 6 needs presets that
    load identically on every shell. So the ONE serializer compiles into the
    wasm (`presets/` is added to the web build, `sumi_presets` linked into
    `sumi_web`), and a small shim in `web/sumi_web.cpp` assembles the
    session in one static preset — `sumi_web_preset_capture` (the core's
    params and palette), the host state added after (input dialect, the
    CC-map mirror, the control values, in the page's order), `_write` →
    JSON, `_read` over the captured session (the schema rule), `_apply` →
    the core, and getters for the fields the page mirrors back — plus the
    palette flattened to 45 floats (`sumi_web_palette_get/_set/_preset`)
    so JS never lays out a C struct. The page's session is that JSON
    (localStorage `sumi-web-session`, written on every change, restored on
    load; named presets in `sumi-web-presets`; export downloads the file,
    import reads any preset). The page's CC map becomes the desktop's
    default list (the core's map + 102–109 — until now only 102/103 were
    added) and its controls the desktop's six, so a web preset carries the
    same host state as the others. The 1.x settings object migrates once;
    scenes, the field dump and the preset check never read or write the
    session. THE PANEL (lil-gui): Medium (the switch; Anod's strike charge,
    glow scale, grid lines), Substrate (tint, roughness, fiber scale in
    Sumi; glass darkness, grain, bloom, reach in Anod — rows shown per
    medium), Palette (the medium's built-ins and Custom, the library with
    "Load into custom", the editor: stop count with the desktop's insert
    rule, stop colours and positions, depth curve / floor, hue drift, drift
    colour, clear water), Presets (save, saved list, load, delete, export,
    import), the 1.1 routing modes with "Medium default" first, and the dip
    / clear copy ("Dip the paper — keep the print", "Clear the canvas —
    discard"; a clear disables Save until the next dip). Values display to
    six significant digits (the float32 behind 0.48 shows as 0.48 and writes
    back as the same float32). A new permanent gate mode, `web_gate.mjs
    --preset <file>`, sends a preset through the page's import path and back
    out: the author's desktop session comes back byte-identical (2040
    bytes). `site/scripts/check.mjs` learns the Phase-6 scene names
    (torsion, chladni, burst, spark, chirikov, anod): an iframe may name
    them, their embedding is required from step 63 (their pages), and the
    check now fails when its list and `web/site/scenes.js` disagree. What
    the page does not carry: an imported iPad preset's strip-wheel
    assignments (the web has no strip) are dropped on the web's own export.
    FLAGGED for the roadmap (the author's file): ROADMAP_5 names this step
    46 and puts the phase end (the `v2.0.0-alpha.1` tag, folding
    DECISIONS_5) on it; in the author's numbering the phase closes after 46
    (Windows), so the tag and the fold wait. The DONE line's ROLI over Web
    MIDI in Chrome is the author's to play.

75. **The marble gestures follow the medium — in the core.** The author, before
    the handoff: in Anod a tap still lays an ink drop and a pinch still folds
    — the gestures were the Sumi operators whatever the medium. The table,
    the author's (2026-09-23): TAP → the Anod strike (the note-on's: a
    charge of radius·anod_drop and the spark shear episode on the full
    radius, along the layout's pitch axis at the touch — off the lattice,
    radial from the canvas centre); PINCH → the viscous multipole burst
    (the pinch is its r → 0 limit, MEDIUM §2.3); TWIST → the torsion
    vortex; LONG PRESS → hold / push = the torsion sweep feed around the
    charge (a held key's press feed), pull = the Chladni stir reversed. The
    comb (drag) and the pen's wake are the same in both media. WHERE: in the
    core, so every shell plays the same table — five additive calls in the
    1.1.0 window, `sumi_gesture_tap / _pinch / _twist / _press /
    _press_end`, each reading params.medium; in Sumi each is exactly the
    operator call the shells made before (`--gesture-test`: the field after
    tap, pinch, twist and a pushed-then-pulled press is BITWISE the 1.0
    calls'), so the press's feed / swirl rates moved from the three shells
    into the core, unchanged. In Anod: the tap is sumi_add_drop at the
    charge plus the mapper's spark episode — the same two calls the note-on
    makes, so a tap and a note cannot differ; the pinch accumulates its
    squeeze and fires a burst per 0.06 of it (core a quarter of the finger
    span, clamped 0.03–0.15 — a mouse or the pen, having no span, pass twice
    the vortex radius — lobes along the finger axis, +D spreading and −D
    squeezing, the params' order); the twist is the same call with the
    torsion profile; the press's push spends torsion passes on the press's
    own phase clock (the rings travel, as a held key's), reach three times
    the charge floored at 0.05, merged below the sweep's 0.002 rad floor,
    and its pull writes the stir's target and sense (−1) — the long press's
    first touch is a tap, so it strikes. The stir a press sets is let go on
    the release (`_press_end`) or when the press returns to push; a bend or
    a CC that wrote the target since keeps it (last writer). Shells: the
    desktop's click / Shift-drag / right-drag / Shift+right press, the
    iPad's tap / pinch / twist / long press and its stylus barrel pinch, the
    web page's tap / two-finger pinch and twist / mouse drags / long press —
    all through the five calls; the iPad's Mode line and the web hint say
    what the gestures do in Anod. Android picks the calls up at 45b.
    Headless `--gesture-test` (6/6): Sumi bitwise; the Anod tap starts a
    spark episode and inks 405 texels against the Sumi drop's 2966; a 0.2
    spread fires three bursts; the twist is bitwise the torsion pass, not
    the exponential one; the push moves the water round the charge with R
    held; the pull stirs, and the field is still after the press lets go.
    `anod_gestures.png` (step44b evidence) shows the four on one sheet.

## Step 45a — the Linux desktop (GL) — the Linux box

76. **GCC refused the core: a `static` forward declaration with C++ linkage,
    defined inside `extern "C"`.** The first Linux build of 1.1.0 stopped in
    `core/src/renderer.cpp`: `destroy_export` is declared at the top of the
    file (C++ linkage) and defined inside the `extern "C" { … }` block of
    the renderer's ABI (C linkage). Clang, the Mac's and the NDK's compiler,
    accepts the mismatch; GCC rejects it ("conflicting declaration … with 'C'
    linkage"). The forward declaration now sits in its own `extern "C" { }`,
    so both carry the same language linkage; the function stays `static`
    (internal), no code changes, nothing Metal or the wasm compile differs.
    The Windows lane (MSVC) was never affected. This is the only compile
    error the Phase-6 tree had on GCC 15; `presets/src/sumi_preset.c:57`
    carries a `-Wmisleading-indentation` warning left as is.

77. **The print ledger ran the core from the settings window's GL context
    — on Linux that is a foreign context; its dip and export now run in
    `tick()`.** Checklist 6 on the box: Settings › Canvas › Paper dip
    logged `swapchain_gl: readback framebuffer incomplete` and kept no
    sheet, a re-export logged `PBO map failed`. The ledger's `dip()`
    (`sumi_read_field`) and `export_png()` (`sumi_export_begin`, which
    renders a pass) were called from the settings window's `draw()`, with
    that window's context current (#2 of Part IV: the settings window owns a
    GL 3.2 context). The core's GL objects — the readback FBO, the export
    target, sokol's state cache — belong to the CANVAS's context on Linux
    (§5.1's GL exception), and framebuffer objects are not shared between
    contexts; Metal and D3D11 have no current context, which is why the Mac
    and the Windows box never saw it. Now `dip()` and `export_png()` only
    record the request; `tick()`, which the main loop calls with the core's
    context current, carries it out (the export restores the look as it
    stands in the instance, read back, rather than the settings' copy). The
    delay is one frame on every platform. Regression, in `--print-test`
    (now 6/6): a dip and a 1024×576 re-export requested with a second GL
    context current must keep the field and its print and write the PNG —
    with the old ledger the process dies of SIGSEGV at that request
    (`print_test_without_fix.txt`), with the new one it passes. The Mac and
    Windows builds take the same code; re-running `--print-test` there is
    their owners' (nothing here can).

78. **The composite gate on GL: one 8-bit step, from the washi's float
    math — a GL tier of `--max-diff 1` proposed, Metal stays at 0.** The
    canonical print on the box's NVIDIA GL (RTX 5090, driver 610.43)
    differs from `composite_512_metal.rgba` on 2.4 % of channel samples, by
    exactly 1 wherever it differs, biased dark (mean −0.37 / −0.16 / −0.49
    on R / G / B where it differs), alpha never. It is NOT the field's own
    GL difference (#44 of Part IV: mean 6.85e-6, unchanged): 87 % of the
    differing pixels are bare paper and 57 % have a bitwise field in their
    3 × 3 — it is the procedural washi (simplex noise, fibres) rounding
    differently in NVIDIA's GLSL compiler, a driver's rounding and not a
    palette path (a wrong palette would move whole regions by tens). The
    dump is deterministic run to run. So: on GL `composite_gate.py
    --max-diff 1` is green with its negative control red (max 65); no
    workflow runs this gate (only the field gate is in `release.yml`), so
    nothing changes in CI — the tier is recorded here. The gate's summary
    line says "GREEN on metal (bitwise, …)" whatever the tolerance — cosmetic,
    left as is. The palette test's eight FNV-1a hashes are Metal's prints
    (`--palette-test` is 4/5 here by design): the GL hashes are e51602d2a2ffd4e2,
    39ce2b84cd653d3c, e72420d515248657, eeb7c824b3faf0bf (Sumi 0/1/2 and 0
    under morph) and 50c793eccae0acf1, 095dfa67dbc075a4, a2135a24e751e900,
    6d33c733939b8926 (Anod 0/1/2 and 1 under morph); the Sumi at-rest print
    IS the composite gate's (the fixture hashes to the table's first entry),
    and the Anod plasma-orange print against the Mac's own
    `anod_glow_default_orange.png` (which hashes to the table's Anod entry)
    matches in mean colour to 0.03 of a level, its large per-pixel
    differences 5870 of 5871 where the water is displaced: the grid's fine
    lines moved by a pixel, the GL field's sub-texel difference redrawn
    sharp. Visually identical (`anod_orange_metal_vs_gl_diff.png`). The
    Metal table is not edited.

## Step 45b — the Android shell — the Linux box and the Galaxy Tab

79. **The Android shell holds ONE session — natively, as one
    `sumi_preset_t` — and Kotlin edits it with JSON patches read through the
    one serializer.** The iPad's model (#73), shaped for JNI: Kotlin cannot
    see a C struct, and a Kotlin mirror of ~45 params would be a second
    source of truth. So the session lives in `sumi_jni.cpp` (`g.sess`);
    `nativeSessionJson` returns it as the serializer writes it, and
    `nativeSessionPatch` reads a JSON fragment OVER it with
    `sumi_preset_read` — the schema rule (keys present overwrite, missing keys
    keep) makes any fragment a patch and any preset file a load. The render
    thread applies what changed (`apply_session`: params and palette
    byte-compared, the CC map, the input dialect, each changed routed control
    sent as its CC through the sole producer). The core's defaults seed it
    right after `sumi_create`, then the last session (`filesDir/
    last_session.json`) or, the first time, a patch Kotlin builds from the
    0.x SharedPreferences rows that EXIST (an untouched bend lands on
    "Medium default"; a stored stock CC map of #50 or 1.0 reads as today's
    22 routes). sim_scale stays the thermal listener's (Part IV #31): the
    session keeps its own value so a preset round-trips, the core gets the
    host's. The strip's wheel CCs are session keys (`strip`), written by
    the long-press editor and re-assigned on load. Named presets:
    `filesDir/Presets/<name>.json`; import / export through the Storage
    Access Framework; share as text. THE SHEET (`SettingsSheet.kt`,
    foundation-only Compose) has the iPad's pages and the desktop's names and
    ranges: Canvas first (the dip / clear copy of QOL §6, Prints), Medium &
    look (Palette — built-ins, library, "Load into custom", the stop editor
    as sRGB steps over the linear model, depth, drift, clear water;
    Substrate per medium; Presets; Operators), layout, mode, input, the
    routing modes with "Medium default" first and every 1.1 value, the
    vortex's three profiles and the torsion sweep, ripple, stylus wake, the
    CC map with targets 14–19 and handles 104–109. THE LEDGER is native
    (six sheets or 256 MB, the field per dip, a thumbnail when the print
    lands, a clear's print dropped on arrival); exports at Screen / 2K / 4K /
    8K, Anod optionally over alpha, go to `Pictures/midi-sink` through
    MediaStore and the share sheet. A first build never finished an export:
    `sumi_export_poll`'s size query does not advance the readback, the full
    poll does — the frame loop now polls with a kept buffer while one is in
    flight, as the desktop ledger does. THE GESTURES go through
    `sumi_gesture_tap / _pinch (span = finger distance in canvas heights, a
    pen passes 0 → twice the vortex radius) / _twist / _press / _press_end`
    (#75); the press's feed / swirl constants left the shell (only the
    push / pull travel remains, as on iOS). THE PLAY SURFACE follows the
    medium (`setDarkTheme` on the overlay and the strip, #73 addendum). The
    byte path is untouched: the storm's byte log passes every
    `midi_asserts.py device` assert, the on-device suites pass (hostmpe 1569,
    normalizer 19 004), the §4.6 dump is bit-identical to the pre-change
    build's. `android/cpp/CMakeLists.txt` links `sumi_presets`. Evidence:
    `docs/evidence/step45b/`.

80. **FLAGGED for the author (a core question, not changed here): on the
    Adreno the Anod strike's spark tearing drifts from desktop GL and Metal
    — the look is the same, the streamers are fainter.** The screenshot
    compare of #73 (the author's desktop preset, six strikes, the Tab's
    canvas size) showed six charges on the desktop and three on the Tab.
    Narrowed step by step, all on the device: the preset is byte-identical;
    the canonical §4.6 script prints the same under it (glass level, lit
    share 11.85 / 11.88 %, glow colour within 2 levels) — the composite is
    right on GLES; with the strikes frame-locked on the render thread the
    same spark episodes start at the same frames (1 → 6); without the spark
    shear both show the same six charges; one spark stage matches within the
    mobile tier (mean 1.8e-4); the Tab's own field composited on the desktop
    prints the Tab's three charges — so the FIELD differs; and 200 sub-texel
    spark stages (the episode's quantum, `SPARK_MIN_EMIT` 0.0005 canvas
    heights ≈ ⅓ texel here) drift to a mean 4.6e-3, 25 times one stage. A
    strike spends hundreds of such stages, each resampling the RGBA16F field;
    the Adreno filters half floats with half-precision lerps (Part III #30)
    and the difference accumulates in exactly the strain the Anod medium
    lights, so the faint later streamers fall below the glow. Both GLES
    shaders are `highp` throughout (checked in the generated source); the
    shell is not involved. Options, all core changes that would move the
    Metal fixture or the look, so the author's: a larger emission quantum on
    mobile (fewer, bigger passes), a manual highp bilinear in the
    deformation passes, or accepting it as the device's rendering. The
    #73 compare's other half (glass, glow colour, where the charges sit)
    holds. Evidence: `docs/evidence/step45b/compare/`.
    MEASURED, the float field (the author picked it to try first): a
    throwaway build rendering the field as RGBA32F. On the desktop the float
    field prints the same six charges in the same places as the half-float
    one — NVIDIA's half-float path is already precise enough, so the desktop
    is the right reference. On the Tab (float filtering present,
    `GL_OES_texture_float_linear`; still 120.8 fps) the float field tears the
    charges into a THIRD arrangement (four charges, moved), matching
    neither. So the storage format is not the lever: what differs is the
    Adreno's bilinear filtering itself (its fixed-point sub-texel weights),
    and the spark episode amplifies it. Not pursued; the throwaway is gone.
    Left: fewer, larger spark steps on Android (a host-owned quantum,
    default unchanged), or a manual highp bilinear in the deformation
    passes. Evidence: `docs/evidence/step45b/option3/`.

81. **Withdrawn: the Android spark emission quantum. Android keeps the
    core's default spark steps; there is no host quantum knob.** The
    author's call after living with it on the Tab. What was tried: a
    host-owned quantum (an additive `sumi_set_spark_quantum`, clamped to
    [0.0005, 0.02], default `SPARK_MIN_EMIT` = 0.0005 canvas heights), with
    the Android shell setting 0.004 (8×). The same total kick in 37
    kick-drift steps instead of 147. Live it read well while playing but
    stepped visibly as the episode slowed: in the tail each step was a
    3.4 px jump, with pauses of up to 41 frames between them. Two smoothers
    were measured headlessly: a wait limit (a pending kick goes out after N
    frames; ≤ 8 gave 48 steps, pause 8, the same 3.4 px jumps) and a
    quantum that decays with the episode down to a floor (floor 0.002 gave
    57 steps, 2.0 px tail jumps). Neither was adopted. The author found the
    original look best, even though its sparks fade sooner on the Adreno
    (#80). A measurement caveat for whoever reopens this: the Tab's
    six-strike print is not repeatable. The strikes are framed on the
    device's frame clock and land differently on each run. Three runs of
    the same 0.004 build counted 6, 3 and 5 charges, so the single-run
    charge counts behind #80's quantum table and the smoother sweep do not
    rank variants. A fair comparison needs a fixed-dt evidence hook and
    repeats. Rolled back completely: the core (`sumi_core.h`, `engine.cpp`,
    `voice_mapper.*`), `test_spark_quantum` and the JNI/Kotlin call are
    gone, so the core is byte-identical to before #81 on every backend.

## Step 46 — the Windows desktop (D3D11) — the Windows box

82. **MSVC refused the tree once: a constant-folded NaN in the lab bench;
    the build is otherwise clean under /W4.** The first MSVC build of 1.1.0
    stopped in `desktop/src/dev_tools.cpp` (`--palette-test`'s degenerate
    palette): `bad.depth_gamma = 0.0f / 0.0f` is error C2124 on MSVC, which
    Clang and GCC fold to a NaN. It is `std::nanf("")` now — the same quiet
    NaN, the test unchanged on every platform. With that, the whole Phase-6
    tree builds with MSVC 19.44; the only warning in our own code is C4996
    (`strcpy` in `tests/preset_tests.c`, the CRT's deprecation notice, not a
    /W4 finding — left as is, like #76's `-Wmisleading-indentation`); the C11
    serializer passes its 30 checks under MSVC's /W4 for the first time. The
    D3D11 runtime compiler (FXC through sokol) reports its X3571 (`pow` of a
    possibly negative base) as before and three X4000 "potentially
    uninitialized" warnings: one names `sumi_e1` (Part IV's viscous stroke,
    every path returns — FXC's known false positive on an early `return`
    after inlining), the other two are cut by the 512-byte log bridge; the
    output they belong to is proven by the field, composite, palette and
    strike comparisons below, so they are recorded, not chased. Every
    narrow string literal in the shell, core and presets that carries raw
    UTF-8 reaches the binary byte for byte (MSVC reads the sources as cp1252
    and writes cp1252, and none of the 18 such literals holds one of
    cp1252's five undefined bytes); the settings window's labels are ASCII
    (Part IV #9c).

83. **The gates on D3D11: the field is exactly Step 11's, the composite
    is GL's one step — a D3D11 tier of `--max-diff 1`, and a correction
    to the record.** The field gate is green at the reference tier (max
    3.9e-3 on the ink, mean 6.845268e-06, negative control red). The
    handoff (and Part IV #35's note) expected D3D11 **bitwise** on a real
    GPU; it is not, and it never was on this box: today's dump is
    **bit-identical to the Step-11 D3D11 dump** still in git history
    (`f638c7d`), and that dump already sat at mean 6.845268e-06 from the
    Metal fixture, which was captured after it. So there was no drift, and
    Phase 6 changed nothing in the field on D3D11; #35's "D3D11 and GL
    bitwise with the fixture on real GPUs" is FLAGGED as not true for this
    machine (NVIDIA RTX 5090, driver 32.0.16.1664). The composite gate is
    max **1** step on 25 617 of 1 048 576 channel samples (2.44 %), alpha
    never, deterministic run to run — against GL's 2.4 % in #78 with the
    same profile to the pixel: 25 257 vs 25 312 differing pixels, 87 % over
    bare paper, 57 % with a bitwise field in the 3 × 3, every difference
    exactly one step darker, and the SAME sample pixels with the same values
    on both boxes (e.g. (12, 0): 240 where Metal prints 241). Both boxes
    carry an RTX 5090: this is NVIDIA's shader arithmetic through two
    compilers (FXC/DXBC here, GLSL there), not a backend property. D3D11 is
    green at `--max-diff 1` with the negative control red (max 65), so the
    D3D11 tier is `--max-diff 1`, beside GL's; Metal stays 0 and no
    workflow runs this gate. `--palette-test` is 4/5 by design: the D3D11
    hashes are 98ece962a86a326f, 23a68ac9eb47e34a, ac785955c4a5f2ae,
    d4e6e39c77da563f (Sumi 0/1/2 and 0 under morph), 0d008c5cfd49f9ad,
    532095638db23a4f, 5c0893afe0d410bf, b20c62467579a14f (Anod 0/1/2 and 1
    under morph) — distinct from GL's. The Sumi at-rest print IS the D3D11
    composite-gate print (it hashes to 98ece962a86a326f, as the Metal
    fixture hashes to the table's d7cc418955ac2e0e), so the Sumi mismatch is
    exactly the one step above; the Anod prints against the Mac's
    `anod_glow_default_blue/orange.png` have GL's profile (mean colour within
    0.03 of a level of Metal's and equal to GL's to the hundredth, max 74
    where the grid's lines moved a pixel). A reading note: the harness's
    FNV-1a seed is `1469598103934665603`, one digit short of the standard
    64-bit offset basis `14695981039346656037` — self-consistent (the table
    was captured with it) and left alone, since fixing it would invalidate
    every recorded hash; anyone re-hashing a print outside the harness needs
    the harness's seed.

84. **A CRLF `settings.ini` poisoned the print folder: the loader drops
    the carriage return.** Found in checklist 6: every re-export failed
    ("FAILED to save export … Pictures(CR)/midi-sink-print-…png"). The INI
    is read in binary mode, so a file saved with CRLF endings — Notepad, a
    PowerShell `Set-Content`, both the Windows defaults for text — left a
    carriage return at the end of every value; the numbers parse through it,
    but `print_dir` kept it, the export path became invalid, and the next
    save wrote it back, so the folder stayed broken for good. The settings
    window meanwhile said "Exported 4096x2304, writing the PNG in the
    background" — the failure is only on stdout (FLAGGED: a failed
    background write is invisible to the user; a status line after the
    write would say so — the author's call, not changed). The loader now
    strips one trailing CR per line (`app_settings_load`); the shell only,
    every platform, a no-op for an LF file. Regression in `--print-test`: a
    CRLF INI must load `print_dir` at its 11 characters with the flags either
    side read — 6/7 without the fix (12 characters), 7/7 with it
    (`print_test_without_fix.txt`). An INI already poisoned heals on its
    next save.

85. **Windows paths are UTF-8: the application manifest sets the UTF-8
    code page.** The handoff's one try with non-ASCII paths: a print folder
    `…/Pictures/midi-sink-été` typed into the settings window (ImGui holds
    UTF-8) failed to export. The shell hands UTF-8 strings to the narrow C
    runtime — `fopen` (stb's writer, without `STBIW_WINDOWS_UTF8`),
    `std::ofstream` (presets), `_mkdir`, `getenv` (the config and Pictures
    folders) — and on Windows those read and return the process's ANSI code
    page (1252 here), so any path outside ASCII was garbled on the way to the
    file system, and a user whose name is outside the ANSI page (李, Андрей
    on a Western system) would not even get a config folder. One fix covers
    every call without touching them: `desktop/midi-sink.manifest` declares
    `activeCodePage = UTF-8` (Windows 10 1903 and later; older Windows
    ignores the key and keeps today's behaviour), merged into the embedded
    manifest by CMake. Windows only; macOS and Linux are UTF-8 already. The
    upgrade case: a 1.0/1.1 INI written by a user with an accented name holds
    `print_dir` in the old code page, which is not valid UTF-8 — the loader
    now drops a `print_dir` that is not valid UTF-8, so the default folder
    stands instead of a path that cannot open. Regressions in `--print-test`:
    the folder made through the WIDE API (U+00E9 built from its code point,
    so no source-encoding assumption), a PNG written through its UTF-8 path
    as an export is, found again through the wide API — 7/8 without the
    manifest, 8/8 with it (`print_test_without_utf8_manifest.txt`); and a
    legacy cp1252 `print_dir` is dropped (cross-platform). In the UI the
    accented folder shows correctly and the 4K export lands in it
    (`print_folder_utf8.png`). A trap met on the way, for whoever writes the
    next such test: MSVC reads these sources as cp1252 (no `/utf-8`), so a
    raw `é` inside a wide literal becomes "Ã©" — the first version of this
    test garbled both sides the same way and passed without the fix. What
    could not be re-run here: Metal, GL and the web — the manifest is
    Windows-only, and the two INI changes and the new checks are desktop
    shell code (`--print-test` becomes 8/8 on macOS and Linux, the wide-API
    check being Windows-only); the Mac and Linux owners re-run it.

86. **A background PNG write reports its outcome to the UI; a failure is
    never only on stdout.** The #84 flag, taken up: the settings window said
    "Exported 4096x2304, writing the PNG in the background" while the write
    failed, and the failure was one `[print] FAILED` line on stdout — both
    #84 and #85 hid behind it for a whole checklist. Now the desktop has ONE
    background PNG writer, `print_write_async` in `print_export.cpp` (the
    ledger's exports and last prints, and `save_print_png`'s bench path,
    all go through it), which checks the folder first — the failure a user
    can act on — then encodes, and queues its outcome ("Saved 4096x2304 ->
    path", or "Could not write path: the folder does not exist / the C
    runtime's reason"); `print_write_poll` hands the outcomes to the render
    thread. The ledger polls it in `tick()` and the outcome becomes its
    status row (the Prints section shows that row); the Canvas section
    watches the ledger's write serial and puts the same line under its own
    buttons — six seconds for a save, thirty for a failure, so it is read.
    The two pre-existing #84 / #85 failures would now have read "Could not
    write …\Pictures(CR)/…: the folder does not exist" in the window. iOS
    (its ledger reports the write on the main queue), Android (toasts) and
    the web (a download; nothing to fail) already surfaced theirs. The
    settings window's dead `if (false)` block from step 43 went with it.
    Regression, `--print-test` (10/10): a write into a folder that does not
    exist comes back through the poll as a failure naming the path and the
    reason and a good write as "Saved 16x16 -> …"; through the ledger, a
    last print into that folder lands in `status()` as "Could not write …".

87. **The per-backend measurements live in the tools, not in a "by design"
    red: a tier per backend in the composite gate, a hash column per
    backend in the palette test.** #78 and #83 proposed a GL and a D3D11
    tier of one 8-bit step for `composite_gate.py` and left
    `--palette-test` at 4/5 "by design" on those backends, its eight hashes
    being Metal's prints. A test that is red by design teaches everyone to
    read past red. So: `composite_gate.py --backend metal|gl|d3d11` carries
    the tier itself — Metal 0 (the fixture is Metal's: bitwise), GL and
    D3D11 1 (the washi's float math, one step darker in NVIDIA's two
    compilers, the same pixels on both boxes) — `--max-diff` overrides it,
    and the summary line names the backend and whether the run was bitwise
    or within its tier (it said "GREEN on metal (bitwise" whatever ran).
    `--palette-test`'s table has three columns; the bench knows its backend
    (main.cpp hands `sumi_backend_t` to `DevOptions`) and checks its own:
    Metal's column is step 43's proof (the legacy tables' prints, bitwise,
    and the Anod four of #69); GL's and D3D11's are the eight hashes the
    45a and 46 agents printed (`docs/evidence/step45a/palette_test.txt`,
    `step46/palette_test.txt`), transcribed from those logs, not from the
    entries. The message names the column, and a mismatch prints the eight
    hashes so the column can be recaptured with its evidence — the way the
    Anod four were — if a driver rounds its own way; a column is never
    edited to make a run pass. THE RECORD, corrected: Part IV #35's note
    that D3D11 and GL were bitwise with the Metal field fixture "on real
    GPUs" does not hold on the author's boxes (#44 measured GL at mean
    6.85e-6 in Phase 5; #83 found D3D11 at 6.845268e-06 and bit-identical to
    its own Step-11 dump) — the field gate's reference tier (1e-2 / 1e-4)
    is the truth for those two, Metal alone is bitwise, and the
    `field_512_metal.bin` invariant (#12) is a Metal invariant. The
    harness's FNV-1a seed (one digit short of the standard offset basis)
    stays: every recorded hash was taken with it. Verified here: Metal 5/5
    against its column and the gate bitwise; GL and D3D11 not re-run — the
    columns are the agents' own measurements and the tiers the ones they
    ran green (`composite_gate_gl.txt`, `composite_gate_d3d11_maxdiff1.txt`
    in their evidence).

88. **The Adreno's lossy passes, measured to the bottom, and the Anod strike
    made the classic spark on its charge (#80 closed, #71 revised).** The
    author's call: find a solution if one exists, else make the strike the
    Z-key spark, whose streamers are too thick to vanish. Measured on the
    Tab (SM-X906B, Adreno 730) from this Mac with a frame-locked hook — the
    44a script (six velocity-100 strikes 18 frames apart at 1/120 s, 150
    frames, the dip) under the author's Anod preset (spark_shear 2.0: a
    0.17-canvas-height kick on a 0.029 charge, six charge radii — the
    harshest case), the same script on the desktop harness at the same
    1480×924 field, prints compared by lit blobs (the 45b method, in pure
    Python). BEFORE: desktop 6 charges, 1.93 % lit; Tab 2 fragments of 151
    and 127 px, 0.02 % lit (#80's three were a real-time run; frame-locked,
    nearly nothing survives). The confound excluded first: the Tab's shell
    forces sim_scale 0.75 (#31) while the desktop reference was at 1.0 — at
    a true 1480×924 field the Tab still keeps two fragments, and the desktop
    at 0.75 keeps six (1.65 %). Then the two levers #80 left: (1) a manual
    highp bilinear over texelFetch in every deform pass (the filter's
    fixed-point weights out of the loop): the §4.6 field gate's max fell
    1.51e-2 → 9.3e-3 but its mean did not move (6.08e-4 → 6.01e-4), and the
    six strikes vanished entirely (0 lit) — the filter is not the killer;
    (2) an RGBA32F field with that same bilinear (the store's rounding out
    too, the combination #80 had not tried): five charges survive but at a
    fifth to a tenth of the desktop's size (0.36 % lit) — and the unchanged
    field-gate mean says the drift is in the Adreno's shader arithmetic
    itself, which no sampling or storage choice reaches. So there is no
    cheap core fix. THE REAL LEVER, for a later phase: store the field as a
    DISPLACEMENT (u − x, v − y) instead of an absolute coordinate — near zero
    the half-float ulp is ~60× finer than near 0.5, which would shrink every
    quantum problem of this phase (the emission floors #61/#69, the spark's
    quantum, this drift) on every GPU; it moves every shader, the composite,
    the fixtures and the field gate, so it is an architectural decision, not
    a fix. Both experiments were throwaways; the tree is byte-identical to
    before them. SHIPPED, the author's plan B: the Anod strike is the CLASSIC
    SPARK on its charge — `sumi_add_spark`'s composition with r = the charge
    (radius · anod_drop): the drop, a burst of that core (D = 0.3 r, the
    order params.burst_order, ANOD_STRIKE_BURST_D back) and the shear
    episode with the charge as its band and kick base, in the mapper's
    strike and in `sumi_gesture_tap` alike. `anod_drop`'s default 0.33 →
    **0.57**: the Z spark's 0.05 at the velocity-100 drop. The streamers
    are thicker and shorter — #71's threads on the Sumi radius were exactly
    what a lossy renderer loses. AFTER, the same script: under the author's
    preset (shear 2.0) the Tab keeps six charges (3.68 % lit, two of them
    thinned: 1253 and 763 px against the desktop's 6 of 10–16 k px, 5.99 %);
    under the strike's defaults (shear 0.6, τ 0.25, order 2) the Tab and the
    desktop are alike — six charges, 4.95 % vs 4.62 % lit, 5.8–14 k px vs
    9–12 k px. The look on the desktop changes with it (the sparks are
    fuller; `anod_strikes_default_after.png` beside step 43's
    `anod_strike_after.png`) — the author's to judge and to retune
    (anod_drop, spark_shear). MEDIUM §4's strike row ("drop + rotated
    quadrupole burst (+ spark-shear episode)") is true again, the order from
    burst_order rather than the class table (which stays in the ABI,
    unused). Tests: the binding-table test now sets the core's anod_drop
    and burst_order (its zero-filled params had made the charge 0, which
    #71's check passed trivially) and expects the burst's first pieces
    beside the shear's first step; `--gesture-test`'s tap check reads "a
    charge smaller than the Sumi drop" (1126 vs 2966 texels at 0.57). The
    evidence hooks (an Android `--es strikes 1` intent and a desktop
    `--preset-render`) were removed after the captures; the Tab carries the
    hook-free build and its original preset file.

## Phase close (after step 46, 2026-09-26)

89. **Phase 6 closes: what is folded, what is carried, and what the author
    keeps.** This file becomes Part V of `docs/DECISIONS.md`; the Phase-6
    evidence (`docs/evidence/step35 … step46_fixes`, and Phase 5's
    `store-submission`) condenses into `docs/CHANGELOG.md` (the `v2.0.0`
    section, pre-release `v2.0.0-alpha.1` — the tag is the author's) and
    leaves the tree; the Phase-6 roadmap section moves into
    `docs/ROADMAP.md` as Part 5 under the author's step numbering (44a iOS,
    44b web, 45a Linux, 45b Android, 46 Windows — ROADMAP_5's 44/45/46
    corrected on the way); the operator-page drafts (torsion, chladni ×2,
    burst, spark, chirikov, anod, palettes) wait for step 63 in
    `site/drafts/operators/`; the store-screenshot scripts keep in
    `tools/store_screenshots/`. THE SPECS: `specs/CONTEXT.md`,
    `specs/MEDIUM_SPEC.md`, `specs/chladni.md` and `specs/spark.py` are
    removed at the author's request — their content, corrected to what
    shipped (#50–#88), is drafted for transcription into
    `docs/PROJECT_SPEC.md` in `specs/TO_PROJECT_SPEC.md` (the medium's
    section, the shipped quality-of-life items, the pointers to the removed
    files in git history); `specs/QUALITY_OF_LIFE_SPEC.md` keeps only the
    undone items (replay, the strip's quick-switch and preset-next, per-
    device presets, left-handed mirroring, TIFF-16, the fibre angle drift,
    the advanced palette fold); `INSTRUMENT_SPEC.md` and `SOUND_SPEC.md`
    stand. CARRIED: (1) the field stored as a DISPLACEMENT (#88's real
    lever against every half-float quantum) is on the open roadmap before
    the instruments, as step 55b; (2) the Chirikov standard map's feel — the
    author, after the tuning grid: "the vibrations are tiny and the
    displacement is huge, I was hoping for the contrary" — the operator is
    correct and gated (#40–#43) but not yet the instrument the author hears;
    open, the author's; (3) the local spark (#72's proposal) is not pursued
    — the author keeps the Chladni stir as it is ("working better, don't
    poke it"); (4) the hands-on checks the agents could not make (the ROLI
    in Anod on Linux, Windows and the Tab; the S-Pen; the web marble from a
    ROLI over Web MIDI) the author runs with the simulated MPE, the Airwave
    and the wind songs; (5) the #88 strike look is confirmed by the author
    ("the charge and shear look correct"). Phase 7 (Sound) opens
    `_work/DECISIONS_6.md`; the core is frozen again until then, the phase
    invariant (#12) a Metal invariant (#87).

