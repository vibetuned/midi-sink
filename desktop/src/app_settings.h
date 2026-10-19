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
    int   sound_source = 0;                 // Phase 8 step 56 (SYNTH §1): VOXO_SOURCE_* — 0 the sampler, 1 Suzu, the synth
    float suzu_level = 0.25f;               // step 56: Suzu's patch — the strike's orbit amplitude at velocity 127
    float suzu_attack = 0.003f;             //   step 67: the output ramp after the strike, seconds (the fitted patches set it)
    float suzu_release = 0.4f;              //   the declared contraction after note-off, seconds (a T60)
    float suzu_cutoff = 20000.0f;           //   the SVF's cutoff at CC 74 centre, Hz (20000 = bypassed)
    float suzu_resonance = 0.0f;            //   0..1, the declared dissipation you can hear
    float suzu_shear = 0.0f;                //   the phase-space shear's gain, 0..1
    int   suzu_shear_kind = 0;              //   0 cubic, 1 triangle fold
    int   suzu_voice_kind = 1;              // step 57: 0 one cell, 1 the modal lattice
    int   suzu_preset = 0;                  //   0 harmonic string, 1 stiff bar, 2 bell, 3 glass, 4 plucked string
    int   suzu_modes = 8;                   //   1..16
    float suzu_coupling = 0.05f;            //   κ, 0..1
    float suzu_decay = 3.0f;                //   the fundamental's T60 while held, seconds
    float suzu_decay_bright = 0.3f;         //   β: the extra decay rate per (r² − 1), 1/s
    float suzu_stiffness = 0.0f;            //   B for the string presets
    float suzu_pluck = 0.28f;               //   the pluck position, 0..0.5
    float suzu_bow_onset = 0.15f;           //   the bow's time constant, seconds (0 = no bow)
    float suzu_bow_position = 0.3f;         //   which partials the bow feeds, 0..1
    int   suzu_string_nodes = 48;           // step 58: the Verlet chain's nodes, 2..80
    int   suzu_oversample = 2;              // step 78: the cell family's section, 2 or 4
    float suzu_string_decay = 4.0f;         //   the chain's / the hybrid's T60 while held, seconds
    float suzu_pickup = 0.25f;              //   the pickup position, 0..0.5
    float suzu_bridge_hz = 220.0f;          //   the hybrid's bridge, Hz
    int   suzu_bridge_cells = 2;            //   1..3
    float suzu_bridge_coupling = 0.002f;    //   c, 0..0.02
    float suzu_bridge_decay = 1.5f;         //   the bridge's T60, seconds
    float suzu_loop_loss = 0.5f;            //   the KS averager, 0..1
    float suzu_duffing_beta = 8.0f;         //   the hardening spring, 0..32
    float suzu_drive = 0.0f;                //   the Duffing drive at full pressure, 0..1
    float suzu_drive_ratio = 1.0f;          //   the drive's frequency over the note's, 0.25..4
    float suzu_rotor_k = 0.3f;              //   K without the wheel, 0..2.5
    int   suzu_mod_target = 0;              //   0 none, 1 cutoff, 2 coupling, 3 rotor K, 4 drive
    float suzu_mod_depth = 0.5f;            //   0..1
    float suzu_mod_rate = 1.0f;             //   0.1..4
    int   suzu_bore_nodes = 128;            // step 58b: the flute's bore, cells at most
    float suzu_bore_loss = 0.3f;            //   the ends' radiation loss
    float suzu_bore_corner = 1500.0f;       //   Hz
    float suzu_jet_gain = 560.0f;
    float suzu_jet_drive = 1.0f;
    float suzu_jet_tau = 0.5f;              //   periods
    float suzu_jet_q = 1.0f;
    float suzu_jet_noise = 0.02f;
    float suzu_breath_ref = 0.44f;
    float suzu_breath_range = 12.0f;
    float suzu_bore_wall = 1.0f;            //   the wall loss, T60 s
    bool  suzu_press_blows = true;          //   the press blows the winds and the bow as breath does
    bool  suzu_trace_scope = true;          // step 59 (SYNTH §2.7): the orbit trace on the settings window's scope
    bool  suzu_trace_ink = true;            //   the gesture route: the orbit as segments into the water at the voice's cell
    int   suzu_trace_kinds = 1 << 5;        //   the voice kinds traced (bit k): the kicked rotor by default
    float suzu_trace_scale = 0.25f;         //   canvas heights per unit orbit amplitude
    int   suzu_trace_segments = 6;          //   segments per voice per frame, 4..8
    int   suzu_trace_stroke = 0;            //   0 tine (exact), 1 wake (sub-stepped)
    int   suzu_trace_canvas = 0;            //   the scope on the canvas (libsumi 1.3.0): 0 off, 1 over the water, 2 instead of it
    int   suzu_trace_mode = 0;              // step 75 (KESSHO §2.5): 0 continuous, 1 the Poincaré strobe on the bass phasor, 2 on each voice's own drive clock
    float suzu_reed_hz = 12000.0f;           // step 58c: the saxophone's reed
    float suzu_reed_q = 0.7f;
    float suzu_reed_open = 0.5f;
    float suzu_reed_close = 3.0f;
    float suzu_reed_area = 0.14f;
    float suzu_reed_noise = 0.02f;
    float suzu_cone_apex = 0.25f;
    float suzu_lip_ratio = 0.95f;            //   the trumpet's lips
    float suzu_lip_q = 3.0f;
    float suzu_lip_open = 0.05f;
    float suzu_lip_close = 1.0f;
    float suzu_lip_area = 0.5f;
    float suzu_lip_range = 1.0f;
    int   suzu_partial = 3;
    float suzu_bell_start = 0.6f;
    float suzu_bell_gamma = 0.7f;
    float suzu_brass = 0.5f;
    int   suzu_drum_size = 16;              // step 79: the membrane's nodes a side at most, 4..16
    float suzu_drum_kappa = 0.02f;          //   the biharmonic share, 0 a membrane … 1 a plate
    float suzu_drum_alpha = 0.0f;           //   the gong's swell at the cap, 0..4
    float suzu_drum_cap = 1.0f;             //   the strike-energy cap, in full strikes
    bool  suzu_plate_on = false;            //   the bus plate (the soundboard)
    int   suzu_plate_size = 64;             //   nodes a side, 16..64
    float suzu_plate_hz = 110.0f;
    float suzu_plate_kappa = 0.003f;
    float suzu_plate_decay = 2.0f;
    float suzu_plate_bright = 0.02f;
    float suzu_plate_gain = 1.0f;
    int   suzu_bow_mode = 0;                // step 80: 0 the energy servo, 1 the stick-slip bow
    float suzu_bow_force = 0.5f;            //   the bow's pressure, 0..4
    float suzu_bow_speed = 1.0f;            //   the bow's speed at full mouth, 0.25..4
    float suzu_reed_inertance = 0.0f;       // step 81: the reed channel's acoustic mass, 0..30 (0 none)
    float suzu_bore_leak = 0.4f;            //   the sax's bell's low-frequency leak, 0..1
    float suzu_brass_shock = 1.0f;          //   the crests' convective steepening, 0..4 (the brass shock)
    float suzu_lip_shear = 0.0f;            //   the lips' shearing mode's share, 0..1
    float suzu_lip_shear_ratio = 1.5f;      //   its resonance over the swing's, 0.25..4
    int   suzu_tonehole_mode = 0;           //   0 the bore cut per note, 1 the toneholes (the fingering keys)
    float suzu_hole_size = 1.0f;            //   the holes' size, 0.1..4
    float suzu_hole_loss = 0.3f;            //   the holes' radiation resistance, 0..2
    std::string sound_preset;               // step 50: the Decent Sampler .dspreset / .dslibrary Voxo plays (wins over the sample)
    std::string print_dir;                  // where "Save last print" writes
    sumi_palette_t palette{};               // Phase 6 step 43 (QOL §1): the custom palette slot (active_palette_id 3)
    sumi_mineral_t mineral{};               // 1.7.0 (Phase 11 step 71b, KESSHO §4): the mineral's optical half (Quartz by default)
    uint32_t       mineral_id = 0;          //   the library id it came from; SUMI_MINERAL_COUNT = edited by hand
};

