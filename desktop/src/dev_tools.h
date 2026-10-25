// dev_tools.h — the lab bench behind --dev (Phase 5, DECISIONS_4 #5).
// Everything here existed before as main.cpp's debug keys and scripted DONE
// tests; the product loop calls in only when the flag is set.
#pragma once

#include "sumi_core.h"
#include <cstdint>

struct GLFWwindow;
struct AppSettings;
typedef struct voxo_t voxo_t;   // Phase 7 step 47: the --voxo-storm proxy

struct DevOptions {
    uint32_t backend = 0;             // sumi_backend_t of this build (main.cpp sets it): the tests that hold per-backend measurements pick their column by it (#87)
    double exit_after = 0.0;        // 0 = run until the window is closed
    bool   resize_test = false;
    float  sim_scale = 0.0f;        // 0 = leave the setting alone
    long   drop_test = 0;
    bool   demo_chevron = false;
    bool   demo_vortex = false;
    int    map_cc = -1, map_target = -1;
    int    layout = -1;
    double dip_at = 0.0;
    double dip_burst = 0.0;
    const char* print_out = nullptr;
    const char* strike_render = nullptr;   // step 43: --anod-strike-render <dir>
    const char* pair_drift = nullptr;      // step 55b: --pair-drift <operator>: the growth law of the (b) pairs' pre-image deviation
    const char* preset = nullptr;          // step 55b: --preset <file.json>: a session preset applied before the scripted tests (the Tab compare)
    bool   cycle_visuals = false;
    const char* field_dump = nullptr;   // §4.6 cross-backend field regression
    const char* composite_dump = nullptr;   // Phase 6 step 41: the composite screenshot regression (the print of the same script)
    int         dump_medium = -1;           // step 76: --dump-medium <0|1|2> — the medium the composite dump prints under (−1 = the params' as they stand, Sumi)
    // v0.4 step-19/20 scripted DONE tests (run and exit).
    bool t_wake = false, t_flick = false, t_rankine = false;
    bool t_ripple_group = false, t_ripple_dip = false, t_pinch_demo = false;
    bool t_ripple_perm = false, t_swirl = false;
    bool t_pressure = false;   // v0.6: --pressure-test (feed/swirl gestures + print recycle)
    bool t_stokeslet = false;  // v0.7: --stokeslet-test (the viscous stroke)
    long t_pinch_passes = 0;
    // Phase 6 step 35 (ROADMAP_5): the per-operator FOUR-PART conservation
    // gate — the step-19 pinch soak generalised (DECISIONS_3 #33).
    const char* soak = nullptr;      // --soak <operator|all>
    long soak_passes = 6000;         // --soak-passes <n>: the (c)/(d) stream length
    bool soak_negative = false;      // --soak-negative: the red controls (each part must FAIL)
    bool t_torsion = false;          // --torsion-test (Phase 6 step 36): the wave torsion + the sweep episode
    bool t_chladni = false;          // --chladni-test (Phase 6 step 37): the Chladni lattice
    bool t_burst = false;            // --burst-test (Phase 6 step 38): the viscous multipole burst
    bool t_spark = false;            // --spark-test (Phase 6 step 39): the spark shear and the composed strike
    bool t_chirikov = false;         // --chirikov-test (Phase 6 step 40): the Chirikov standard map
    bool t_palette = false;          // --palette-test (Phase 6 step 41): the custom palette of the 1.0.0 ABI
    bool t_anod = false;             // --anod-test (Phase 6 step 42): the strain-glow composite and the re-read
    bool t_defaults = false;         // --defaults-test (Phase 9 step 64, DECISIONS_8 #19): the core's params as sumi_create leaves them
    bool t_gesture = false;           // #75: --gesture-test, the medium-aware gestures
    bool t_trace = false;             // Phase 8 step 59: --trace-test, the orbit trace's budget and its clean toggle
    const char* trace_demo = nullptr; //   --trace-demo <dir>: the rotor scribbling into Anod, frame by frame (PNGs for the video)
    const char* boris_demo = nullptr; //   --boris-demo <dir>: step 84 — the gyroscope's spirograph into Kessho and Anod's spark, stills
    bool t_print = false;             // --anod-test (Phase 6 step 42): the strain-glow composite and the re-read   // Phase 6 step 43 (QOL §4): prints at any size, the ledger's premise
    double voxo_storm = 0.0;          // Phase 7 step 47: --voxo-storm <s>, the scripted MPE storm through the real device (the ROLI proxy)
    const char* voxo_bounce = nullptr; // step 49: --voxo-bounce <dir>, the ±48 glide bounced offline with Hermite and linear reads
    float       voxo_suzu_shear = 0.0f;  // step 56: --voxo-suzu-shear <g>, the profile's patch with a cubic shear (the harmonics case)
    int         voxo_suzu_preset = -1;   // step 57: --voxo-suzu-preset <n>, the modal preset for the profile (−1 = one cell, step 56's voice)
    float       voxo_suzu_breath = 0.0f; // step 57: --voxo-suzu-breath <b>, breath held through every note of the profile (the bow's case)
    int         voxo_suzu_voice = -1;    // step 58: --voxo-suzu-voice <kind>, Suzu's voice for this run (0 cell … 5 rotor; the profile's and the storm's)
    const char* voxo_chart = nullptr;    // step 58: --voxo-chart <dir>, the chaos charts' WAVs: the rotor's K sweep, the Duffing clang, the driven Duffing
    const char* voxo_profile = nullptr; // Phase 8 step 56: --voxo-profile <dir>, the SOUND PROFILE of the source (--voxo-source, --voxo-preset) across the keyboard, offline
    const char* voxo_load = nullptr;   // step 50: --voxo-load <preset>, the compat report of a Decent Sampler preset, printed; exit 0 loaded / 1 refused
    const char* voxo_preset = nullptr; // step 52: --voxo-preset <preset>, the instrument for this run (the setting untouched) — the storm's material
    const char* voxo_source = nullptr;      // Phase 8 step 56: --voxo-source sampler|suzu — the source for this run, the setting untouched
    double voxo_budget_mb = -1.0;      // step 52: --voxo-budget-mb <n>, the gate's advice for this run (the free-memory check overridden)
    const char* replay = nullptr;          // Phase 9 step 65: --replay <file.sumireplay>: play it on the scripted clock (with --field-dump: the field after, at the recorded size)
    double      replay_wall = 0.0;         //   --replay-wall <hz>: the NEGATIVE test — the file's events re-bucketed by wall time at that cadence (must diverge)
    const char* replay_wav = nullptr;      //   --replay-wav <out.wav>: the replay re-sounded through Voxo offline (--voxo-source, --voxo-preset), the evidence of re-sounding
    const char* bounce_test = nullptr;   // step 91: a replay bounced twice through the host path, byte for byte
    bool        device_test = false;      // step 91: the output device switched without a stop
    long        replay_warmup = 0;
    const char* replay_live = nullptr;     //   --replay-live <file>: the INTERACTIVE player (ReplayHost) from one second in — the harness muted, Metal's display sync off, the pacing
    double      record_live = 0.0;         //   --record-live <s>: the interactive recorder from one second in for <s> seconds (with --voxo-storm, the storm's bytes ride the harness's stage)         //   --replay-warmup <frames>: settle frames (1/120) before the replay begins — the core's clock origin as a variable
    const char* record_demo = nullptr;     //   --record-demo <file.sumireplay>: the bench's canonical performance through the real recorder, with <file>.field.bin beside it
    const char* layout_shot = nullptr; // Phase 9 step 60: --layout-shot <png>, the layout (--layout) drawn as the visualizer's overlay over a scripted fingering phrase, dipped and printed
    bool        trumpet_arc = false;   //   --trumpet-arc: the trumpet's partials on the arc for that run
    int         string_tuning = -1;    //   --string-tuning <0|1|2> (step 61): the strings' preset for that run
    // Phase 11 step 71 (v3.0, KESSHO §1): the Kessho medium
    bool        t_kessho = false;         // --kessho-test: the substrate, the chart, the brush, the regimes, the switch, the exports (gates 1, 3–6)
    const char* kessho_flat = nullptr;    // --kessho-flat <operator|all>: gate 2 — |det J − 1| over the soak scenes (the flat bed at z = 1)
    int         mineral = -1;             // --mineral <id> (step 71b): the library mineral for that run, both halves
    bool        t_habit = false;          // --habit-test (step 71b): the crystal drop's gates 4–9 — homothety, growth, the crease pair, calcite + twin kink, the pass count
    const char* habit_corner = nullptr;   // --habit-corner <dir>: the crease pair alone, archived
    bool        tine_fault = false;       // --tine-fault: the fabrication RED control — the soak under the unwindowed profile must trip (c)
    bool        t_arnold = false;         // --arnold-test (step 72): the pass against the closed form and its exact inverse, the locked islands, the precession, the twist's red, the tongue crossing
    const char* arnold_render = nullptr;  // --arnold-render <dir> (step 72): the locked and the precessing renders in Kessho, archived
    const char* arnold_probe = nullptr;   // --arnold-probe <dir> (step 72): the lock's early frames and the kick regimes, a diagnostic
    bool        t_braid = false;          // --braid-test (step 73): the pass count and the frame time at fifteen voices, the fifth's saddle, the second's jet, the bend's flip
    bool        t_chord_wave = false;     // --chord-wave-test (step 74): a note's parallel ripples in its band, a fifth's lattice where the bands cross
    bool        t_strobe = false;         // --strobe-test (step 75): the Poincaré strobe on the live core — the clean scope, the ink's stamps, one point / two / a ring
    bool        t_seed = false;           // --seed-test (step 76): one gesture script twice under one seed is bitwise in Anod and Kessho; the red: without the reseed Anod diverges
    bool        t_spark_reading = false;  // --spark-reading-test (step 76): the spark reading — plasma bitwise, the field a reader, thin and sparse ridges with their reds, the afterglow's decay, the branches, the screen-locked grain; the A/B renders into --ab-out
    bool        t_leader = false;         // --leader-test (step 76b): the leader stroke — determinism, thinness with the plateau red, branches and the single leader, the afterglow on a stroke, velocity → reach, the storm's pass budget; the three-way viewing into --ab-out
    float       leader_pb = -1.0f;        // --leader-pb <p> (step 76b): the branch probability for this run's live and replay strokes (< 0 = the core's 0.15; the test sets its own)
    const char* chirikov_grid = nullptr;  // --chirikov-grid <dir> (step 76): the Chirikov operator over a grid of K × periods × ε, one PNG each, for the author's choice (DECISIONS_5 #89)
    const char* ab = nullptr;             // --ab polarizer: the A/B item K-1 rendered both ways (the braid's at step 73)
    const char* ab_out = ".";             //   --ab-out <dir>: where the renders go
};

