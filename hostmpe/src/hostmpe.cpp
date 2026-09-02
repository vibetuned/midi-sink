// hostmpe implementation (PHASE4_SPEC.md §3.2). Header contract: pure C ABI.
#include "hostmpe.h"

#include <math.h>

static const float KNEE = 0.03f;   // §3.2: radial deadband at 3% of R_max

extern "C" {

float hostmpe_soft_knee(float d) {
    if (!(d > KNEE)) return 0.0f;          // includes NaN -> 0
    if (d >= 1.0f) return 1.0f;
    return (d - KNEE) / (1.0f - KNEE);
}

void hostmpe_joystick_eff(float dx, float dy, float r_max,
                          float* out_x, float* out_y) {
    if (out_x) *out_x = 0.0f;
    if (out_y) *out_y = 0.0f;
    if (!(r_max > 0.0f)) return;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-9f) return;
    const float g = hostmpe_soft_knee(len / r_max);
    if (out_x) *out_x = dx / len * g;
    if (out_y) *out_y = dy / len * g;
}

} // extern "C"
