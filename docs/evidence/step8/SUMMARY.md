# Step 8 DONE evidence — v1 hardening & visual polish

Date: 2026-09-01. Spec: PROJECT_SPEC_2.md. All GUI runs
`METAL_DEVICE_WRAPPER_TYPE=1`, zero validation output. Rules v2 in CLAUDE.md;
new ambiguities in DECISIONS_2.md.

| DONE check | Result | Evidence |
|---|---|---|
| Stuck-voice safeguard (§3.1) with passing test | **PASS** — overflow-armed per-voice ~10 s inactivity timeout; unit test scripts a swallowed Note Off (dropped counter increments), asserts NO expiry before arming, synthetic `VoiceEnd` (lift 0) after 10 s of voice silence amid flowing traffic, and no re-expiry | tests: `test_overflow_stuck_voice_timeout` |
| Dip double-buffer (§5.3) with stress test | **PASS** — two print buffers flip on dip; live burst (dips at t, t+0.2 s, t+0.25 s during MPE stress): two `print ready`, third dip `refused (both print buffers busy)` WARN, both prints read back afterwards (newest then oldest) and saved intact; 0 dropped | `run_dip_burst.log`, `burst_print_newest.png`, `burst_print_oldest.png`; test `test_dip_rebase_and_refusal` |
| Aux rebase on dip (§4.2) with passing test | **PASS** — drop counter rebases to 0 on every accepted dip; scripted 3000-drop two-dip session asserts every emitted aux < 2048 (max seen ≈ 1000) and counter < 2048 | test `test_aux_rebase_3000_drop_session` |
| Nested-ring check (§4.4) | **PASS** — feed episodes: press onset after full release stamps a NEW band (counter advances, seeded small); live pulse script (tests/pressure_pulses.swift, 1 voice × 5 pulses at C4) yields 5 visibly nested sharp rings; unit test asserts band/aux progression (first episode continues the strike band, 4 new bands after) | `nested_rings.png`, `run_pulses.log`; test `test_feed_episodes_nested_rings` |
| Dip print: organic non-lattice fiber, near-black pooled ink | **PASS** — per-region ±20° fiber angle drift + ridge-length segment break-up (no coherent crosshatch anywhere); pooled ink ≈ 0.05–0.07 linear; fiber modulation under dense ink capped low; thin-ink grain falloff along ring boundaries via neighborhood band probing (DECISIONS_2 #4 — fract-based thickness fails on feed micro-shells) | `dip_print_polished.png` |
| No step-3/5 regressions | **PASS** — 500-drop run: rings pixel-sharp (1:1 crop), 98.6 fps; osmose stress 15 s: 98.3 fps, 0 dropped; dip-window worst frame 14.5 ms; all 502 headless checks green; leaks at OS baseline across a dip burst | `regression_drops500_crop.png`, `run_regression_*.log`, ctest |

ABI note: only the §5.3 comment on `sumi_trigger_paper_dip` changed — the v2
layout/params/version-0.2.0 extensions are explicitly NOT implemented (later
step, per the working rules).
