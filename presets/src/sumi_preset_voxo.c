/* sumi_preset_voxo.c — step 82 (TO_PROJECT_SPEC §13.12): a preset file's Suzu block into Voxo's params, with the rule
 * every shell applied by hand until now — a field group a file from before its step holds as zeros leaves the defaults
 * standing (its sentinel named per group). Pure C11 over the two headers; links nothing (the shell hands the result to
 * voxo_set_suzu_params). The desktop's settings keep their own copy of this mapping (app_settings.cpp), which this
 * function mirrors; the tablets take their patch files through here. */
#include "sumi_preset_voxo.h"

void sumi_preset_suzu_to_voxo(const sumi_preset_t* p, voxo_suzu_params_t* out) {
    voxo_suzu_default_params(out);
    if (!p || !p->suzu_present) return;
    /* step 57: the base block, always carried */
    out->level = p->suzu.level; if (p->suzu.attack_s > 0.0f) out->attack_s = p->suzu.attack_s; out->release_s = p->suzu.release_s; out->cutoff_hz = p->suzu.cutoff_hz; out->resonance = p->suzu.resonance;
    out->shear = p->suzu.shear; out->shear_kind = p->suzu.shear_kind; out->voice_kind = p->suzu.voice_kind; out->modal_preset = p->suzu.modal_preset; out->modes = p->suzu.modes;
    out->coupling = p->suzu.coupling; out->decay_s = p->suzu.decay_s; out->decay_bright = p->suzu.decay_bright; out->stiffness = p->suzu.stiffness; out->pluck = p->suzu.pluck;
    out->bow_onset_s = p->suzu.bow_onset_s; out->bow_position = p->suzu.bow_position; out->breath_cc = p->suzu.breath_cc;
    if (p->suzu.oversample) out->oversample = p->suzu.oversample == 4u ? 4u : 2u;   /* step 78 */
    if (p->suzu.string_nodes) {   /* step 58: the strings and the chaos voices */
        out->string_nodes = p->suzu.string_nodes; out->string_decay_s = p->suzu.string_decay_s; out->pickup = p->suzu.pickup; out->bridge_hz = p->suzu.bridge_hz; out->bridge_cells = p->suzu.bridge_cells;
        out->bridge_coupling = p->suzu.bridge_coupling; out->bridge_decay_s = p->suzu.bridge_decay_s; out->loop_loss = p->suzu.loop_loss; out->duffing_beta = p->suzu.duffing_beta;
        out->drive = p->suzu.drive; out->drive_ratio = p->suzu.drive_ratio; out->rotor_k = p->suzu.rotor_k; out->mod_target = p->suzu.mod_target; out->mod_depth = p->suzu.mod_depth; out->mod_rate = p->suzu.mod_rate;
    }
    if (p->suzu.bore_nodes) {   /* step 58b: the flute */
        out->bore_nodes = p->suzu.bore_nodes; out->bore_loss = p->suzu.bore_loss; out->bore_corner_hz = p->suzu.bore_corner_hz; out->jet_gain = p->suzu.jet_gain; out->jet_drive = p->suzu.jet_drive;
        out->jet_tau = p->suzu.jet_tau; out->jet_q = p->suzu.jet_q; out->jet_noise = p->suzu.jet_noise; out->breath_ref = p->suzu.breath_ref; out->breath_range = p->suzu.breath_range; out->bore_wall_s = p->suzu.bore_wall_s;
        if (p->suzu.jet_area > 0.0f) { out->jet_area = p->suzu.jet_area; out->jet_offset = p->suzu.jet_offset; }   /* step 67's two */
        out->press_blows = p->suzu.press_blows;
    }
    if (p->suzu.reed_hz > 0.0f) {   /* step 58c: the reed and the lips */
        out->reed_hz = p->suzu.reed_hz; out->reed_q = p->suzu.reed_q; out->reed_open = p->suzu.reed_open; out->reed_close = p->suzu.reed_close; out->reed_area = p->suzu.reed_area; out->reed_noise = p->suzu.reed_noise; out->cone_apex = p->suzu.cone_apex;
        out->lip_ratio = p->suzu.lip_ratio; out->lip_q = p->suzu.lip_q; out->lip_open = p->suzu.lip_open; out->lip_close = p->suzu.lip_close; out->lip_area = p->suzu.lip_area; out->lip_range = p->suzu.lip_range;
        out->partial = p->suzu.partial; out->bell_start = p->suzu.bell_start; out->bell_gamma = p->suzu.bell_gamma; out->brass = p->suzu.brass;
    }
    if (p->suzu.drum_size) { out->drum_size = p->suzu.drum_size; out->drum_kappa = p->suzu.drum_kappa; out->drum_alpha = p->suzu.drum_alpha; out->drum_cap = p->suzu.drum_cap; }   /* step 79 */
    if (p->suzu.plate_size) { out->plate_on = p->suzu.plate_on; out->plate_size = p->suzu.plate_size; out->plate_hz = p->suzu.plate_hz; out->plate_kappa = p->suzu.plate_kappa; out->plate_decay_s = p->suzu.plate_decay_s; out->plate_bright = p->suzu.plate_bright; out->plate_gain = p->suzu.plate_gain; }
    if (p->suzu.bow_speed > 0.0f) { out->bow_mode = p->suzu.bow_mode; out->bow_force = p->suzu.bow_force; out->bow_speed = p->suzu.bow_speed; }   /* step 80 */
    if (p->suzu.hole_size > 0.0f) {   /* step 81 */
        out->reed_inertance = p->suzu.reed_inertance; out->bore_leak = p->suzu.bore_leak; out->brass_shock = p->suzu.brass_shock; out->lip_shear = p->suzu.lip_shear; out->lip_shear_ratio = p->suzu.lip_shear_ratio;
        out->tonehole_mode = p->suzu.tonehole_mode; out->hole_size = p->suzu.hole_size; out->hole_loss = p->suzu.hole_loss;
    }
    if (p->suzu.modal_ratio[0] > 0.0f) {   /* step 82: the table */
        for (int k = 0; k < SUMI_PRESET_MODAL_MAX && k < VOXO_MODAL_MAX; k++) { out->modal_ratio[k] = p->suzu.modal_ratio[k]; out->modal_t60[k] = p->suzu.modal_t60[k]; out->modal_weight[k] = p->suzu.modal_weight[k]; }
        out->modal_comp = p->suzu.modal_comp;
    }
}
