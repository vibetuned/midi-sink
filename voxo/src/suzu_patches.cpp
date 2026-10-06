// suzu_patches.cpp — THE PATCH TABLE (Phase 10 step 67's app fixes, DECISIONS_9 #10):
// the named Suzu patches every shell lists, one table for the desktop's picker,
// the tablets' Sound pages and the lab's deep links. The first five are the
// tablets' patches of step 63 (Voxo's defaults with the voice kind — and the
// modal preset — picked); the six that follow are the patches fitted to the
// Versilian Community Sample Library's instruments (CC0) at DECISIONS_9 #9 —
// presets/<name>_vcsl.json, whose `suzu` blocks these entries repeat field for
// field (tests/voxo_suzu_tests.cpp pins them to the files, so the two cannot
// drift). Every entry starts from voxo_suzu_default_params.
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
