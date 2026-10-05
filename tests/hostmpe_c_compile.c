/* Strict C11 consumer of hostmpe.h (Phase-4 working rule: the header must be
   pure C, exactly like sumi_core.h — the Swift module-map pattern can only
   import C headers). The abi_c_compile sibling for the host-side library. */
#include "hostmpe.h"

#include <stdio.h>
#include <math.h>

int main(void) {
    if (hostmpe_soft_knee(0.03f) != 0.0f || hostmpe_soft_knee(1.0f) != 1.0f) {
        fprintf(stderr, "FAIL: soft knee endpoints\n");
        return 1;
    }
    float x = 0.0f, y = 0.0f;
    hostmpe_joystick_eff(0.06f, 0.08f, 0.1f, &x, &y);
    if (fabsf(x - 0.6f) > 1e-5f || fabsf(y - 0.8f) > 1e-5f) {
        fprintf(stderr, "FAIL: joystick_eff\n");
        return 1;
    }
    /* Phase 9 step 62: the fingering widgets, the retune, the theremin, the UX items — pure C too. */
    {
        typedef void (*fn_ptr)(void);
        const fn_ptr step62[] = {
            (fn_ptr)hostmpe_strip_valve_press, (fn_ptr)hostmpe_strip_valve_release, (fn_ptr)hostmpe_strip_valves,
            (fn_ptr)hostmpe_strip_slide_set, (fn_ptr)hostmpe_strip_slide_value, (fn_ptr)hostmpe_strip_reset,
            (fn_ptr)hostmpe_strip_quick_set, (fn_ptr)hostmpe_strip_quick_count, (fn_ptr)hostmpe_strip_quick_next,
            (fn_ptr)hostmpe_voice_retune, (fn_ptr)hostmpe_tick, (fn_ptr)hostmpe_voice_pitch_offset,
            (fn_ptr)hostmpe_touch_begin_offset, (fn_ptr)hostmpe_theremin_begin, (fn_ptr)hostmpe_theremin_move,
            (fn_ptr)hostmpe_set_mirror, (fn_ptr)hostmpe_mirror, (fn_ptr)hostmpe_device_profile,
        };
        hostmpe_device_profile_t prof = hostmpe_device_profile("Osmose");
        if (sizeof step62 / sizeof step62[0] != 18u || prof.device != HOSTMPE_DEVICE_OSMOSE || prof.input_mode != 1u ||
            HOSTMPE_SLIDE_POSITIONS != 7 || HOSTMPE_QUICK_MAX != 13) {
            fprintf(stderr, "FAIL: step 62 symbols\n");
            return 1;
        }
    }
    printf("hostmpe_c_compile: OK\n");
    return 0;
}
