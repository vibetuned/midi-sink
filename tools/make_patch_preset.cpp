// tools/make_patch_preset.cpp — step 82: a patch table entry written as a preset file through the preset writer itself, so the
// file the gate pins the patch to carries every field as the engine states it. Build against libvoxo and sumi_presets:
//   clang++ -std=c++20 -I voxo/include -I presets/include -I core/include tools/make_patch_preset.cpp build-release/voxo/libvoxo.a build-release/presets/libsumi_presets.a -framework CoreFoundation -framework CoreAudio -framework AudioToolbox -o build/make_patch_preset
//   build/make_patch_preset presets/bowed_chain.json presets/<new>.json <index> "<name>"   (the source file lends the params block)
#include "voxo.h"
#include "sumi_preset.h"
#include <cstdio>
#include <cstring>
#include <string>
int main(int argc, char** argv) {
    const char* src_path = argv[1]; const char* out_path = argv[2]; const uint32_t index = (uint32_t)atoi(argv[3]); const char* name = argc > 4 ? argv[4] : "";
    FILE* f = std::fopen(src_path, "rb"); std::string text; char buf[4096]; size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n); std::fclose(f);
    sumi_preset_t p; std::memset(&p, 0, sizeof p);
    if (!sumi_preset_read(text.c_str(), 0, &p)) { std::printf("read failed\n"); return 1; }
    voxo_suzu_params_t sp; if (!voxo_suzu_patch(index, &sp)) { std::printf("no patch %u\n", index); return 1; }
    p.suzu.attack_s = sp.attack_s;
    p.suzu.bell_gamma = sp.bell_gamma;
    p.suzu.bell_start = sp.bell_start;
    p.suzu.bore_corner_hz = sp.bore_corner_hz;
    p.suzu.bore_leak = sp.bore_leak;
    p.suzu.bore_loss = sp.bore_loss;
    p.suzu.bore_nodes = sp.bore_nodes;
    p.suzu.bore_wall_s = sp.bore_wall_s;
    p.suzu.bow_force = sp.bow_force;
    p.suzu.bow_mode = sp.bow_mode;
    p.suzu.bow_onset_s = sp.bow_onset_s;
    p.suzu.bow_position = sp.bow_position;
    p.suzu.bow_speed = sp.bow_speed;
    p.suzu.brass = sp.brass;
    p.suzu.brass_shock = sp.brass_shock;
    p.suzu.breath_cc = sp.breath_cc;
    p.suzu.breath_range = sp.breath_range;
    p.suzu.breath_ref = sp.breath_ref;
    p.suzu.bridge_cells = sp.bridge_cells;
    p.suzu.bridge_coupling = sp.bridge_coupling;
    p.suzu.bridge_decay_s = sp.bridge_decay_s;
    p.suzu.bridge_hz = sp.bridge_hz;
    p.suzu.cone_apex = sp.cone_apex;
    p.suzu.coupling = sp.coupling;
    p.suzu.cutoff_hz = sp.cutoff_hz;
    p.suzu.decay_bright = sp.decay_bright;
    p.suzu.decay_s = sp.decay_s;
    p.suzu.drive = sp.drive;
    p.suzu.drive_ratio = sp.drive_ratio;
    p.suzu.drum_alpha = sp.drum_alpha;
    p.suzu.drum_cap = sp.drum_cap;
    p.suzu.drum_kappa = sp.drum_kappa;
    p.suzu.drum_size = sp.drum_size;
    p.suzu.duffing_beta = sp.duffing_beta;
    p.suzu.hole_loss = sp.hole_loss;
    p.suzu.hole_size = sp.hole_size;
    p.suzu.jet_area = sp.jet_area;
    p.suzu.jet_drive = sp.jet_drive;
    p.suzu.jet_gain = sp.jet_gain;
    p.suzu.jet_noise = sp.jet_noise;
    p.suzu.jet_offset = sp.jet_offset;
    p.suzu.jet_q = sp.jet_q;
    p.suzu.jet_tau = sp.jet_tau;
    p.suzu.level = sp.level;
    p.suzu.lip_area = sp.lip_area;
    p.suzu.lip_close = sp.lip_close;
    p.suzu.lip_open = sp.lip_open;
    p.suzu.lip_q = sp.lip_q;
    p.suzu.lip_range = sp.lip_range;
    p.suzu.lip_ratio = sp.lip_ratio;
    p.suzu.lip_shear = sp.lip_shear;
    p.suzu.lip_shear_ratio = sp.lip_shear_ratio;
    p.suzu.loop_loss = sp.loop_loss;
    p.suzu.mod_depth = sp.mod_depth;
    p.suzu.mod_rate = sp.mod_rate;
    p.suzu.mod_target = sp.mod_target;
    p.suzu.modal_comp = sp.modal_comp;
    p.suzu.modal_preset = sp.modal_preset;
    for (int k = 0; k < 16; k++) p.suzu.modal_ratio[k] = sp.modal_ratio[k];
    for (int k = 0; k < 16; k++) p.suzu.modal_t60[k] = sp.modal_t60[k];
    for (int k = 0; k < 16; k++) p.suzu.modal_weight[k] = sp.modal_weight[k];
    p.suzu.modes = sp.modes;
    p.suzu.oversample = sp.oversample;
    p.suzu.partial = sp.partial;
    p.suzu.pickup = sp.pickup;
    p.suzu.plate_bright = sp.plate_bright;
    p.suzu.plate_decay_s = sp.plate_decay_s;
    p.suzu.plate_gain = sp.plate_gain;
    p.suzu.plate_hz = sp.plate_hz;
    p.suzu.plate_kappa = sp.plate_kappa;
    p.suzu.plate_on = sp.plate_on;
    p.suzu.plate_size = sp.plate_size;
    p.suzu.pluck = sp.pluck;
    p.suzu.press_blows = sp.press_blows;
    p.suzu.reed_area = sp.reed_area;
    p.suzu.reed_close = sp.reed_close;
    p.suzu.reed_hz = sp.reed_hz;
    p.suzu.reed_inertance = sp.reed_inertance;
    p.suzu.reed_noise = sp.reed_noise;
    p.suzu.reed_open = sp.reed_open;
    p.suzu.reed_q = sp.reed_q;
    p.suzu.release_s = sp.release_s;
    p.suzu.resonance = sp.resonance;
    p.suzu.rotor_k = sp.rotor_k;
    p.suzu.shear = sp.shear;
    p.suzu.shear_kind = sp.shear_kind;
    p.suzu.stiffness = sp.stiffness;
    p.suzu.string_decay_s = sp.string_decay_s;
    p.suzu.string_nodes = sp.string_nodes;
    p.suzu.tonehole_mode = sp.tonehole_mode;
    p.suzu.toda_nodes = sp.toda_nodes; p.suzu.toda_b = sp.toda_b; p.suzu.toda_cap = sp.toda_cap; p.suzu.toda_width = sp.toda_width;   // step 83
    p.suzu.boris_b = sp.boris_b; p.suzu.boris_b_rest = sp.boris_b_rest;   // step 84
    p.suzu.voice_kind = sp.voice_kind;
    p.suzu.source = 1u; p.suzu_present = true;
    std::snprintf(p.name, sizeof p.name, "%s", name);
    char out[65536]; const size_t len = sumi_preset_write(&p, p.sumi_version, out, sizeof out);
    FILE* o = std::fopen(out_path, "wb"); std::fwrite(out, 1, len, o); std::fclose(o);
    std::printf("wrote %zu bytes, %zu fields copied\n", len, (size_t)97);
    return 0;
}
