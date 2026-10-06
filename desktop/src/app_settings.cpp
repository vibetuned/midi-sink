// app_settings.cpp — INI persistence + the params / CC-map mirror.
#include "app_settings.h"
#include "midi_harness.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iterator>
#include <algorithm>

#if defined(_WIN32)
#include <direct.h>
#define SUMI_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define SUMI_MKDIR(p) mkdir(p, 0755)
#endif

static const char* env_or(const char* name, const char* fallback) {
    const char* v = std::getenv(name);
    return (v && v[0]) ? v : fallback;
}

// DECISIONS_5 #85: a stored path must be UTF-8 (the process code page is UTF-8 on Windows).
// A 1.0/1.1 INI written by a user with a non-ASCII name holds its print_dir in the old
// ANSI code page instead - invalid UTF-8 - and is dropped for the default folder.
static bool utf8_valid(const std::string& s) {
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = (unsigned char)s[i];
        const size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (n == 0 || i + n > s.size()) return false;
        for (size_t k = 1; k < n; k++) if (((unsigned char)s[i + k] >> 6) != 0x2) return false;
        i += n;
    }
    return true;
}

static void mkdir_p(const std::string& path) {
    // Creates each component; errors (already exists) are ignored.
    std::string cur;
    for (size_t i = 0; i < path.size(); i++) {
        cur += path[i];
        if ((path[i] == '/' || path[i] == '\\') && cur.size() > 1) SUMI_MKDIR(cur.c_str());
    }
    SUMI_MKDIR(path.c_str());
}

std::string app_config_dir() {
    std::string dir;
#if defined(_WIN32)
    dir = std::string(env_or("APPDATA", ".")) + "\\midi-sink";
#elif defined(__APPLE__)
    dir = std::string(env_or("HOME", ".")) + "/Library/Application Support/midi-sink";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    dir = (xdg && xdg[0]) ? std::string(xdg) + "/midi-sink"
                          : std::string(env_or("HOME", ".")) + "/.config/midi-sink";
#endif
    mkdir_p(dir);
    return dir;
}

std::string app_pictures_dir() {
#if defined(_WIN32)
    return std::string(env_or("USERPROFILE", ".")) + "\\Pictures";
#else
    return std::string(env_or("HOME", ".")) + "/Pictures";
#endif
}

std::string app_settings_path() {
#if defined(_WIN32)
    return app_config_dir() + "\\settings.ini";
#else
    return app_config_dir() + "/settings.ini";
#endif
}

// #71: earlier DEFAULT maps, so an INI still carrying one verbatim (the map is
// persisted whole) follows the redesign instead of pinning the old layout
// forever. Anything that differs from these is the user's and is kept.
static const CcRoute kDefaultRoutesV1[] = {   // pre-#50 (imagined Airwave numbering)
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 20, 0}, {0xFF, 21, 1}, {0xFF, 22, 2}, {0xFF, 23, 3}, {0xFF, 24, 4}, {0xFF, 25, 5},
    {0xFF, 102, 7}, {0xFF, 103, 8},
};
static const CcRoute kDefaultRoutesV2[] = {   // #50 (measured pairs, material on the right hand)
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 26, 0}, {0xFF, 24, 1}, {0xFF, 22, 2}, {0xFF, 29, 3}, {0xFF, 30, 4}, {0xFF, 31, 5},
    {0xFF, 27, 7}, {0xFF, 28, 8}, {0xFF, 102, 7}, {0xFF, 103, 8},
};
static const CcRoute kDefaultRoutesV3[] = {   // #69 (symmetric hands) + the harness's ripple handles
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 26, 0}, {0xFF, 24, 1}, {0xFF, 22, 2}, {0xFF, 27, 9}, {0xFF, 25, 10}, {0xFF, 23, 11},
    {0xFF, 20, 12}, {0xFF, 21, 13}, {0xFF, 28, 8}, {0xFF, 29, 7}, {0xFF, 102, 7}, {0xFF, 103, 8},
};
static const CcRoute kDefaultRoutesV4[] = {   // Phase 6 step 36: + the torsion handles (CC 104/105)
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 26, 0}, {0xFF, 24, 1}, {0xFF, 22, 2}, {0xFF, 27, 9}, {0xFF, 25, 10}, {0xFF, 23, 11},
    {0xFF, 20, 12}, {0xFF, 21, 13}, {0xFF, 28, 8}, {0xFF, 29, 7}, {0xFF, 102, 7}, {0xFF, 103, 8},
    {0xFF, 104, 14}, {0xFF, 105, 15},
};
static const CcRoute kDefaultRoutesV5[] = {   // Phase 6 step 37: + the Chladni handles (CC 106/107)
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 26, 0}, {0xFF, 24, 1}, {0xFF, 22, 2}, {0xFF, 27, 9}, {0xFF, 25, 10}, {0xFF, 23, 11},
    {0xFF, 20, 12}, {0xFF, 21, 13}, {0xFF, 28, 8}, {0xFF, 29, 7}, {0xFF, 102, 7}, {0xFF, 103, 8},
    {0xFF, 104, 14}, {0xFF, 105, 15}, {0xFF, 106, 16}, {0xFF, 107, 17},
};
static const CcRoute kDefaultRoutesV6[] = {   // Phase 6 step 39: + the spark frequency handle (CC 108)
    {0xFF, 1, 0}, {0xFF, 2, 6}, {0xFF, 7, 6}, {0xFF, 11, 6},
    {0xFF, 26, 0}, {0xFF, 24, 1}, {0xFF, 22, 2}, {0xFF, 27, 9}, {0xFF, 25, 10}, {0xFF, 23, 11},
    {0xFF, 20, 12}, {0xFF, 21, 13}, {0xFF, 28, 8}, {0xFF, 29, 7}, {0xFF, 102, 7}, {0xFF, 103, 8},
    {0xFF, 104, 14}, {0xFF, 105, 15}, {0xFF, 106, 16}, {0xFF, 107, 17}, {0xFF, 108, 18},
};
static const int APP_CCMAP_VERSION = 7;        // Phase 6 step 40: + the Chirikov throw handle (CC 109)

static bool routes_equal_as_set(const std::vector<CcRoute>& a, const CcRoute* b, size_t nb) {
    if (a.size() != nb) return false;
    for (const CcRoute& r : a) {
        bool found = false;
        for (size_t i = 0; i < nb && !found; i++) {
            found = b[i].channel == r.channel && b[i].cc == r.cc && b[i].target == r.target;
        }
        if (!found) return false;
    }
    return true;
}

// The stock map of an older version, or not a stock map at all.
static bool routes_are_old_default(const std::vector<CcRoute>& routes) {
    return routes_equal_as_set(routes, kDefaultRoutesV1, sizeof(kDefaultRoutesV1) / sizeof(kDefaultRoutesV1[0])) ||
           routes_equal_as_set(routes, kDefaultRoutesV2, sizeof(kDefaultRoutesV2) / sizeof(kDefaultRoutesV2[0])) ||
           routes_equal_as_set(routes, kDefaultRoutesV3, sizeof(kDefaultRoutesV3) / sizeof(kDefaultRoutesV3[0])) ||
           routes_equal_as_set(routes, kDefaultRoutesV4, sizeof(kDefaultRoutesV4) / sizeof(kDefaultRoutesV4[0])) ||
           routes_equal_as_set(routes, kDefaultRoutesV5, sizeof(kDefaultRoutesV5) / sizeof(kDefaultRoutesV5[0])) ||
           routes_equal_as_set(routes, kDefaultRoutesV6, sizeof(kDefaultRoutesV6) / sizeof(kDefaultRoutesV6[0]));
}

