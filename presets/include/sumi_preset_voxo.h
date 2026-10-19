/* sumi_preset_voxo.h — step 82 (TO_PROJECT_SPEC §13.12): a preset file's Suzu block as Voxo's params.
 *
 * The preset library never links Voxo; this one function only fills Voxo's struct from the file's (both headers are
 * types alone), so a shell that reads a Suzu patch file — the tablets' Sound pages, the bench's --preset — applies it
 * with voxo_set_suzu_params and nothing else. The rule is the schema's: a field group a file from before its step holds
 * as zeros leaves Voxo's defaults standing (the sentinels: string_nodes, bore_nodes, reed_hz, drum_size, plate_size,
 * bow_speed, hole_size, modal_ratio[0]). */
#ifndef SUMI_PRESET_VOXO_H
#define SUMI_PRESET_VOXO_H

#include "sumi_preset.h"
#include "voxo.h"

#ifdef __cplusplus
extern "C" {
#endif

void sumi_preset_suzu_to_voxo(const sumi_preset_t* p, voxo_suzu_params_t* out);

#ifdef __cplusplus
}
#endif
#endif
