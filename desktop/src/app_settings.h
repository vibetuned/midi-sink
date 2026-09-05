// app_settings.h — the desktop app's persisted settings (DECISIONS_4 #4, #7).
// One plain INI in the platform config directory; a host-side mirror of the
// core params and of the CC map (the core has no map readback and is frozen,
// so the mirror IS the map: it is applied as clear + one sumi_map_cc per
// route). Shared by all three desktop platforms.
#pragma once

#include "sumi_core.h"

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
    bool first_run_dismissed = false;       // the spec's one dismissible hint
    bool settings_open = true;              // settings window shown at launch
    std::string print_dir;                  // where "Save last print" writes
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

// Human names for the UI.
const char* app_layout_name(uint32_t layout);     // 6 layouts
const char* app_palette_name(uint32_t palette);   // 3 palettes
const char* app_ctl_name(uint32_t ctl);           // sumi_ctl_t
