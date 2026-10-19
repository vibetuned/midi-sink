#include "voxo.h"
#include <cstdio>
#include <initializer_list>
static void logf_(int, const char* m, void*) { std::printf("    [log] %s\n", m); }
int main() {
    for (float shock : { 1.0f, 1.5f, 2.0f, 4.0f }) for (uint32_t lim : { 1u, 0u }) {
        voxo_config_t c{}; c.sample_rate = 48000; c.block_frames = 128; c.max_voices = 4; c.log_cb = logf_; voxo_t* v = voxo_create(&c); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = 8; sp.brass_shock = shock; sp.shock_limiter = lim;
        std::printf("shock %.1f limiter %s: set_suzu_params -> %s\n", shock, lim ? "on " : "OFF", voxo_set_suzu_params(v, &sp) ? "accepted" : "REFUSED");
        voxo_destroy(v);
    }
    return 0;
}
