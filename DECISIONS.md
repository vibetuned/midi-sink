# DECISIONS

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

39. **Pinned dependencies** (all FetchContent):
    - glm `1.0.1`
    - sokol `1847290135f95e57e6d220b0a41208306aafc0dd` (master 2026-08-30)
    - libremidi `v5.4.3`
    - GLFW `3.4`
    - sokol-tools-bin `11d0cf678105d614d675e6d9bd2aaf3eeff12f8c` (sokol-shdc)
