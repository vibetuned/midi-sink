// app_settings.cpp — INI persistence + the params / CC-map mirror.
#include "app_settings.h"
#include "midi_harness.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

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

void app_settings_default_routes(std::vector<CcRoute>& out) {
    out.clear();
    // The core's install_default_cc_map, verbatim (README "Default bindings").
    out.push_back({0xFF, 1,  SUMI_CTL_VORTEX_STRENGTH});   // mod wheel
    out.push_back({0xFF, 2,  SUMI_CTL_INK_FLOW});          // breath
    out.push_back({0xFF, 7,  SUMI_CTL_INK_FLOW});          // volume = breath alias
    out.push_back({0xFF, 11, SUMI_CTL_INK_FLOW});          // expression = breath alias
    // ROLI Airwave as measured (DECISIONS_4 #50): Grasp 20/21, Slide 22/23,
    // Glide 24/25, Raise 26/27, Tilt 28/29, Flex 30/31 (left/right).
    out.push_back({0xFF, 26, SUMI_CTL_VORTEX_STRENGTH});   // Raise L
    out.push_back({0xFF, 24, SUMI_CTL_VORTEX_X});          // Glide L
    out.push_back({0xFF, 22, SUMI_CTL_VORTEX_Y});          // Slide L
    out.push_back({0xFF, 29, SUMI_CTL_VISCOSITY});         // Tilt R
    out.push_back({0xFF, 30, SUMI_CTL_PAPER_ROUGHNESS});   // Flex L
    out.push_back({0xFF, 31, SUMI_CTL_PALETTE_MORPH});     // Flex R
    out.push_back({0xFF, 27, SUMI_CTL_RIPPLE_AMP});        // Raise R
    out.push_back({0xFF, 28, SUMI_CTL_RIPPLE_FREQ});       // Tilt L
    // The harness's ripple handles (the core ships these dims unmapped).
    out.push_back({0xFF, 102, SUMI_CTL_RIPPLE_AMP});
    out.push_back({0xFF, 103, SUMI_CTL_RIPPLE_FREQ});
}

void app_settings_defaults(AppSettings& s, const sumi_params_t& core_defaults) {
    s.params = core_defaults;
    app_settings_default_routes(s.cc_routes);
    s.ripple_amp_cc = 0;
    s.ripple_freq_cc = 32;
    s.first_run_dismissed = false;
    s.settings_open = true;
    s.print_dir = app_pictures_dir();
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
    std::ostringstream o;
    o << "# midi-sink settings — written by the app; edit while it is closed.\n";
    const sumi_params_t& p = s.params;
    put_f(o, "viscosity", p.fluid_viscosity);
    put_f(o, "expansion", p.expansion_rate);
    put_f(o, "roughness", p.paper_roughness);
    put_f(o, "smoothing_ms", p.smoothing_ms);
    put_u(o, "palette", p.active_palette_id);
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
    put_u(o, "wake_profile", p.wake_profile);
    put_f(o, "wake_spread", p.wake_spread);
    put_i(o, "ripple_amp_cc", s.ripple_amp_cc);
    put_i(o, "ripple_freq_cc", s.ripple_freq_cc);
    put_i(o, "first_run_dismissed", s.first_run_dismissed ? 1 : 0);
    put_i(o, "settings_open", s.settings_open ? 1 : 0);
    o << "print_dir=" << s.print_dir << "\n";
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
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::string line;
    bool any = false;
    sumi_params_t& p = s.params;
    while (std::getline(f, line)) {
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
        else if (k == "palette")        p.active_palette_id = (uint32_t)lv % 3;
        else if (k == "layout")         p.pitch_layout = (uint32_t)lv % 6;
        else if (k == "sim_scale")      p.sim_scale = fv;
        else if (k == "bpm")            p.bpm = fv;
        else if (k == "roll_speed")     p.roll_speed = fv;
        else if (k == "slide_mode")     p.slide_mode = lv ? 1u : 0u;
        else if (k == "vortex_profile") p.vortex_profile = lv ? 1u : 0u;
        else if (k == "ripple_bake")    p.ripple_bake = lv ? 1u : 0u;
        else if (k == "ripple_angle")   p.ripple_angle = fv;
        else if (k == "pinch_variant")  p.pinch_variant = lv ? 1u : 0u;
        else if (k == "bend_mode")      p.bend_mode = lv ? 1u : 0u;
        else if (k == "press_mode")     p.press_mode = lv ? 1u : 0u;
        else if (k == "wake_profile")   p.wake_profile = lv ? 1u : 0u;
        else if (k == "wake_spread")    p.wake_spread = fv < 1.5f ? 1.5f : (fv > 12.0f ? 12.0f : fv);
        else if (k == "ripple_amp_cc")  s.ripple_amp_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "ripple_freq_cc") s.ripple_freq_cc = (int)(lv < 0 ? 0 : lv > 127 ? 127 : lv);
        else if (k == "first_run_dismissed") s.first_run_dismissed = lv != 0;
        else if (k == "settings_open")  s.settings_open = lv != 0;
        else if (k == "print_dir")      { if (!v.empty()) s.print_dir = v; }
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
    // Clamp what the core would otherwise reject or render badly.
    if (!(p.sim_scale > 0.0f) || p.sim_scale > 2.0f) p.sim_scale = 1.0f;
    if (p.bpm < 20.0f || p.bpm > 300.0f) p.bpm = 120.0f;
    if (!(p.roll_speed > 0.0f)) p.roll_speed = 0.0625f;
    return any;
}

void app_settings_apply(const AppSettings& s, sumi_instance_t* inst, void* midi) {
    if (!inst) return;
    sumi_set_params(inst, &s.params);
    sumi_clear_cc_map(inst);
    for (const CcRoute& r : s.cc_routes) {
        sumi_map_cc(inst, r.channel, r.cc, (sumi_ctl_t)r.target);
    }
    if (midi) {
        const int amp = app_settings_route_for(s, SUMI_CTL_RIPPLE_AMP);
        const int frq = app_settings_route_for(s, SUMI_CTL_RIPPLE_FREQ);
        if (amp >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)amp, (uint8_t)s.ripple_amp_cc);
        if (frq >= 0) sumi_midi_harness_inject(midi, 0xB0, (uint8_t)frq, (uint8_t)s.ripple_freq_cc);
    }
}

// ---- names ----------------------------------------------------------------

const char* app_layout_name(uint32_t layout) {
    switch (layout) {
        case SUMI_LAYOUT_FIFTHS:      return "Circle of fifths";
        case SUMI_LAYOUT_CHROMA_GRID: return "Chromatic grid";
        case SUMI_LAYOUT_JANKO:       return "Janko";
        case SUMI_LAYOUT_ROLL_H:      return "Piano roll (horizontal)";
        case SUMI_LAYOUT_ROLL_V:      return "Piano roll (vertical)";
        case SUMI_LAYOUT_PIANO_GRID:  return "Piano grid";
        default:                      return "?";
    }
}

const char* app_palette_name(uint32_t palette) {
    switch (palette) {
        case 0:  return "Sumi black";
        case 1:  return "Indigo";
        case 2:  return "Ochre";
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
        default:                       return "?";
    }
}
