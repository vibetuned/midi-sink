// wind_cost.cpp — the winds' cost: 13 voices of a kind held under breath 100 for 4 s at 48 kHz, 128-frame blocks; the mean and
// the worst block time in µs; the trumpet with the shock (1) and without (0); the limiter on / off.
#include "voxo.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <initializer_list>
int main() {
    struct Cfg { int kind; float shock; uint32_t lim; const char* name; };
    for (Cfg c : { Cfg{ 8, 1.0f, 1u, "trumpet, shock 1, limiter" }, Cfg{ 8, 1.0f, 0u, "trumpet, shock 1, no limiter" }, Cfg{ 8, 0.0f, 1u, "trumpet, no shock (v1)" }, Cfg{ 7, 1.0f, 1u, "sax, shock 1" }, Cfg{ 7, 0.0f, 1u, "sax, no shock" }, Cfg{ 6, 1.0f, 1u, "flute" } }) {
        voxo_config_t cf{}; cf.sample_rate = 48000; cf.block_frames = 128; cf.max_voices = 16; voxo_t* v = voxo_create(&cf); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)c.kind; sp.brass_shock = c.shock; sp.shock_limiter = c.lim; voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        for (int i = 0; i < 13; i++) { voxo_push_midi(v, (uint8_t)(0xB1 + i), 2, 100); voxo_push_midi(v, (uint8_t)(0x91 + i), (uint8_t)(48 + i * 2), 100); }
        std::vector<float> out(256); double worst = 0, sum = 0; int n = 0;
        for (uint32_t f = 0; f < 48000 * 4; f += 128) { const auto t0 = std::chrono::steady_clock::now(); voxo_render(v, out.data(), 128); const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); if (f > 48000) { sum += us; n++; if (us > worst) worst = us; } }
        std::printf("%-30s 13 voices: mean %6.0f µs, worst %6.0f µs of the 2667 µs block\n", c.name, sum / n, worst); voxo_destroy(v);
    }
    return 0;
}
