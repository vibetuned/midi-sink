// app_settings.h — the desktop app's persisted settings (DECISIONS_4 #4, #7).
// One plain INI in the platform config directory; a host-side mirror of the
// core params and of the CC map (the core has no map readback and is frozen,
// so the mirror IS the map: it is applied as clear + one sumi_map_cc per
// route). Shared by all three desktop platforms.
#pragma once

#include "sumi_core.h"
#include "sumi_preset.h"   // Phase 6 step 43 (QOL §3): the shared preset serializer

#include <string>
#include <vector>

struct CcRoute {
    uint8_t  channel;   // 0xFF = any channel
    uint8_t  cc;        // 0..127
    uint32_t target;    // sumi_ctl_t
};

struct AppSettings {
    sumi_params_t        params{};          // mirror of the core params
    std::vector<CcRoute> cc_routes;         // the CC map mirror
    int  ripple_amp_cc  = 0;                // 0..127, sent on the RIPPLE_AMP route
    int  ripple_freq_cc = 32;               // 0..127, sent on the RIPPLE_FREQ route
    int  chladni_a_cc = 0;                  // Phase 6 step 37: 0..127, sent on the CHLADNI_A route
    int  chladni_b_cc = 0;                  //   and the CHLADNI_B route
    int  spark_k_cc = 64;                   // Phase 6 step 39: 0..127, sent on the SPARK_K route
    int  chirikov_k_cc = 0;                 // Phase 6 step 40: 0..127, sent on the CHIRIKOV_K route (its changes are the throws)
    bool first_run_dismissed = false;       // the spec's one dismissible hint
    bool settings_open = true;              // settings window shown at launch
    bool fullscreen = false;                // #58: canvas fills its monitor
    uint32_t input_mode = 1;                // #60: sumi_input_mode_t — 1 MPE (default), 2 classic, 3 wind
    bool  sound = false;                    // Phase 7 step 47 (SOUND §1): Voxo, the internal sound; OFF = the controller alone
    float sound_gain = 0.8f;                // Voxo's master gain, 0..1.5
    std::string sound_sample;               // step 49: the WAV Voxo plays (empty = the sine)
    int  sound_root = 60;                   // the sample's root note (MIDI), 0..127
    std::string sound_preset;               // step 50: the Decent Sampler .dspreset / .dslibrary Voxo plays (wins over the sample)
    std::string print_dir;                  // where "Save last print" writes
    sumi_palette_t palette{};               // Phase 6 step 43 (QOL §1): the custom palette slot (active_palette_id 3)
};

// Platform config directory (created if missing), e.g.
// ~/Library/Application Support/midi-sink on macOS.
std::string app_config_dir();
// ~/Pictures (or the home directory) — the default print folder.
std::string app_pictures_dir();
std::string app_settings_path();            // <config dir>/settings.ini

// The documented default CC map (README table) plus the harness's two ripple
// routes (CC 102/103, DECISIONS_3 #32).
void app_settings_default_routes(std::vector<CcRoute>& out);
// Defaults for a fresh install: the core's own params + the default routes.
void app_settings_defaults(AppSettings& s, const sumi_params_t& core_defaults);

bool app_settings_load(AppSettings& s, const std::string& path);
bool app_settings_save(const AppSettings& s, const std::string& path);

// Push the mirror into the core: params, CC map (clear + re-map), and the
// ripple CC values through the harness producer (so they ride the real ctl
// path). Safe to call every time anything changes.
void app_settings_apply(const AppSettings& s, sumi_instance_t* inst, void* midi);

// First CC routed (on any channel) to `target`, or -1.
int  app_settings_route_for(const AppSettings& s, uint32_t target);

// Phase 6 step 43 (QOL §3): PRESETS through the one serializer. The session's
// preset content (params, input mode, custom palette, CC map, the harness's
// control values) as a sumi_preset_t and back; the last session lives at
// <config>/last_session.json (read before the INI, written beside it), named
// presets at <config>/presets/<name>.json; export/import are the same file
// anywhere.
void app_settings_to_preset(const AppSettings& s, sumi_preset_t* out, const char* name);
void app_settings_from_preset(AppSettings& s, const sumi_preset_t& p);
std::string app_session_path();                        // <config dir>/last_session.json
std::string app_presets_dir();                         // <config dir>/presets (created)
std::string app_preset_path(const std::string& name);  // <presets dir>/<name>.json (the name sanitised)
bool app_preset_save_file(const AppSettings& s, const std::string& path, const char* name);
bool app_preset_load_file(AppSettings& s, const std::string& path);
std::vector<std::string> app_preset_names();           // the saved presets, sorted

// Human names for the UI.
const char* app_layout_name(uint32_t layout);     // 8 layouts (v0.8)
const char* app_palette_name(uint32_t palette);   // the three built-ins + the custom slot
const char* app_ctl_name(uint32_t ctl);           // sumi_ctl_t, or a VOXO_CTL_ (>= 1000, step 53)
// The CC map's target list as the picker shows it (step 53, DECISIONS_6 #27):
// the core's controls, then Voxo's bus targets from 1000. `index` 0..count-1.
uint32_t app_ctl_target_count();
uint32_t app_ctl_target_at(uint32_t index);
bool     app_ctl_is_voxo(uint32_t ctl);
