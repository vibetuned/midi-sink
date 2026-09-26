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
        else if (k == "layout")         p.pitch_layout = (uint32_t)lv % 8;   // 8 layouts since #64 (was % 6: rolls 6/7 reloaded as 0/1)
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
        else if (k == "sound_root")     s.sound_root = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "print_dir")      { if (!v.empty() && utf8_valid(v)) s.print_dir = v; }
        else if (k == "ccmap_version")  ccmap_version = (int)lv;
        else if (k == "ccmap") {
            std::vector<CcRoute> routes;
            std::stringstream ss(v);
            std::string item;
            while (std::getline(ss, item, ';')) {
                unsigned ch = 0, cc = 0, tg = 0;
                if (std::sscanf(item.c_str(), "%u:%u:%u", &ch, &cc, &tg) == 3 &&
                    cc <= 127 && tg < SUMI_CTL_COUNT && (ch == 0xFF || ch < 16)) {
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

const char* app_ctl_name(uint32_t ctl) {
    switch (ctl) {
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
}

std::string app_session_path() { return app_config_dir() + "/last_session.json"; }
std::string app_presets_dir() {
    const std::string dir = app_config_dir() + "/presets";
    mkdir_p(dir);
    return dir;
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
