// strike_cost.cpp — the cost of a block that carries a burst of strikes: 15 note-ons (one per MPE channel) pushed before one
// 128-frame block, the block timed; then a block with no strikes (the steady render) — per kind.
#include "voxo.h"
#include <chrono>
#include <cstdio>
#include <vector>
#include <initializer_list>
int main() {
    for (int kind : { 6, 7, 8 }) {
        double worst = 0, steady = 0;
        for (int rep = 0; rep < 5; rep++) {
            voxo_config_t cf{}; cf.sample_rate = 48000; cf.block_frames = 128; cf.max_voices = 16; voxo_t* v = voxo_create(&cf); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
            voxo_suzu_params_t sp; voxo_suzu_default_params(&sp); sp.voice_kind = (uint32_t)kind; voxo_set_suzu_params(v, &sp);
            voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
            std::vector<float> out(256);
            for (int w = 0; w < 20; w++) voxo_render(v, out.data(), 128);   // warm
            for (int i = 0; i < 15; i++) { voxo_push_midi(v, (uint8_t)(0xB1 + i), 2, 100); voxo_push_midi(v, (uint8_t)(0x91 + i), (uint8_t)(48 + i * 2), 100); }
            auto t0 = std::chrono::steady_clock::now(); voxo_render(v, out.data(), 128); const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count(); if (us > worst) worst = us;
            for (int w = 0; w < 40; w++) voxo_render(v, out.data(), 128);
            t0 = std::chrono::steady_clock::now(); voxo_render(v, out.data(), 128); steady = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
            voxo_destroy(v);
        }
        std::printf("kind %d: the block with 15 strikes %6.0f µs (worst of 5); a steady block of 15 voices %6.0f µs\n", kind, worst, steady);
    }
    return 0;
}