void app_settings_default_routes(std::vector<CcRoute>& out) {
    out.clear();
    // The core's install_default_cc_map, verbatim (README "Default bindings").
    out.push_back({0xFF, 1,  SUMI_CTL_VORTEX_STRENGTH});   // mod wheel
    out.push_back({0xFF, 2,  SUMI_CTL_INK_FLOW});          // breath
    out.push_back({0xFF, 7,  SUMI_CTL_INK_FLOW});          // volume = breath alias
    out.push_back({0xFF, 11, SUMI_CTL_INK_FLOW});          // expression = breath alias
    // ROLI Airwave as measured (#50), laid out per the author (#69): each
    // hand stirs — Raise strength, Glide X, Slide Y (reversed: hand up =
    // centre up); left the vortex, right the Lamb-Oseen swirl. Grasp is the
    // pinch (saddle L, crossed R), Tilt the ripple (wavelength L, amount R).
    // Flex free (it cannot be played without disturbing the others).
    out.push_back({0xFF, 26, SUMI_CTL_VORTEX_STRENGTH});   // Raise L
    out.push_back({0xFF, 24, SUMI_CTL_VORTEX_X});          // Glide L
    out.push_back({0xFF, 22, SUMI_CTL_VORTEX_Y});          // Slide L
    out.push_back({0xFF, 27, SUMI_CTL_SWIRL_STRENGTH});    // Raise R
    out.push_back({0xFF, 25, SUMI_CTL_SWIRL_X});           // Glide R
    out.push_back({0xFF, 23, SUMI_CTL_SWIRL_Y});           // Slide R
    out.push_back({0xFF, 20, SUMI_CTL_PINCH_SADDLE});      // Grasp L
    out.push_back({0xFF, 21, SUMI_CTL_PINCH_CROSS});       // Grasp R
    out.push_back({0xFF, 28, SUMI_CTL_RIPPLE_FREQ});       // Tilt L
    out.push_back({0xFF, 29, SUMI_CTL_RIPPLE_AMP});        // Tilt R
    // The harness's ripple handles (the core ships these dims unmapped).
    out.push_back({0xFF, 102, SUMI_CTL_RIPPLE_AMP});
    out.push_back({0xFF, 103, SUMI_CTL_RIPPLE_FREQ});
    // Phase 6 step 36: the wave torsion's handles, the same way (unmapped in the core).
    out.push_back({0xFF, 104, SUMI_CTL_TORSION_K});
    out.push_back({0xFF, 105, SUMI_CTL_TORSION_PHASE});
    // Phase 6 step 37: the Chladni lattice's two amplitudes.
    out.push_back({0xFF, 106, SUMI_CTL_CHLADNI_A});
    out.push_back({0xFF, 107, SUMI_CTL_CHLADNI_B});
    // Phase 6 step 39: the spark shear's wavenumber (CC 74 takes it under slide_mode 2).
    out.push_back({0xFF, 108, SUMI_CTL_SPARK_K});
    // Phase 6 step 40: the Chirikov throw (the mod wheel takes it under the Anod table).
    out.push_back({0xFF, 109, SUMI_CTL_CHIRIKOV_K});
}

void app_settings_defaults(AppSettings& s, const sumi_params_t& core_defaults) {
    sumi_palette_preset(SUMI_MEDIUM_SUMI, 0u, &s.palette, nullptr);   // step 43: the custom slot starts as Sumi black
    s.params = core_defaults;
    app_settings_default_routes(s.cc_routes);
    s.ripple_amp_cc = 0;
    s.ripple_freq_cc = 32;
    s.chladni_a_cc = 0;
    s.chladni_b_cc = 0;
    s.spark_k_cc = 64;
    s.chirikov_k_cc = 0;
    s.first_run_dismissed = false;
    s.settings_open = true;
    s.print_dir = app_pictures_dir();
    s.sound = false;
    s.sound_gain = 0.8f;
    s.sound_sample.clear();
    s.sound_root = 60;
    s.sound_preset.clear();
    s.sound_source = 0;
    s.suzu_level = 0.25f; s.suzu_attack = 0.003f; s.suzu_release = 0.4f; s.suzu_cutoff = 20000.0f; s.suzu_resonance = 0.0f; s.suzu_shear = 0.0f; s.suzu_shear_kind = 0;
    s.suzu_voice_kind = 1; s.suzu_preset = 0; s.suzu_modes = 8; s.suzu_coupling = 0.05f; s.suzu_decay = 3.0f; s.suzu_decay_bright = 0.3f;
    s.suzu_stiffness = 0.0f; s.suzu_pluck = 0.28f; s.suzu_bow_onset = 0.15f; s.suzu_bow_position = 0.3f;
    s.suzu_string_nodes = 48; s.suzu_string_decay = 4.0f; s.suzu_pickup = 0.25f; s.suzu_bridge_hz = 220.0f; s.suzu_bridge_cells = 2;
    s.suzu_bridge_coupling = 0.002f; s.suzu_bridge_decay = 1.5f; s.suzu_loop_loss = 0.5f; s.suzu_duffing_beta = 8.0f; s.suzu_drive = 0.0f;
    s.suzu_drive_ratio = 1.0f; s.suzu_rotor_k = 0.3f; s.suzu_mod_target = 0; s.suzu_mod_depth = 0.5f; s.suzu_mod_rate = 1.0f;
    s.suzu_bore_nodes = 128; s.suzu_bore_loss = 0.3f; s.suzu_bore_corner = 1500.0f; s.suzu_jet_gain = 560.0f; s.suzu_jet_drive = 1.0f;
    s.suzu_jet_tau = 0.5f; s.suzu_jet_q = 1.0f; s.suzu_jet_noise = 0.02f; s.suzu_breath_ref = 0.44f; s.suzu_breath_range = 12.0f; s.suzu_bore_wall = 1.0f; s.suzu_press_blows = true;
    s.suzu_trace_scope = true; s.suzu_trace_ink = true; s.suzu_trace_kinds = 1 << 5; s.suzu_trace_scale = 0.25f; s.suzu_trace_segments = 6; s.suzu_trace_stroke = 0; s.suzu_trace_canvas = 0;   // step 59
    s.suzu_reed_hz = 12000.0f; s.suzu_reed_q = 0.7f; s.suzu_reed_open = 0.5f; s.suzu_reed_close = 3.0f; s.suzu_reed_area = 0.14f; s.suzu_reed_noise = 0.02f; s.suzu_cone_apex = 0.25f;
    s.suzu_lip_ratio = 0.95f; s.suzu_lip_q = 3.0f; s.suzu_lip_open = 0.05f; s.suzu_lip_close = 1.0f; s.suzu_lip_area = 0.5f; s.suzu_lip_range = 1.0f;
    s.suzu_partial = 3; s.suzu_bell_start = 0.6f; s.suzu_bell_gamma = 0.7f; s.suzu_brass = 0.5f;
}

int app_settings_route_for(const AppSettings& s, uint32_t target) {
    for (const CcRoute& r : s.cc_routes) {
        if (r.target == target) return r.cc;
    }
    return -1;
}

// ---- INI ------------------------------------------------------------------

static void put_f(std::ostream& o, const char* k, float v)   { o << k << "=" << v << "\n"; }
static void put_u(std::ostream& o, const char* k, uint32_t v){ o << k << "=" << v << "\n"; }
static void put_i(std::ostream& o, const char* k, int v)     { o << k << "=" << v << "\n"; }

