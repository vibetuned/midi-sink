// suzu_patches.cpp — THE PATCH TABLE (Phase 10 step 67's app fixes, DECISIONS_9 #10):
// the named Suzu patches every shell lists, one table for the desktop's picker,
// the tablets' Sound pages and the lab's deep links. The first five are the
// tablets' patches of step 63 (Voxo's defaults with the voice kind — and the
// modal preset — picked); the six that follow are the patches fitted to the
// Versilian Community Sample Library's instruments (CC0) at DECISIONS_9 #9 —
// presets/<name>_vcsl.json, whose `suzu` blocks these entries repeat field for
// field (tests/voxo_suzu_tests.cpp pins them to the files, so the two cannot
// drift). Every entry starts from voxo_suzu_default_params.
// Step 79 (TO_PROJECT_SPEC §13.9): the membrane's two — the drum and the gong — and the harp on the bus plate (the
// soundboard a tablet hears under a chord); presets/drum.json, gong.json, harp_soundboard.json, pinned the same way.
#include "voxo.h"

namespace {

struct Patch { const char* name; void (*fill)(voxo_suzu_params_t&); };

// The tablets' five (step 63, DECISIONS_8 #13).
void bowed_string(voxo_suzu_params_t& s)  { s.voice_kind = 1; s.modal_preset = 0; }   // the bowed harmonic string: Voxo's default voice
void bell(voxo_suzu_params_t& s)          { s.voice_kind = 1; s.modal_preset = 2; }
void flute(voxo_suzu_params_t& s)         { s.voice_kind = 6; }
void saxophone(voxo_suzu_params_t& s)     { s.voice_kind = 7; }
void trumpet(voxo_suzu_params_t& s)       { s.voice_kind = 8; }

// The VCSL fits (DECISIONS_9 #9) — presets/*_vcsl.json, field for field.
void dan_tranh(voxo_suzu_params_t& s) {
    s.voice_kind = 1; s.modal_preset = 0; s.modes = 13; s.coupling = 0.05f; s.decay_s = 3.435f; s.decay_bright = 0.0836f;
    s.stiffness = 0.000132f; s.pluck = 0.3f; s.bow_onset_s = 0.0f; s.bow_position = 0.3f; s.breath_cc = 2;
    s.level = 0.28f; s.attack_s = 0.0124f; s.release_s = 1.031f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.shear = 0.0f; s.shear_kind = 0;
}
void glockenspiel(voxo_suzu_params_t& s) {
    s.voice_kind = 1; s.modal_preset = 1; s.modes = 3; s.coupling = 0.05f; s.decay_s = 4.161f; s.decay_bright = 0.0f;
    s.stiffness = 1.8e-05f; s.pluck = 0.11f; s.bow_onset_s = 0.0f; s.bow_position = 0.3f; s.breath_cc = 2;
    s.level = 0.28f; s.attack_s = 0.0196f; s.release_s = 1.248f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.shear = 0.0f; s.shear_kind = 0;
}
void tubular_bells(voxo_suzu_params_t& s) {
    s.voice_kind = 1; s.modal_preset = 2; s.modes = 9; s.coupling = 0.05f; s.decay_s = 17.359f; s.decay_bright = 0.0406f;
    s.stiffness = 5.7e-05f; s.pluck = 0.11f; s.bow_onset_s = 0.0f; s.bow_position = 0.3f; s.breath_cc = 2;
    s.level = 0.28f; s.attack_s = 0.0135f; s.release_s = 1.5f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.shear = 0.0f; s.shear_kind = 0;
}
void concert_harp(voxo_suzu_params_t& s) {
    s.voice_kind = 1; s.modal_preset = 4; s.modes = 5; s.coupling = 0.05f; s.decay_s = 5.728f; s.decay_bright = 0.0088f;
    s.stiffness = 0.000206f; s.pluck = 0.445f; s.bow_onset_s = 0.0f; s.bow_position = 0.3f; s.breath_cc = 2;
    s.level = 0.28f; s.attack_s = 0.0182f; s.release_s = 1.5f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.shear = 0.0f; s.shear_kind = 0;
}
void tenor_sax(voxo_suzu_params_t& s) {
    s.voice_kind = 7; s.level = 0.25f; s.attack_s = 0.174f; s.release_s = 0.25f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.press_blows = 1;
    s.reed_hz = 11500.0f; s.reed_q = 0.72f; s.reed_open = 0.5f; s.reed_close = 3.0f; s.reed_area = 0.14f; s.reed_noise = 0.02f; s.cone_apex = 0.25f;
}
void baroque_recorder(voxo_suzu_params_t& s) {
    s.voice_kind = 6; s.level = 0.25f; s.attack_s = 0.1f; s.release_s = 0.25f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.press_blows = 1;
    s.bore_nodes = 128; s.bore_loss = 0.28f; s.bore_corner_hz = 1600.0f; s.jet_gain = 560.0f; s.jet_drive = 1.0f; s.jet_tau = 0.5f;
    s.jet_q = 1.0f; s.jet_noise = 0.025f; s.jet_area = 0.05f; s.jet_offset = 0.16f; s.breath_ref = 0.44f; s.breath_range = 12.0f; s.bore_wall_s = 2.0f;
}

// Step 79: the membranes and the soundboard — presets/drum.json, gong.json, harp_soundboard.json, field for field.
void drum(voxo_suzu_params_t& s) {
    s.voice_kind = 9; s.drum_size = 16; s.drum_kappa = 0.02f; s.drum_alpha = 0.0f; s.drum_cap = 1.0f; s.decay_s = 1.2f; s.decay_bright = 0.15f;
    s.pluck = 0.3f; s.pickup = 0.32f; s.level = 0.3f; s.attack_s = 0.002f; s.release_s = 0.6f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f;
}
void gong(voxo_suzu_params_t& s) {
    s.voice_kind = 9; s.drum_size = 16; s.drum_kappa = 0.08f; s.drum_alpha = 0.8f; s.drum_cap = 1.5f; s.decay_s = 6.0f; s.decay_bright = 0.05f;
    s.pluck = 0.42f; s.pickup = 0.3f; s.level = 0.3f; s.attack_s = 0.004f; s.release_s = 2.0f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f;
}
void harp_soundboard(voxo_suzu_params_t& s) {
    concert_harp(s);
    s.plate_on = 1; s.plate_size = 64; s.plate_hz = 110.0f; s.plate_kappa = 0.003f; s.plate_decay_s = 2.0f; s.plate_bright = 0.02f; s.plate_gain = 1.0f;
}

// Step 80 (TO_PROJECT_SPEC §13.10): the stick-slip bow — the hybrid string (the waveguide) and the Verlet chain under it;
// presets/bowed_stickslip.json, bowed_chain.json, field for field.
void bowed_stickslip(voxo_suzu_params_t& s) {   // the hybrid's delay line under the bow: dispersionless, the Helmholtz corner sharp
    s.voice_kind = 3; s.string_decay_s = 4.0f; s.pickup = 0.25f; s.pluck = 0.28f; s.bridge_hz = 220.0f; s.bridge_cells = 2; s.bridge_coupling = 0.002f;
    s.bridge_decay_s = 1.5f; s.loop_loss = 0.5f; s.bow_position = 0.12f; s.bow_mode = 1; s.bow_force = 0.5f; s.bow_speed = 1.0f;
    s.level = 0.25f; s.attack_s = 0.003f; s.release_s = 0.4f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.press_blows = 1;
}
void bowed_chain(voxo_suzu_params_t& s) {
    s.voice_kind = 2; s.string_nodes = 48; s.string_decay_s = 4.0f; s.pickup = 0.3f; s.pluck = 0.28f;
    s.bow_position = 0.12f; s.bow_mode = 1; s.bow_force = 0.5f; s.bow_speed = 1.0f;
    s.level = 0.25f; s.attack_s = 0.003f; s.release_s = 0.4f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f; s.press_blows = 1;
}

// Step 82 (TO_PROJECT_SPEC §13.12): the unison pair — the table's ratios in pairs, the compensation off: the chain's spring
// splits each pair into its two normal modes — for the first pair at +κ(3 − √5)/4 and +κ(3 + √5)/4 of the frequency (one
// mode has one neighbour, the other two) — and the pairs beat: 11.6 cents apart at κ 0.006, 1.5 Hz at A3 (measured at
// κ 0.01: +3.5 / +22 cents against the theory's +3.3 / +22.7). presets/unison.json, field for field. The raw lattice takes
// no swirl share (the load gate reads the patch's κ alone).
void unison_pair(voxo_suzu_params_t& s) {
    s.voice_kind = 1; s.modal_preset = 5; s.modes = 8; s.coupling = 0.006f; s.modal_comp = 0; s.decay_s = 4.0f; s.decay_bright = 0.2f;
    static const float r[8] = { 1.0f, 1.0f, 2.0f, 2.0f, 3.0f, 3.0f, 4.0f, 4.0f };
    for (int k = 0; k < 8; k++) { s.modal_ratio[k] = r[k]; s.modal_weight[k] = 1.0f / r[k]; }
    s.pluck = 0.28f; s.bow_position = 0.3f; s.level = 0.25f; s.attack_s = 0.003f; s.release_s = 0.4f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f;
}

// Step 83 (TO_PROJECT_SPEC §13.13): the Toda ring — sixteen nodes, the nonlinearity on the press and the slide (presets/toda.json).
void toda_ring(voxo_suzu_params_t& s) {
    s.voice_kind = 10; s.toda_nodes = 16; s.toda_b = 0.6f; s.toda_cap = 6.0f; s.toda_width = 1.5f; s.decay_s = 3.0f; s.pickup = 0.3f;
    s.level = 0.25f; s.attack_s = 0.002f; s.release_s = 0.5f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f;
}
// Step 84: the Boris gyroscope — a slow rosette at rest, the poly pressure spreading the doublet to 24 Hz
void gyroscope(voxo_suzu_params_t& s) {
    s.voice_kind = 11; s.boris_b = 24.0f; s.boris_b_rest = 2.0f; s.boris_prewarp = 1; s.decay_s = 4.0f; s.pluck = 0.5f;
    s.level = 0.3f; s.attack_s = 0.002f; s.release_s = 0.5f; s.cutoff_hz = 20000.0f; s.resonance = 0.0f;
}

const Patch TABLE[] = {
    { "Bowed string",             bowed_string },
    { "Bell",                     bell },
    { "Flute",                    flute },
    { "Saxophone",                saxophone },
    { "Trumpet",                  trumpet },
    { "Dan Tranh (VCSL)",         dan_tranh },
    { "Glockenspiel (VCSL)",      glockenspiel },
    { "Tubular bells (VCSL)",     tubular_bells },
    { "Concert harp (VCSL)",      concert_harp },
    { "Tenor sax (VCSL)",         tenor_sax },
    { "Baroque recorder (VCSL)",  baroque_recorder },
    { "Drum",                     drum },               // step 79: the membrane
    { "Gong",                     gong },               //   the membrane with the von Kármán swell
    { "Harp on a soundboard",     harp_soundboard },    //   the concert harp over the bus plate
    { "Bowed string (stick-slip)", bowed_stickslip },   // step 80: the hybrid string under the Helmholtz bow
    { "Bowed chain",              bowed_chain },        //   the Verlet chain under it
    { "Unison pair",              unison_pair },        // step 82: the table in pairs, the coupling's splitting as the sound
    { "Toda ring",                toda_ring },          // step 83: the soliton ring — the press sharpens the pulse
    { "Gyroscope (Zeeman stereo)", gyroscope },         // step 84: the Boris gyroscope — the poly pressure splits the note into a stereo pair
};
constexpr uint32_t COUNT = (uint32_t)(sizeof TABLE / sizeof TABLE[0]);

} // namespace

extern "C" {

uint32_t voxo_suzu_patch_count(void) { return COUNT; }
const char* voxo_suzu_patch_name(uint32_t index) { return index < COUNT ? TABLE[index].name : nullptr; }
bool voxo_suzu_patch(uint32_t index, voxo_suzu_params_t* out) {
    if (!out || index >= COUNT) return false;
    voxo_suzu_default_params(out);
    TABLE[index].fill(*out);
    return true;
}

}
