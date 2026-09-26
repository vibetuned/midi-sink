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
    bool   cycle_visuals = false;
    const char* field_dump = nullptr;   // §4.6 cross-backend field regression
    const char* composite_dump = nullptr;   // Phase 6 step 41: the composite screenshot regression (the print of the same script)
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
    bool t_gesture = false;           // #75: --gesture-test, the medium-aware gestures
    bool t_print = false;             // --anod-test (Phase 6 step 42): the strain-glow composite and the re-read   // Phase 6 step 43 (QOL §4): prints at any size, the ledger's premise
    double voxo_storm = 0.0;          // Phase 7 step 47: --voxo-storm <s>, the scripted MPE storm through the real device (the ROLI proxy)
    const char* voxo_bounce = nullptr; // step 49: --voxo-bounce <dir>, the ±48 glide bounced offline with Hermite and linear reads
    const char* voxo_load = nullptr;   // step 50: --voxo-load <preset>, the compat report of a Decent Sampler preset, printed; exit 0 loaded / 1 refused
    const char* voxo_preset = nullptr; // step 52: --voxo-preset <preset>, the instrument for this run (the setting untouched) — the storm's material
    double voxo_budget_mb = -1.0;      // step 52: --voxo-budget-mb <n>, the gate's advice for this run (the free-memory check overridden)
};

// Parses argv[i] (advancing i for valued flags). 1 = consumed, 0 = not a
// lab-bench flag, -1 = malformed (message already printed).
int  dev_parse_arg(DevOptions& o, int argc, char** argv, int& i);
void dev_print_usage(const char* argv0);
const char* dev_key_legend();

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