bool app_settings_save(const AppSettings& s, const std::string& path) {
    app_preset_save_file(s, app_session_path(), "last session");   // step 43: the session as a preset — the shared format
    std::ostringstream o;
    o << "# midi-sink settings — written by the app; edit while it is closed.\n";
    const sumi_params_t& p = s.params;
    put_f(o, "viscosity", p.fluid_viscosity);
    put_f(o, "expansion", p.expansion_rate);
    put_f(o, "roughness", p.paper_roughness);
    put_f(o, "smoothing_ms", p.smoothing_ms);
    put_u(o, "palette", p.active_palette_id);
    // Phase 6 step 43 (QOL §1): the custom palette slot — stops as "r g b position", linear RGB
    put_u(o, "pal_count", s.palette.stop_count);
    for (uint32_t i = 0; i < SUMI_PALETTE_MAX_STOPS; i++)
        o << "pal_stop_" << i << "=" << s.palette.stops[i].rgb[0] << " " << s.palette.stops[i].rgb[1] << " " << s.palette.stops[i].rgb[2] << " " << s.palette.stops[i].position << "\n";
    put_f(o, "pal_gamma", s.palette.depth_gamma);
    put_f(o, "pal_floor", s.palette.depth_floor);
    put_f(o, "pal_drift", s.palette.hue_drift);
    o << "pal_accent=" << s.palette.accent_rgb[0] << " " << s.palette.accent_rgb[1] << " " << s.palette.accent_rgb[2] << "\n";
    o << "pal_clear=" << s.palette.clear_rgb[0] << " " << s.palette.clear_rgb[1] << " " << s.palette.clear_rgb[2] << "\n";
    put_u(o, "layout", p.pitch_layout);
    put_f(o, "sim_scale", p.sim_scale);
    put_f(o, "bpm", p.bpm);
    put_f(o, "roll_speed", p.roll_speed);
    put_u(o, "slide_mode", p.slide_mode);
    put_u(o, "vortex_profile", p.vortex_profile);
    put_u(o, "ripple_bake", p.ripple_bake);
    put_f(o, "ripple_angle", p.ripple_angle);
    put_u(o, "pinch_variant", p.pinch_variant);
    put_u(o, "bend_mode", p.bend_mode);
    put_u(o, "press_mode", p.press_mode);
    put_u(o, "input_mode", s.input_mode);
    put_u(o, "wake_profile", p.wake_profile);
    put_f(o, "wake_spread", p.wake_spread);
    put_u(o, "torsion_sweep", p.torsion_sweep);
    put_f(o, "chladni_cell", p.chladni_cell);
    put_u(o, "chladni_mode", p.chladni_mode);
    put_u(o, "trumpet_arc", p.trumpet_arc);
    put_u(o, "string_tuning", p.string_tuning);
    put_f(o, "burst_age", p.burst_age);
    put_f(o, "burst_life", p.burst_life);
    put_u(o, "burst_order", p.burst_order);
    put_u(o, "spark_stack", p.spark_stack);
    put_u(o, "spark_profile", p.spark_profile);
    put_f(o, "spark_shear", p.spark_shear);
    put_f(o, "spark_tau", p.spark_tau);
    put_i(o, "spark_k_cc", s.spark_k_cc);
    put_f(o, "chirikov_kmax", p.chirikov_kmax);
    put_u(o, "chirikov_periods", p.chirikov_periods);
    put_f(o, "chirikov_eps", p.chirikov_eps);
    put_i(o, "chirikov_k_cc", s.chirikov_k_cc);
    put_u(o, "medium", p.medium);
    put_f(o, "anod_glow", p.anod_glow);
    put_f(o, "anod_pitch", p.anod_pitch);
    o << "paper_tint=" << p.paper_tint[0] << " " << p.paper_tint[1] << " " << p.paper_tint[2] << "\n";   // step 43 (QOL §2), linear RGB
    put_f(o, "fiber_scale", p.fiber_scale);
    put_f(o, "anod_dark", p.anod_dark);
    put_f(o, "anod_grain", p.anod_grain);
    put_f(o, "anod_bloom", p.anod_bloom);
    put_u(o, "anod_bloom_levels", p.anod_bloom_levels);
    put_f(o, "anod_drop", p.anod_drop);
    put_i(o, "chladni_a_cc", s.chladni_a_cc);
    put_i(o, "chladni_b_cc", s.chladni_b_cc);
    put_i(o, "ripple_amp_cc", s.ripple_amp_cc);
    put_i(o, "ripple_freq_cc", s.ripple_freq_cc);
    put_i(o, "first_run_dismissed", s.first_run_dismissed ? 1 : 0);
    put_i(o, "settings_open", s.settings_open ? 1 : 0);
    put_i(o, "fullscreen", s.fullscreen ? 1 : 0);
    put_i(o, "sound", s.sound ? 1 : 0);
    put_f(o, "sound_gain", s.sound_gain);
    o << "sound_sample=" << s.sound_sample << "\n";
    put_i(o, "sound_root", s.sound_root);
    o << "sound_preset=" << s.sound_preset << "\n";
    put_i(o, "sound_source", s.sound_source);
    put_f(o, "suzu_level", s.suzu_level); put_f(o, "suzu_attack", s.suzu_attack); put_f(o, "suzu_release", s.suzu_release); put_f(o, "suzu_cutoff", s.suzu_cutoff);
    put_f(o, "suzu_resonance", s.suzu_resonance); put_f(o, "suzu_shear", s.suzu_shear); put_i(o, "suzu_shear_kind", s.suzu_shear_kind);
    put_i(o, "suzu_voice_kind", s.suzu_voice_kind); put_i(o, "suzu_preset", s.suzu_preset); put_i(o, "suzu_modes", s.suzu_modes);
    put_f(o, "suzu_coupling", s.suzu_coupling); put_f(o, "suzu_decay", s.suzu_decay); put_f(o, "suzu_decay_bright", s.suzu_decay_bright);
    put_f(o, "suzu_stiffness", s.suzu_stiffness); put_f(o, "suzu_pluck", s.suzu_pluck); put_f(o, "suzu_bow_onset", s.suzu_bow_onset); put_f(o, "suzu_bow_position", s.suzu_bow_position);
    put_i(o, "suzu_string_nodes", s.suzu_string_nodes); put_f(o, "suzu_string_decay", s.suzu_string_decay); put_f(o, "suzu_pickup", s.suzu_pickup);
    put_f(o, "suzu_bridge_hz", s.suzu_bridge_hz); put_i(o, "suzu_bridge_cells", s.suzu_bridge_cells); put_f(o, "suzu_bridge_coupling", s.suzu_bridge_coupling);
    put_f(o, "suzu_bridge_decay", s.suzu_bridge_decay); put_f(o, "suzu_loop_loss", s.suzu_loop_loss); put_f(o, "suzu_duffing_beta", s.suzu_duffing_beta);
    put_f(o, "suzu_drive", s.suzu_drive); put_f(o, "suzu_drive_ratio", s.suzu_drive_ratio); put_f(o, "suzu_rotor_k", s.suzu_rotor_k);
    put_i(o, "suzu_mod_target", s.suzu_mod_target); put_f(o, "suzu_mod_depth", s.suzu_mod_depth); put_f(o, "suzu_mod_rate", s.suzu_mod_rate);
    put_i(o, "suzu_bore_nodes", s.suzu_bore_nodes); put_f(o, "suzu_bore_loss", s.suzu_bore_loss); put_f(o, "suzu_bore_corner", s.suzu_bore_corner);
    put_f(o, "suzu_jet_gain", s.suzu_jet_gain); put_f(o, "suzu_jet_drive", s.suzu_jet_drive); put_f(o, "suzu_jet_tau", s.suzu_jet_tau); put_f(o, "suzu_jet_q", s.suzu_jet_q);
    put_f(o, "suzu_jet_noise", s.suzu_jet_noise); put_f(o, "suzu_breath_ref", s.suzu_breath_ref); put_f(o, "suzu_breath_range", s.suzu_breath_range); put_f(o, "suzu_bore_wall", s.suzu_bore_wall); put_i(o, "suzu_press_blows", s.suzu_press_blows ? 1 : 0);
    put_i(o, "suzu_trace_scope", s.suzu_trace_scope ? 1 : 0); put_i(o, "suzu_trace_ink", s.suzu_trace_ink ? 1 : 0); put_i(o, "suzu_trace_kinds", s.suzu_trace_kinds);   // step 59
    put_f(o, "suzu_trace_scale", s.suzu_trace_scale); put_i(o, "suzu_trace_segments", s.suzu_trace_segments); put_i(o, "suzu_trace_stroke", s.suzu_trace_stroke); put_i(o, "suzu_trace_canvas", s.suzu_trace_canvas);
    put_f(o, "suzu_reed_hz", s.suzu_reed_hz); put_f(o, "suzu_reed_q", s.suzu_reed_q); put_f(o, "suzu_reed_open", s.suzu_reed_open); put_f(o, "suzu_reed_close", s.suzu_reed_close);
    put_f(o, "suzu_reed_area", s.suzu_reed_area); put_f(o, "suzu_reed_noise", s.suzu_reed_noise); put_f(o, "suzu_cone_apex", s.suzu_cone_apex);
    put_f(o, "suzu_lip_ratio", s.suzu_lip_ratio); put_f(o, "suzu_lip_q", s.suzu_lip_q); put_f(o, "suzu_lip_open", s.suzu_lip_open); put_f(o, "suzu_lip_close", s.suzu_lip_close);
    put_f(o, "suzu_lip_area", s.suzu_lip_area); put_f(o, "suzu_lip_range", s.suzu_lip_range); put_i(o, "suzu_partial", s.suzu_partial);
    put_f(o, "suzu_bell_start", s.suzu_bell_start); put_f(o, "suzu_bell_gamma", s.suzu_bell_gamma); put_f(o, "suzu_brass", s.suzu_brass);
    o << "print_dir=" << s.print_dir << "\n";
    // #71: the layout generation of the DEFAULT map this file was written
    // against. A file carrying an older default set verbatim is upgraded on
    // load; a customised map is never touched.
    put_i(o, "ccmap_version", APP_CCMAP_VERSION);
    o << "ccmap=";
    for (size_t i = 0; i < s.cc_routes.size(); i++) {
        const CcRoute& r = s.cc_routes[i];
        if (i) o << ";";
        o << (unsigned)r.channel << ":" << (unsigned)r.cc << ":" << r.target;
    }
    o << "\n";
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << o.str();
    return (bool)f;
}

