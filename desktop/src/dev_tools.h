// dev_tools.h — the lab bench behind --dev (Phase 5, DECISIONS_4 #5).
// Everything here existed before as main.cpp's debug keys and scripted DONE
// tests; the product loop calls in only when the flag is set.
#pragma once

#include "sumi_core.h"
#include <cstdint>

struct GLFWwindow;
struct AppSettings;

struct DevOptions {
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
    bool   cycle_visuals = false;
    const char* field_dump = nullptr;   // §4.6 cross-backend field regression
    // v0.4 step-19/20 scripted DONE tests (run and exit).
    bool t_wake = false, t_flick = false, t_rankine = false;
    bool t_ripple_group = false, t_ripple_dip = false, t_pinch_demo = false;
    bool t_ripple_perm = false, t_swirl = false;
    bool t_pressure = false;   // v0.6: --pressure-test (feed/swirl gestures + print recycle)
    bool t_stokeslet = false;  // v0.7: --stokeslet-test (the viscous stroke)
    long t_pinch_passes = 0;
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
};
void dev_loop_begin(DevLoop& d, const DevOptions& o, AppSettings& st,
                    sumi_instance_t* inst, void* midi);
void dev_loop_pre_update(DevLoop& d, sumi_instance_t* inst);
void dev_loop_post_frame(DevLoop& d, GLFWwindow* window, sumi_instance_t* inst,
                         AppSettings& st, bool* settings_changed,
                         double now, double dt, uint64_t frames);
void dev_loop_report(const DevLoop& d, sumi_instance_t* inst, double now, uint64_t frames);

// The debug key bindings. Mutates the settings mirror; *changed_out tells the
// caller to apply + persist.
void dev_key(GLFWwindow* window, AppSettings& st, sumi_instance_t* inst, void* midi,
             int key, int mods, bool* changed_out);
