// §4.2 ink-phase assignment, shared by every drop producer (gesture ABI and
// voice mapper). Parity-derived, RGBA16F-safe — see DECISIONS.md #18.
#pragma once

#include <stdint.h>

// Returns the band base (1 or 2) for the next ink drop and advances the
// global monotonic drop counter.
static inline float sumi_next_ink_phase_base(uint32_t* drop_counter) {
    const float base = 1.0f + (float)(*drop_counter % 2u);
    (*drop_counter)++;
    return base;
}