bool app_settings_load(AppSettings& s, const std::string& path) {
    // step 43 (QOL §3): the preset content comes from the session JSON when it exists (written by every save
    // since 1.1.0, the shared format); the INI still carries the app's own flags and, on the first launch
    // after the upgrade, the whole legacy settings — it is read after, and its preset keys give way to the
    // JSON's when the JSON was there.
    const bool json_ok = app_preset_load_file(s, app_session_path());
    const AppSettings keep = s;
    std::ifstream f(path, std::ios::binary);
    if (!f) return json_ok;
    std::string line;
    bool any = false;
    int ccmap_version = 1;   // absent = written before the key existed (#71)
    sumi_params_t& p = s.params;
    while (std::getline(f, line)) {
        // A settings.ini edited on Windows (Notepad, PowerShell) comes back CRLF: the binary-mode read
        // leaves the '\r', which would end up inside print_dir (DECISIONS_5 #84).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
        any = true;
        const float fv = (float)std::atof(v.c_str());
        const long lv = std::atol(v.c_str());
        if      (k == "viscosity")      p.fluid_viscosity = fv;
        else if (k == "expansion")      p.expansion_rate = fv;
        else if (k == "roughness")      p.paper_roughness = fv;
        else if (k == "smoothing_ms")   p.smoothing_ms = fv;
        else if (k == "palette")        p.active_palette_id = (uint32_t)lv % 4;   // step 43: 3 = the custom slot
        else if (k == "pal_count")      s.palette.stop_count = (uint32_t)(lv < 2 ? 2 : lv > 8 ? 8 : lv);
        else if (k.rfind("pal_stop_", 0) == 0) {
            const int idx = std::atoi(k.c_str() + 9);
            if (idx >= 0 && idx < (int)SUMI_PALETTE_MAX_STOPS) {
                float r = 0, g = 0, b = 0, pos = 0;
                if (std::sscanf(v.c_str(), "%f %f %f %f", &r, &g, &b, &pos) == 4) {
                    s.palette.stops[idx].rgb[0] = r; s.palette.stops[idx].rgb[1] = g; s.palette.stops[idx].rgb[2] = b; s.palette.stops[idx].position = pos;
                }
            }
        }
        else if (k == "pal_gamma")      s.palette.depth_gamma = fv;
        else if (k == "pal_floor")      s.palette.depth_floor = fv;
        else if (k == "pal_drift")      s.palette.hue_drift = fv;
        else if (k == "pal_accent")     std::sscanf(v.c_str(), "%f %f %f", &s.palette.accent_rgb[0], &s.palette.accent_rgb[1], &s.palette.accent_rgb[2]);
        else if (k == "pal_clear")      std::sscanf(v.c_str(), "%f %f %f", &s.palette.clear_rgb[0], &s.palette.clear_rgb[1], &s.palette.clear_rgb[2]);
        else if (k == "layout")         p.pitch_layout = (uint32_t)lv % 13;  // 13 layouts since step 61 (10 at step 60, 8 since #64; was % 6: rolls 6/7 reloaded as 0/1)
        else if (k == "sim_scale")      p.sim_scale = fv;
        else if (k == "bpm")            p.bpm = fv;
        else if (k == "roll_speed")     p.roll_speed = fv;
        else if (k == "slide_mode")     p.slide_mode = (lv == 255) ? SUMI_MODE_MEDIUM_DEFAULT : (uint32_t)(lv < 0 ? 0 : lv > 2 ? 2 : lv);
        else if (k == "vortex_profile") p.vortex_profile = lv == 3 ? 3u : (lv ? 1u : 0u);   // 0 exp, 1 rankine, 3 torsion (2 is gesture-only)
        else if (k == "ripple_bake")    p.ripple_bake = lv ? 1u : 0u;
        else if (k == "ripple_angle")   p.ripple_angle = fv;
        else if (k == "pinch_variant")  p.pinch_variant = lv ? 1u : 0u;
        else if (k == "bend_mode")      p.bend_mode = (lv == 255) ? SUMI_MODE_MEDIUM_DEFAULT : (uint32_t)(lv < 0 ? 0 : lv > 4 ? 4 : lv);   // step 43: 4 = the Chladni stir
        else if (k == "press_mode")     p.press_mode = (lv == 255) ? SUMI_MODE_MEDIUM_DEFAULT : (uint32_t)(lv < 0 ? 0 : lv > 2 ? 2 : lv);
        else if (k == "input_mode")     s.input_mode = (lv >= 1 && lv <= 3) ? (uint32_t)lv : 1u;
        else if (k == "wake_profile")   p.wake_profile = lv ? 1u : 0u;
        else if (k == "wake_spread")    p.wake_spread = fv < 1.5f ? 1.5f : (fv > 12.0f ? 12.0f : fv);
        else if (k == "torsion_sweep")  p.torsion_sweep = lv ? 1u : 0u;
        else if (k == "chladni_cell")   p.chladni_cell = fv < 0.5f ? 0.5f : (fv > 1.5f ? 1.5f : fv);
        else if (k == "chladni_mode")   p.chladni_mode = lv == 1 ? 1u : 0u;
        else if (k == "trumpet_arc")    p.trumpet_arc = lv ? 1u : 0u;   // step 60: the trumpet's partials on an arc
        else if (k == "string_tuning")  p.string_tuning = lv < 0 ? 0u : (lv > 2 ? 2u : (uint32_t)lv);   // step 61: the strings' tuning preset
        else if (k == "burst_age")      p.burst_age = fv < 1.5f ? 1.5f : (fv > 12.0f ? 12.0f : fv);
        else if (k == "burst_life")     p.burst_life = fv < 0.0f ? 0.0f : (fv > 4.0f ? 4.0f : fv);
        else if (k == "burst_order")    p.burst_order = (uint32_t)(lv < 2 ? 2 : lv > 8 ? 8 : lv);
        else if (k == "spark_stack")    p.spark_stack = (uint32_t)(lv < 1 ? 1 : lv > 4 ? 4 : lv);
        else if (k == "spark_profile")  p.spark_profile = lv ? 1u : 0u;
        else if (k == "spark_shear")    p.spark_shear = fv < 0.0f ? 0.0f : (fv > 2.0f ? 2.0f : fv);
        else if (k == "spark_tau")      p.spark_tau = fv < 0.05f ? 0.05f : (fv > 2.0f ? 2.0f : fv);
        else if (k == "spark_k_cc")     s.spark_k_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "chirikov_kmax")  p.chirikov_kmax = fv < 0.0f ? 0.0f : (fv > 2.0f ? 2.0f : fv);
        else if (k == "chirikov_periods") p.chirikov_periods = (uint32_t)(lv < 1 ? 1 : lv > 8 ? 8 : lv);
        else if (k == "chirikov_eps")   p.chirikov_eps = fv < 0.05f ? 0.05f : (fv > 1.0f ? 1.0f : fv);
        else if (k == "chirikov_k_cc")  s.chirikov_k_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "medium")         p.medium = lv == 1 ? 1u : 0u;
        else if (k == "anod_glow")      p.anod_glow = fv < 0.2f ? 0.2f : (fv > 5.0f ? 5.0f : fv);
        else if (k == "anod_pitch")     p.anod_pitch = !(fv > 0.0f) ? 0.0f : (fv < 1.0f / 256.0f ? 1.0f / 256.0f : (fv > 1.0f / 8.0f ? 1.0f / 8.0f : fv));
        else if (k == "paper_tint")     std::sscanf(v.c_str(), "%f %f %f", &p.paper_tint[0], &p.paper_tint[1], &p.paper_tint[2]);
        else if (k == "fiber_scale")    p.fiber_scale = fv < 0.5f ? 0.5f : (fv > 2.0f ? 2.0f : fv);
        else if (k == "anod_dark")      p.anod_dark = fv < 0.0f ? 0.0f : (fv > 1.0f ? 1.0f : fv);
        else if (k == "anod_grain")     p.anod_grain = fv < 0.0f ? 0.0f : (fv > 1.0f ? 1.0f : fv);
        else if (k == "anod_bloom")     p.anod_bloom = fv < 0.0f ? 0.0f : (fv > 3.0f ? 3.0f : fv);
        else if (k == "anod_bloom_levels") p.anod_bloom_levels = (uint32_t)(lv < 1 ? 1 : lv > 5 ? 5 : lv);
        else if (k == "anod_drop")      p.anod_drop = fv < 0.1f ? 0.1f : (fv > 1.0f ? 1.0f : fv);
        else if (k == "chladni_a_cc")   s.chladni_a_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "chladni_b_cc")   s.chladni_b_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "ripple_amp_cc")  s.ripple_amp_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "ripple_freq_cc") s.ripple_freq_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "first_run_dismissed") s.first_run_dismissed = lv != 0;
        else if (k == "settings_open")  s.settings_open = lv != 0;
        else if (k == "fullscreen")     s.fullscreen = lv != 0;
        else if (k == "sound")          s.sound = lv != 0;
        else if (k == "sound_gain")     s.sound_gain = fv < 0.0f ? 0.0f : fv > 1.5f ? 1.5f : fv;
        else if (k == "sound_sample")   { if (utf8_valid(v)) s.sound_sample = v; }
        else if (k == "sound_source")   s.sound_source = (int)(lv < 0 ? 0 : lv > 2 ? 2 : lv);
        else if (k == "suzu_level")     s.suzu_level = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_release")   s.suzu_release = fv < 0.005f ? 0.005f : fv > 20.0f ? 20.0f : fv;
        else if (k == "suzu_attack")    s.suzu_attack = fv < 0.0005f ? 0.0005f : fv > 2.0f ? 2.0f : fv;   // step 67
        else if (k == "suzu_cutoff")    s.suzu_cutoff = fv < 20.0f ? 20.0f : fv > 20000.0f ? 20000.0f : fv;
        else if (k == "suzu_resonance") s.suzu_resonance = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_shear")     s.suzu_shear = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_shear_kind") s.suzu_shear_kind = lv == 1 ? 1 : 0;
        else if (k == "suzu_voice_kind") s.suzu_voice_kind = (int)(lv < 0 ? 0 : lv > 8 ? 8 : lv);
        else if (k == "suzu_reed_hz") s.suzu_reed_hz = fv < 200.0f ? 200.0f : fv > 16000.0f ? 16000.0f : fv;
        else if (k == "suzu_reed_q") s.suzu_reed_q = fv < 0.2f ? 0.2f : fv > 10.0f ? 10.0f : fv;
        else if (k == "suzu_reed_open") s.suzu_reed_open = fv < 0.05f ? 0.05f : fv > 2.0f ? 2.0f : fv;
        else if (k == "suzu_reed_close") s.suzu_reed_close = fv < 0.5f ? 0.5f : fv > 10.0f ? 10.0f : fv;
        else if (k == "suzu_reed_area") s.suzu_reed_area = fv < 0.01f ? 0.01f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_reed_noise") s.suzu_reed_noise = fv < 0.0f ? 0.0f : fv > 0.5f ? 0.5f : fv;
        else if (k == "suzu_cone_apex") s.suzu_cone_apex = fv < 0.03f ? 0.03f : fv > 0.5f ? 0.5f : fv;
        else if (k == "suzu_lip_ratio") s.suzu_lip_ratio = fv < 0.3f ? 0.3f : fv > 2.0f ? 2.0f : fv;
        else if (k == "suzu_lip_q") s.suzu_lip_q = fv < 0.3f ? 0.3f : fv > 10.0f ? 10.0f : fv;
        else if (k == "suzu_lip_open") s.suzu_lip_open = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_lip_close") s.suzu_lip_close = fv < 0.2f ? 0.2f : fv > 10.0f ? 10.0f : fv;
        else if (k == "suzu_lip_area") s.suzu_lip_area = fv < 0.01f ? 0.01f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_lip_range") s.suzu_lip_range = fv < 0.0f ? 0.0f : fv > 2.0f ? 2.0f : fv;
        else if (k == "suzu_partial") s.suzu_partial = (int)(lv < 1 ? 1 : lv > 6 ? 6 : lv);
        else if (k == "suzu_bell_start") s.suzu_bell_start = fv < 0.2f ? 0.2f : fv > 0.95f ? 0.95f : fv;
        else if (k == "suzu_bell_gamma") s.suzu_bell_gamma = fv < 0.1f ? 0.1f : fv > 1.5f ? 1.5f : fv;
        else if (k == "suzu_brass") s.suzu_brass = fv < 0.0f ? 0.0f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_bore_nodes") s.suzu_bore_nodes = (int)(lv < 16 ? 16 : lv > 256 ? 256 : lv);
        else if (k == "suzu_bore_loss") s.suzu_bore_loss = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_bore_corner") s.suzu_bore_corner = fv < 0.0f ? 0.0f : fv > 8000.0f ? 8000.0f : fv;
        else if (k == "suzu_jet_gain") s.suzu_jet_gain = fv < 0.0f ? 0.0f : fv > 4000.0f ? 4000.0f : fv;
        else if (k == "suzu_jet_drive") s.suzu_jet_drive = fv < 0.0f ? 0.0f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_jet_tau") s.suzu_jet_tau = fv < 0.2f ? 0.2f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_jet_q") s.suzu_jet_q = fv < 0.3f ? 0.3f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_jet_noise") s.suzu_jet_noise = fv < 0.0f ? 0.0f : fv > 0.5f ? 0.5f : fv;
        else if (k == "suzu_breath_ref") s.suzu_breath_ref = fv < 0.1f ? 0.1f : fv > 0.9f ? 0.9f : fv;
        else if (k == "suzu_breath_range") s.suzu_breath_range = fv < 1.5f ? 1.5f : fv > 32.0f ? 32.0f : fv;
        else if (k == "suzu_bore_wall") s.suzu_bore_wall = fv < 0.0f ? 0.0f : fv > 30.0f ? 30.0f : fv;
        else if (k == "suzu_press_blows") s.suzu_press_blows = lv != 0;
        else if (k == "suzu_trace_scope") s.suzu_trace_scope = lv != 0;                                   // step 59
        else if (k == "suzu_trace_ink") s.suzu_trace_ink = lv != 0;
        else if (k == "suzu_trace_kinds") s.suzu_trace_kinds = (int)(lv < 0 ? 0 : lv > 511 ? 511 : lv);
        else if (k == "suzu_trace_scale") s.suzu_trace_scale = fv < 0.02f ? 0.02f : fv > 2.0f ? 2.0f : fv;
        else if (k == "suzu_trace_segments") s.suzu_trace_segments = (int)(lv < 4 ? 4 : lv > 8 ? 8 : lv);
        else if (k == "suzu_trace_stroke") s.suzu_trace_stroke = (int)(lv < 0 ? 0 : lv > 1 ? 1 : lv);
        else if (k == "suzu_trace_canvas") s.suzu_trace_canvas = (int)(lv < 0 ? 0 : lv > 2 ? 2 : lv);
        else if (k == "suzu_string_nodes") s.suzu_string_nodes = (int)(lv < 2 ? 2 : lv > 80 ? 80 : lv);
        else if (k == "suzu_string_decay") s.suzu_string_decay = fv < 0.0f ? 0.0f : fv > 30.0f ? 30.0f : fv;
        else if (k == "suzu_pickup")    s.suzu_pickup = fv < 0.02f ? 0.02f : fv > 0.5f ? 0.5f : fv;
        else if (k == "suzu_bridge_hz") s.suzu_bridge_hz = fv < 40.0f ? 40.0f : fv > 4000.0f ? 4000.0f : fv;
        else if (k == "suzu_bridge_cells") s.suzu_bridge_cells = (int)(lv < 1 ? 1 : lv > 3 ? 3 : lv);
        else if (k == "suzu_bridge_coupling") s.suzu_bridge_coupling = fv < 0.0f ? 0.0f : fv > 0.02f ? 0.02f : fv;
        else if (k == "suzu_bridge_decay") s.suzu_bridge_decay = fv < 0.0f ? 0.0f : fv > 30.0f ? 30.0f : fv;
        else if (k == "suzu_loop_loss") s.suzu_loop_loss = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_duffing_beta") s.suzu_duffing_beta = fv < 0.0f ? 0.0f : fv > 32.0f ? 32.0f : fv;
        // The key chain restarts here: MSVC caps nested blocks at 128 (C1061) and one else-if
        // chain of every key passed it at step 67 (DECISIONS_9 #8). The keys are distinct, so
        // two chains read the same as one; an unknown key still falls through both.
        if (k == "suzu_drive")          s.suzu_drive = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_drive_ratio") s.suzu_drive_ratio = fv < 0.25f ? 0.25f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_rotor_k")   s.suzu_rotor_k = fv < 0.0f ? 0.0f : fv > 2.5f ? 2.5f : fv;
        else if (k == "suzu_mod_target") s.suzu_mod_target = (int)(lv < 0 ? 0 : lv > 4 ? 4 : lv);
        else if (k == "suzu_mod_depth") s.suzu_mod_depth = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_mod_rate")  s.suzu_mod_rate = fv < 0.1f ? 0.1f : fv > 4.0f ? 4.0f : fv;
        else if (k == "suzu_preset")    s.suzu_preset = (int)(lv < 0 ? 0 : lv > 4 ? 4 : lv);
        else if (k == "suzu_modes")     s.suzu_modes = (int)(lv < 1 ? 1 : lv > 16 ? 16 : lv);
        else if (k == "suzu_coupling")  s.suzu_coupling = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "suzu_decay")     s.suzu_decay = fv < 0.05f ? 0.05f : fv > 30.0f ? 30.0f : fv;
        else if (k == "suzu_decay_bright") s.suzu_decay_bright = fv < 0.0f ? 0.0f : fv > 20.0f ? 20.0f : fv;
        else if (k == "suzu_stiffness") s.suzu_stiffness = fv < 0.0f ? 0.0f : fv > 0.2f ? 0.2f : fv;
        else if (k == "suzu_pluck")     s.suzu_pluck = fv < 0.01f ? 0.01f : fv > 0.5f ? 0.5f : fv;
        else if (k == "suzu_bow_onset") s.suzu_bow_onset = fv < 0.0f ? 0.0f : fv > 5.0f ? 5.0f : fv;
        else if (k == "suzu_bow_position") s.suzu_bow_position = fv < 0.0f ? 0.0f : fv > 1.0f ? 1.0f : fv;
        else if (k == "sound_root")     s.sound_root = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "sound_preset")   { if (utf8_valid(v)) s.sound_preset = v; }
        else if (k == "print_dir")      { if (!v.empty() && utf8_valid(v)) s.print_dir = v; }
        else if (k == "ccmap_version")  ccmap_version = (int)lv;
        else if (k == "ccmap") {
            std::vector<CcRoute> routes;
            std::stringstream ss(v);
            std::string item;
            while (std::getline(ss, item, ';')) {
                unsigned ch = 0, cc = 0, tg = 0;
                if (std::sscanf(item.c_str(), "%u:%u:%u", &ch, &cc, &tg) == 3 &&
                    cc <= 127 && (tg < SUMI_CTL_COUNT || (tg >= 1000u && tg < 1006u)) && (ch == 0xFF || ch < 16)) {   // step 54: Voxo's bus targets survive the reload
                    routes.push_back({(uint8_t)ch, (uint8_t)cc, tg});
                }
            }
            s.cc_routes = routes;   // an explicit empty map is a valid choice
        }
    }
    // #71: an INI written against an older DEFAULT map, still carrying that
    // map verbatim, follows the current defaults (#69). A customised map is
    // kept as it is — only the stock sets are recognised.
    if (ccmap_version < APP_CCMAP_VERSION && routes_are_old_default(s.cc_routes)) {
        app_settings_default_routes(s.cc_routes);
        std::printf("[settings] CC map was the stock map of an older version - upgraded to the "
                    "current default layout (DECISIONS_4 #69/#71)\n");
    }
    // Clamp what the core would otherwise reject or render badly.
    if (!(p.sim_scale > 0.0f) || p.sim_scale > 2.0f) p.sim_scale = 1.0f;
    if (p.bpm < 20.0f || p.bpm > 300.0f) p.bpm = 120.0f;
    if (!(p.roll_speed > 0.0f)) p.roll_speed = 0.0625f;
    if (json_ok) {   // the session JSON is the preset's truth; the INI keeps the window flags, the print folder, the hint
        s.params = keep.params; s.cc_routes = keep.cc_routes; s.palette = keep.palette; s.input_mode = keep.input_mode;
        s.ripple_amp_cc = keep.ripple_amp_cc; s.ripple_freq_cc = keep.ripple_freq_cc; s.chladni_a_cc = keep.chladni_a_cc;
        s.chladni_b_cc = keep.chladni_b_cc; s.spark_k_cc = keep.spark_k_cc; s.chirikov_k_cc = keep.chirikov_k_cc;
    }
    return any || json_ok;
}