// 1.7.0 (step 71b): a mineral from the library with BOTH halves — the core's optical descriptor
// (sumi_mineral_preset) and the mechanical pairing the shells carry (the habit, its roundness, the
// blades, the tine profile — KESSHO §4.6's table; the JSON presets carry the same). False past the
// library; `name` may be null. The params' other fields are untouched.
bool app_mineral_preset(uint32_t id, sumi_mineral_t* m, sumi_params_t* p, const char** name);

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
// Phase 9 step 65: the session as the serializer writes it (a recording's header and state events),
// and where the recordings live (<config dir>/replays, created).
std::string app_settings_session_json(const AppSettings& s);
std::string app_replays_dir();

// Human names for the UI.
const char* app_layout_name(uint32_t layout);     // 8 layouts (v0.8)
const char* app_palette_name(uint32_t palette);   // the three built-ins + the custom slot
const char* app_ctl_name(uint32_t ctl);           // sumi_ctl_t, or a VOXO_CTL_ (>= 1000, step 53)
// The CC map's target list as the picker shows it (step 53, DECISIONS_6 #27):
// the core's controls, then Voxo's bus targets from 1000. `index` 0..count-1.
uint32_t app_ctl_target_count();
uint32_t app_ctl_target_at(uint32_t index);
bool     app_ctl_is_voxo(uint32_t ctl);