// Parses argv[i] (advancing i for valued flags). 1 = consumed, 0 = not a
// lab-bench flag, -1 = malformed (message already printed).
int  dev_parse_arg(DevOptions& o, int argc, char** argv, int& i);
void dev_print_usage(const char* argv0);
const char* dev_key_legend();
// 1.7.0 (step 71b): a dev key's name on this keyboard layout (GLFW's keys are US positions; "A" is "Q" on AZERTY)
const char* dev_key_name(int key);

// Run-and-exit modes (--field-dump, the test battery). Returns the process
// exit code, or -1 when none was requested.
int  dev_run_scripted(const DevOptions& o, GLFWwindow* window, sumi_instance_t* inst);

// Scripted inputs that ride the interactive loop.
struct DevLoop {
    DevOptions o;
    void*  midi = nullptr;
    double start = -1.0;
    double dt_min = 1e9, dt_max = 0.0;   // first frame excluded (startup cost)
    long   drops_done = 0;
    int    demo_frame = 0;
    int    resize_step = 0;
    bool   dip_done = false, print_saved = false;
    double dip_time = -1.0, dip_worst = 0.0;
    int    visual_step = 0;
    int    burst_step = 0;                // §5.3 double-buffer stress
    voxo_t* voxo = nullptr;               // step 47: main.cpp sets it after begin (the storm's stats)
    uint64_t storm_frames = 0;            // frames the storm has been fed
    uint32_t storm_messages = 0;          // messages injected
};
void dev_loop_begin(DevLoop& d, const DevOptions& o, AppSettings& st,
                    sumi_instance_t* inst, void* midi);
void dev_loop_pre_update(DevLoop& d, sumi_instance_t* inst);
void dev_loop_post_frame(DevLoop& d, GLFWwindow* window, sumi_instance_t* inst,
                         AppSettings& st, bool* settings_changed,
                         double now, double dt, uint64_t frames);
// Returns the process exit code: 0, or 1 when a scripted check in the loop (the storm) failed.
int  dev_loop_report(const DevLoop& d, sumi_instance_t* inst, double now, uint64_t frames);

// The debug key bindings. Mutates the settings mirror; *changed_out tells the
// caller to apply + persist.
void dev_key(GLFWwindow* window, AppSettings& st, sumi_instance_t* inst, void* midi,
             int key, int mods, bool* changed_out);

// Phase 9 step 64 (DECISIONS_8 #19): the params exactly as sumi_create left them, captured by main.cpp
// before any settings apply — the --defaults-test's subject (a field added without a default read 200 on the Tab).
extern sumi_params_t g_dev_core_defaults;
extern bool g_dev_core_defaults_valid;