void app_settings_apply(const AppSettings& s, sumi_instance_t* inst, void* midi) {
    if (!inst) return;
    sumi_set_params(inst, &s.params);
    sumi_set_palette(inst, &s.palette);   // step 43: the custom slot rides with the params (the core validates)
    // #60: the input dialect is the user's choice, never a heuristic.
    sumi_set_input_mode(inst, (sumi_input_mode_t)(s.input_mode >= 1 && s.input_mode <= 3 ? s.input_mode : 1u));
    sumi_clear_cc_map(inst);
    for (const CcRoute& r : s.cc_routes) {
        if (app_ctl_is_voxo(r.target)) continue;   // step 53: the bus routes are Voxo's (main.cpp's sound_apply)
        sumi_map_cc(inst, r.channel, r.cc, (sumi_ctl_t)r.target);
    }
    if (midi) {
        const int amp = app_settings_route_for(s, SUMI_CTL_RIPPLE_AMP);
        const int frq = app_settings_route_for(s, SUMI_CTL_RIPPLE_FREQ);
        if (amp >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)amp, (uint8_t)s.ripple_amp_cc);
        if (frq >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)frq, (uint8_t)s.ripple_freq_cc);
        // Phase 6 step 37: the Chladni amplitudes ride the same real ctl path.
        const int ca = app_settings_route_for(s, SUMI_CTL_CHLADNI_A);
        const int cb = app_settings_route_for(s, SUMI_CTL_CHLADNI_B);
        if (ca >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)ca, (uint8_t)s.chladni_a_cc);
        if (cb >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)cb, (uint8_t)s.chladni_b_cc);
        // Phase 6 step 39: the spark shear's wavenumber, the same way.
        const int sk = app_settings_route_for(s, SUMI_CTL_SPARK_K);
        if (sk >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)sk, (uint8_t)s.spark_k_cc);
        // Phase 6 step 40: the Chirikov throw's position (its changes are the throws).
        const int ck = app_settings_route_for(s, SUMI_CTL_CHIRIKOV_K);
        if (ck >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)ck, (uint8_t)s.chirikov_k_cc);
    }
}

