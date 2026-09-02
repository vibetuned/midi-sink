/* Strict C11 consumer of hostmpe.h (PHASE4 working rule: the header must be
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
    printf("hostmpe_c_compile: OK\n");
    return 0;
}
