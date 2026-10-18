# Step 79 — The membrane, the bus plate and the gong (2026-10-19)

Voxo **0.19.0**, additive (`DECISIONS_12 #3`; `_work/ROADMAP_6.md` step 79;
`_work/TO_PROJECT_SPEC.md` §13.9 as shipped). The core is frozen: nothing
under `core/` moved.

## What shipped

* **`suzu::Membrane<CAP>`** (`voxo/src/suzu.h`) — the 2D symplectic
  leapfrog grid inside a simply-supported zero ring: the kick from the
  pre-update positions (the biharmonic as the Laplacian twice through the
  ring), the declared contraction γ₀ + γ₂Δ, the drift; the gong's
  stretching E read off the Laplacian pass and the kick's c² + αE as an
  exact momentum shear; the strike (a raised-cosine bump of momentum), the
  cap on the plain energy, the pickup, the exact invariant `energy()`. Four
  lanes a row behind `simd.h`, the stretching summed by lane then in order
  — the web gate's reference is bit for bit.
* **`voice_kind` 9** in `voxo.cpp`: the grid cut per note under the CFL
  gate at E_max (`MEMBRANE_CFL_GATE` 1.95; **the gong's bar 1.80**,
  measured — `gong_bar_probe.txt`), the strike scaled by ε₁₁ so the level
  holds across the keyboard, the cap enforced, the damping on the momentum
  at twice the rate (the declared T60 read back), the release multiplying
  the held contraction, the glide saturating at the admitted ε. The params
  `drum_size`, `drum_kappa` (the biharmonic share), `drum_alpha` (the
  swell), `drum_cap`, the lab's `drum_cfl_gate` / `drum_cfl`.
* **The bus plate**: one `Membrane<64>` on `voxo_t` at the output rate;
  every Suzu voice writes its block beside the sum and injects it at its
  point (`voxo_set_strike_point` — the iPad's and the Tab's touch-begins
  hand the layout cell's centre; the desktop's keyboard the default map);
  two mirrored pickups into the stereo sum before the reverb; the tuning
  clamped under the gate with a log line, never refused; `VOXO_MOBILE`
  caps 32². The params `plate_on`, `plate_size`, `plate_hz`,
  `plate_kappa`, `plate_decay_s`, `plate_bright`, `plate_gain`.
* **The plumbing**: the web table and `sw_strike_point`, `KIND_NAMES`
  'membrane', the preset library's eleven fields (`presets/SCHEMA.md`),
  the desktop's *Suzu drum …* rows and the *Suzu bus plate* section with
  their INI keys, the inspection's kind 9, the trace's aux pair, the
  strobe; the patch table's fourteen (*Drum*, *Gong*, *Harp on a
  soundboard* ↔ `presets/drum.json`, `gong.json`, `harp_soundboard.json`).
* **Found and fixed**: the bench's `--preset` applied a session preset to
  the engine alone, so a storm "on a preset" played the settings' patch —
  it reaches the Sound settings now (unsaved for the run). The Android
  debug APK's engine is −O0: the timing evidence moved to the release APK.

## Gates

| Gate | Result | File |
|---|---|---|
| the mode table (simply supported), the membrane and the plate at A2 on 16 × 13 | the (1,1), (2,1), (1,2) within 0.01 cent of the table (√λ / λ) | `suite_step79_blocks.txt` |
| the undamped 8 × 6 grid's exact invariant, ten minutes at 96 k | −0.00016 dB (the span 0.0008; the bound 0.005); the plain energy's wobble 0.27 dB | same |
| the CFL gate at E_max through the ABI | 1.05× rejected with its sentence; 0.95 admitted; the bypass trips the breaker within a second (RED) | same |
| the class's red / green | tuned to 1.95, displaced to 1.2× the cap's stretching (λ 2.045): non-finite within 100 steps; at the gong's bar 1.80 the sheet holds 200 000 steps within 0.8 dB — at 1.95 it diverged | same, `gong_bar_probe.txt` |
| the gong's swell | C3 at velocity 127 +107 cents over the first 60 ms against +2 at velocity 30; on the note a second later; the undamped energy within 0.33 dB | same |
| the bus plate's tier | 64² λ_2D 1.48, 32² 0.46 at the declared share; the clamp line when asked 0.5 | same |
| the plate audible / silent / moved | −9.3 dB under the dry harp chord; off = the concert harp bit for bit; the centre against the corner differs by −11 dB of the dry; a negative point = the default map bit for bit | same |
| headroom (Release, NEON) | 16 membranes 16.5 % of the period (the bar 30 %); the 64² plate 16.3 % (the bar 25 %) | `suite_release.txt` |
| the strobe | 261 returns in 2 s at C3 (262 expected), finite; the chart's last panel | `strobe/` |
| the web gate | GREEN — the membrane bit for bit against the scalar no-FMA reference; the other kinds as before | `gates/suzu_web_gate.txt` |
| the lab gate (headless Chrome, six pages) | GREEN | `gates/suzu_lab_gate.txt` |
| the storm from the Release universal build (the preset's patch for real) | the drum, the gong, the harp on the soundboard: 0 XRuns, render max 0.79 / 0.79 / 0.74 ms | `gates/storm_*.txt` |
| the fuzz (Release) | 60 s clean; the strobe fuzz 30 s: 0 blocks differ, 0 non-finite | `gates/voxo_fuzz.txt` |
| ctest (Debug) | 14/14 | `gates/ctest.txt` |
| the profiles | the drum: peak −23 … −15 dBFS over 21..108; the gong: −19 … −9 with the swell in the spectrogram | `profiles/` |

## The tablets

* **Android (the Pixel 10 Pro emulator, arm64 / NEON, Android 17; the Tab
  unplugged) from the RELEASE APK signed with the debug key**, idle machine:
  the Drum 4669 callbacks, 0 device XRuns (1 proxy at the first run's
  start), render max 10.0 ms of a 20 ms burst; the Gong 4661 callbacks,
  0 / 0, render max 2.5 ms; the Harp on a soundboard (the plate at 32²)
  4666 callbacks, 1 device XRun, 3 proxy, render max 15.8 ms.
  `tablets/android_emulator_patch1*.txt`. The DEBUG APK (−O0) read 60 ms
  and hundreds of XRuns on the same runs — not the engine's cost.
* **iOS (the iPad Air M4)**: `build-ios` rebuilt, the app built on the
  existing project (`xcodebuild … generic/platform=iOS`), installed and
  launched with `--voxo-source suzu --suzu-patch <n> --voxo-spike 12` on
  the unlocked device: the Drum 8 036 callbacks, 0 XRuns, render max
  0.578 ms; the Gong 8 008 callbacks, 0 XRuns, render max 0.653 ms; the
  Harp on a soundboard (the plate at 32²) 8 009 callbacks, 0 XRuns, render
  max 0.346 ms — 57 note-ons each, the 2.667 ms session buffer at 128
  frames (`tablets/ipad_voxo_spike_patch1*.csv`). A first copy taken while
  the device was locked had pulled a stale CSV from an earlier run; the
  three above are the step's.

## Carried

The Tab's own run, the author's choice of
the drum and gong recordings the profiles are read against, the lab's
membrane page (step 92), the strike profile from the layout cell (step
80), the AVX2 lanes.