// ---- names ----------------------------------------------------------------

const char* app_layout_name(uint32_t layout) {
    switch (layout) {
        case SUMI_LAYOUT_FIFTHS:      return "Circle of fifths";
        case SUMI_LAYOUT_CHROMA_GRID: return "Chromatic grid";
        case SUMI_LAYOUT_JANKO:       return "Janko";
        case SUMI_LAYOUT_ROLL_H:      return "Piano roll (left)";
        case SUMI_LAYOUT_ROLL_V:      return "Piano roll (top)";
        case SUMI_LAYOUT_PIANO_GRID:  return "Piano grid";
        case SUMI_LAYOUT_ROLL_H_RIGHT:  return "Piano roll (right)";
        case SUMI_LAYOUT_ROLL_V_BOTTOM: return "Piano roll (bottom)";
        case SUMI_LAYOUT_TRUMPET:       return "Trumpet (valves)";
        case SUMI_LAYOUT_TROMBONE:      return "Trombone (slide)";
        case SUMI_LAYOUT_WICKI:         return "Wicki-Hayden";
        case SUMI_LAYOUT_STRINGS:       return "Strings";
        case SUMI_LAYOUT_THEREMIN:      return "Theremin";
        default:                      return "?";
    }
}

const char* app_palette_name(uint32_t palette) {
    switch (palette) {
        case 0:  return "Sumi black";
        case 1:  return "Indigo";
        case 2:  return "Ochre";
        case 3:  return "Custom";
        default: return "?";
    }
}

uint32_t app_ctl_target_count() { return (uint32_t)SUMI_CTL_COUNT + 6u; }
uint32_t app_ctl_target_at(uint32_t index) { return index < (uint32_t)SUMI_CTL_COUNT ? index : 1000u + (index - (uint32_t)SUMI_CTL_COUNT); }
bool app_ctl_is_voxo(uint32_t ctl) { return ctl >= 1000u; }

