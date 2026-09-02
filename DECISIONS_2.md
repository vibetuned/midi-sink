# DECISIONS_2

Ambiguities resolved during spec-v2 implementation (v1 history: DECISIONS.md;
where the two conflict, PROJECT_SPEC_2.md wins — it absorbed the validated v1
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
