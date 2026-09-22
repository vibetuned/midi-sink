/* sumi_preset_apply.c — the part of the preset the core takes directly. Its
 * own translation unit so a consumer that only serializes never links the
 * core (sumi_preset.h). */
#include "sumi_preset.h"

void sumi_preset_apply(sumi_instance_t* inst, const sumi_preset_t* p) {
    if (!inst || !p) return;
    sumi_set_params(inst, &p->params);                                /* the core clamps */
    sumi_set_input_mode(inst, (sumi_input_mode_t)(p->input_mode >= 1u && p->input_mode <= 3u ? p->input_mode : 1u));
    sumi_set_palette(inst, &p->palette);
    sumi_clear_cc_map(inst);
    const uint32_t n = p->cc_count > SUMI_PRESET_MAX_CC ? SUMI_PRESET_MAX_CC : p->cc_count;
    for (uint32_t i = 0; i < n; i++) sumi_map_cc(inst, p->cc[i].channel, p->cc[i].cc, (sumi_ctl_t)p->cc[i].target);
}