const char* app_ctl_name(uint32_t ctl) {
    switch (ctl) {
        case 1000: return "Reverb amount";      // step 53 (#27): Voxo's bus
        case 1001: return "Reverb room";
        case 1002: return "Reverb damping";
        case 1003: return "Delay amount";
        case 1004: return "Delay time";
        case 1005: return "Delay feedback";
        case SUMI_CTL_VORTEX_STRENGTH: return "Vortex strength";
        case SUMI_CTL_VORTEX_X:        return "Vortex center X";
        case SUMI_CTL_VORTEX_Y:        return "Vortex center Y";
        case SUMI_CTL_VISCOSITY:       return "Viscosity";
        case SUMI_CTL_PAPER_ROUGHNESS: return "Paper roughness";
        case SUMI_CTL_PALETTE_MORPH:   return "Palette morph";
        case SUMI_CTL_INK_FLOW:        return "Ink flow (breath)";
        case SUMI_CTL_RIPPLE_AMP:      return "Ripple amount";
        case SUMI_CTL_RIPPLE_FREQ:     return "Ripple wavelength";
        case SUMI_CTL_SWIRL_STRENGTH:  return "Swirl strength";
        case SUMI_CTL_SWIRL_X:         return "Swirl center X";
        case SUMI_CTL_SWIRL_Y:         return "Swirl center Y";
        case SUMI_CTL_PINCH_SADDLE:    return "Pinch (saddle)";
        case SUMI_CTL_PINCH_CROSS:     return "Pinch (crossed tines)";
        case SUMI_CTL_TORSION_K:       return "Torsion wavelength";
        case SUMI_CTL_TORSION_PHASE:   return "Torsion phase";
        case SUMI_CTL_CHLADNI_A:       return "Chladni stir";
        case SUMI_CTL_CHLADNI_B:       return "Chladni balance";
        case SUMI_CTL_SPARK_K:         return "Spark frequency";
        case SUMI_CTL_CHIRIKOV_K:      return "Chirikov throw";
        default:                       return "?";
    }
}

// ---- Phase 6 step 43 (QOL §3): presets through the one serializer -------------
void app_settings_to_preset(const AppSettings& s, sumi_preset_t* out, const char* name) {
    if (!out) return;
    sumi_preset_init(out, &s.params, &s.palette);
    std::snprintf(out->name, sizeof out->name, "%s", name ? name : "");
    out->input_mode = s.input_mode;
    out->cc_count = 0;
    for (const CcRoute& r : s.cc_routes) {
        if (out->cc_count >= SUMI_PRESET_MAX_CC) break;
        out->cc[out->cc_count].channel = r.channel; out->cc[out->cc_count].cc = r.cc; out->cc[out->cc_count].target = r.target;
        out->cc_count++;
    }
    // the harness's control values, per routed dimension (a tablet's strip values live in the same list)
    const struct { uint32_t ctl; int value; } ctls[] = {
        {SUMI_CTL_RIPPLE_AMP, s.ripple_amp_cc}, {SUMI_CTL_RIPPLE_FREQ, s.ripple_freq_cc},
        {SUMI_CTL_CHLADNI_A, s.chladni_a_cc}, {SUMI_CTL_CHLADNI_B, s.chladni_b_cc},
        {SUMI_CTL_SPARK_K, s.spark_k_cc}, {SUMI_CTL_CHIRIKOV_K, s.chirikov_k_cc},
    };
    out->control_count = 0;
    for (const auto& c : ctls) {
        out->controls[out->control_count].ctl = c.ctl;
        out->controls[out->control_count].value = (uint8_t)(c.value < 0 ? 0 : c.value > 127 ? 127 : c.value);
        out->control_count++;
    }
    // Phase 8 step 57: Suzu's patch rides the preset (host-side numbers, SCHEMA.md `suzu`)
    out->suzu.source = (uint32_t)s.sound_source;
    out->suzu.level = s.suzu_level; out->suzu.attack_s = s.suzu_attack; out->suzu.release_s = s.suzu_release;
    out->suzu.cutoff_hz = s.suzu_cutoff; out->suzu.resonance = s.suzu_resonance; out->suzu.shear = s.suzu_shear;
    out->suzu.shear_kind = (uint32_t)s.suzu_shear_kind; out->suzu.voice_kind = (uint32_t)s.suzu_voice_kind;
    out->suzu.modal_preset = (uint32_t)s.suzu_preset; out->suzu.modes = (uint32_t)s.suzu_modes;
    out->suzu.coupling = s.suzu_coupling; out->suzu.decay_s = s.suzu_decay; out->suzu.decay_bright = s.suzu_decay_bright;
    out->suzu.stiffness = s.suzu_stiffness; out->suzu.pluck = s.suzu_pluck; out->suzu.bow_onset_s = s.suzu_bow_onset;
    out->suzu.bow_position = s.suzu_bow_position; out->suzu.breath_cc = 2;
    out->suzu.string_nodes = (uint32_t)s.suzu_string_nodes; out->suzu.string_decay_s = s.suzu_string_decay; out->suzu.pickup = s.suzu_pickup;
    out->suzu.bridge_hz = s.suzu_bridge_hz; out->suzu.bridge_cells = (uint32_t)s.suzu_bridge_cells; out->suzu.bridge_coupling = s.suzu_bridge_coupling;
    out->suzu.bridge_decay_s = s.suzu_bridge_decay; out->suzu.loop_loss = s.suzu_loop_loss; out->suzu.duffing_beta = s.suzu_duffing_beta;
    out->suzu.drive = s.suzu_drive; out->suzu.drive_ratio = s.suzu_drive_ratio; out->suzu.rotor_k = s.suzu_rotor_k;
    out->suzu.mod_target = (uint32_t)s.suzu_mod_target; out->suzu.mod_depth = s.suzu_mod_depth; out->suzu.mod_rate = s.suzu_mod_rate;
    out->suzu.bore_nodes = (uint32_t)s.suzu_bore_nodes; out->suzu.bore_loss = s.suzu_bore_loss; out->suzu.bore_corner_hz = s.suzu_bore_corner;
    out->suzu.jet_gain = s.suzu_jet_gain; out->suzu.jet_drive = s.suzu_jet_drive; out->suzu.jet_tau = s.suzu_jet_tau; out->suzu.jet_q = s.suzu_jet_q;
    out->suzu.jet_noise = s.suzu_jet_noise; out->suzu.breath_ref = s.suzu_breath_ref; out->suzu.breath_range = s.suzu_breath_range; out->suzu.bore_wall_s = s.suzu_bore_wall; out->suzu.press_blows = s.suzu_press_blows ? 1u : 0u;
    out->suzu.reed_hz = s.suzu_reed_hz; out->suzu.reed_q = s.suzu_reed_q; out->suzu.reed_open = s.suzu_reed_open; out->suzu.reed_close = s.suzu_reed_close;
    out->suzu.reed_area = s.suzu_reed_area; out->suzu.reed_noise = s.suzu_reed_noise; out->suzu.cone_apex = s.suzu_cone_apex;
    out->suzu.lip_ratio = s.suzu_lip_ratio; out->suzu.lip_q = s.suzu_lip_q; out->suzu.lip_open = s.suzu_lip_open; out->suzu.lip_close = s.suzu_lip_close;
    out->suzu.lip_area = s.suzu_lip_area; out->suzu.lip_range = s.suzu_lip_range; out->suzu.partial = (uint32_t)s.suzu_partial;
    out->suzu.bell_start = s.suzu_bell_start; out->suzu.bell_gamma = s.suzu_bell_gamma; out->suzu.brass = s.suzu_brass;
    out->suzu.trace_scope = s.suzu_trace_scope ? 1u : 0u; out->suzu.trace_ink = s.suzu_trace_ink ? 1u : 0u; out->suzu.trace_kinds = (uint32_t)s.suzu_trace_kinds;   // step 59
    out->suzu.trace_segments = (uint32_t)s.suzu_trace_segments; out->suzu.trace_stroke = (uint32_t)s.suzu_trace_stroke; out->suzu.trace_scale = s.suzu_trace_scale; out->suzu.trace_canvas = (uint32_t)s.suzu_trace_canvas;
    out->suzu_present = true;
}

void app_settings_from_preset(AppSettings& s, const sumi_preset_t& p) {
    s.params = p.params;
    s.palette = p.palette;
    s.input_mode = p.input_mode >= 1u && p.input_mode <= 3u ? p.input_mode : 1u;
    s.cc_routes.clear();
    for (uint32_t i = 0; i < p.cc_count && i < SUMI_PRESET_MAX_CC; i++) s.cc_routes.push_back({p.cc[i].channel, p.cc[i].cc, p.cc[i].target});
    for (uint32_t i = 0; i < p.control_count && i < SUMI_PRESET_MAX_CONTROLS; i++) {
        const int v = p.controls[i].value;
        switch (p.controls[i].ctl) {
            case SUMI_CTL_RIPPLE_AMP:  s.ripple_amp_cc = v; break;
            case SUMI_CTL_RIPPLE_FREQ: s.ripple_freq_cc = v; break;
            case SUMI_CTL_CHLADNI_A:   s.chladni_a_cc = v; break;
            case SUMI_CTL_CHLADNI_B:   s.chladni_b_cc = v; break;
            case SUMI_CTL_SPARK_K:     s.spark_k_cc = v; break;
            case SUMI_CTL_CHIRIKOV_K:  s.chirikov_k_cc = v; break;
            default: break;   // a tablet's strip values: not this shell's
        }
    }
    if (p.suzu_present) {   // step 57: the patch from the file (clamped where the setting is)
        s.sound_source = (int)(p.suzu.source > 2u ? 2u : p.suzu.source);
        s.suzu_level = p.suzu.level; s.suzu_attack = p.suzu.attack_s > 0.0f ? p.suzu.attack_s : s.suzu_attack; s.suzu_release = p.suzu.release_s; s.suzu_cutoff = p.suzu.cutoff_hz; s.suzu_resonance = p.suzu.resonance;
        s.suzu_shear = p.suzu.shear; s.suzu_shear_kind = p.suzu.shear_kind == 1u ? 1 : 0; s.suzu_voice_kind = (int)(p.suzu.voice_kind > 8u ? 8u : p.suzu.voice_kind);
        s.suzu_preset = (int)(p.suzu.modal_preset > 4u ? 4u : p.suzu.modal_preset); s.suzu_modes = (int)(p.suzu.modes < 1u ? 1u : p.suzu.modes > 16u ? 16u : p.suzu.modes);
        s.suzu_coupling = p.suzu.coupling; s.suzu_decay = p.suzu.decay_s; s.suzu_decay_bright = p.suzu.decay_bright; s.suzu_stiffness = p.suzu.stiffness;
        s.suzu_pluck = p.suzu.pluck; s.suzu_bow_onset = p.suzu.bow_onset_s; s.suzu_bow_position = p.suzu.bow_position;
        if (p.suzu.string_nodes) {   // step 58's fields (a step-57 file leaves them zero: the defaults stand)
            s.suzu_string_nodes = (int)(p.suzu.string_nodes < 2u ? 2u : p.suzu.string_nodes > 80u ? 80u : p.suzu.string_nodes);
            s.suzu_string_decay = p.suzu.string_decay_s; s.suzu_pickup = p.suzu.pickup; s.suzu_bridge_hz = p.suzu.bridge_hz;
            s.suzu_bridge_cells = (int)(p.suzu.bridge_cells < 1u ? 1u : p.suzu.bridge_cells > 3u ? 3u : p.suzu.bridge_cells);
            s.suzu_bridge_coupling = p.suzu.bridge_coupling; s.suzu_bridge_decay = p.suzu.bridge_decay_s; s.suzu_loop_loss = p.suzu.loop_loss;
            s.suzu_duffing_beta = p.suzu.duffing_beta; s.suzu_drive = p.suzu.drive; s.suzu_drive_ratio = p.suzu.drive_ratio; s.suzu_rotor_k = p.suzu.rotor_k;
            s.suzu_mod_target = (int)(p.suzu.mod_target > 4u ? 4u : p.suzu.mod_target); s.suzu_mod_depth = p.suzu.mod_depth; s.suzu_mod_rate = p.suzu.mod_rate;
        }
        if (p.suzu.bore_nodes) {   // step 58b's fields
            s.suzu_bore_nodes = (int)(p.suzu.bore_nodes < 16u ? 16u : p.suzu.bore_nodes > 256u ? 256u : p.suzu.bore_nodes);
            s.suzu_bore_loss = p.suzu.bore_loss; s.suzu_bore_corner = p.suzu.bore_corner_hz; s.suzu_jet_gain = p.suzu.jet_gain; s.suzu_jet_drive = p.suzu.jet_drive;
            s.suzu_jet_tau = p.suzu.jet_tau; s.suzu_jet_q = p.suzu.jet_q; s.suzu_jet_noise = p.suzu.jet_noise; s.suzu_breath_ref = p.suzu.breath_ref; s.suzu_breath_range = p.suzu.breath_range; if (p.suzu.bore_wall_s > 0.0f) { s.suzu_bore_wall = p.suzu.bore_wall_s; s.suzu_press_blows = p.suzu.press_blows != 0u; }
        }
        if (p.suzu.reed_hz > 0.0f) {   // step 58c's fields
            s.suzu_reed_hz = p.suzu.reed_hz; s.suzu_reed_q = p.suzu.reed_q; s.suzu_reed_open = p.suzu.reed_open; s.suzu_reed_close = p.suzu.reed_close;
            s.suzu_reed_area = p.suzu.reed_area; s.suzu_reed_noise = p.suzu.reed_noise; s.suzu_cone_apex = p.suzu.cone_apex;
            s.suzu_lip_ratio = p.suzu.lip_ratio; s.suzu_lip_q = p.suzu.lip_q; s.suzu_lip_open = p.suzu.lip_open; s.suzu_lip_close = p.suzu.lip_close;
            s.suzu_lip_area = p.suzu.lip_area; s.suzu_lip_range = p.suzu.lip_range; s.suzu_partial = (int)(p.suzu.partial < 1u ? 1u : p.suzu.partial > 6u ? 6u : p.suzu.partial);
            s.suzu_bell_start = p.suzu.bell_start; s.suzu_bell_gamma = p.suzu.bell_gamma; s.suzu_brass = p.suzu.brass;
        }
        if (p.suzu.trace_segments >= 4u) {   // step 59's fields (a file from before them holds zeros: the defaults stay)
            s.suzu_trace_scope = p.suzu.trace_scope != 0u; s.suzu_trace_ink = p.suzu.trace_ink != 0u; s.suzu_trace_kinds = (int)(p.suzu.trace_kinds > 511u ? 511u : p.suzu.trace_kinds);
            s.suzu_trace_segments = (int)(p.suzu.trace_segments > 8u ? 8u : p.suzu.trace_segments); s.suzu_trace_stroke = (int)(p.suzu.trace_stroke > 1u ? 1u : p.suzu.trace_stroke);
            s.suzu_trace_scale = p.suzu.trace_scale < 0.02f ? 0.02f : p.suzu.trace_scale > 2.0f ? 2.0f : p.suzu.trace_scale;
            s.suzu_trace_canvas = (int)(p.suzu.trace_canvas > 2u ? 2u : p.suzu.trace_canvas);
        }
    }
}

std::string app_session_path() { return app_config_dir() + "/last_session.json"; }
std::string app_presets_dir() {
    const std::string dir = app_config_dir() + "/presets";
    mkdir_p(dir);
    return dir;
}
std::string app_replays_dir() {
    const std::string dir = app_config_dir() + "/replays";
    mkdir_p(dir);
    return dir;
}
std::string app_settings_session_json(const AppSettings& s) {
    sumi_preset_t p;
    app_settings_to_preset(s, &p, "session");
    const size_t need = sumi_preset_write(&p, sumi_version(), nullptr, 0);
    std::string out(need + 1, '\0');
    sumi_preset_write(&p, sumi_version(), &out[0], need + 1);
    out.resize(need);
    return out;
}
std::string app_preset_path(const std::string& name) {
    std::string safe;
    for (char c : name) safe += (c == '/' || c == '\\' || c == ':' || c == '"' || c == '<' || c == '>' || c == '|' || c == '?' || c == '*') ? '_' : c;
    if (safe.empty()) safe = "preset";
    return app_presets_dir() + "/" + safe + ".json";
}
bool app_preset_save_file(const AppSettings& s, const std::string& path, const char* name) {
    sumi_preset_t p;
    app_settings_to_preset(s, &p, name);
    const size_t need = sumi_preset_write(&p, sumi_version(), nullptr, 0);
    std::string text(need + 1, '\0');
    sumi_preset_write(&p, sumi_version(), &text[0], text.size());
    text.resize(need);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << text;
    return (bool)f;
}
bool app_preset_load_file(AppSettings& s, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    sumi_preset_t p;
    app_settings_to_preset(s, &p, "");            // the session as it stands is the default a partial file falls back to
    if (!sumi_preset_read(text.c_str(), text.size(), &p)) return false;
    app_settings_from_preset(s, p);
    return true;
}
std::vector<std::string> app_preset_names() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(app_presets_dir(), ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        names.push_back(e.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}
