// the fuzz's finding, reproduced directly: the flute at a high note bent far up in one jump, under breath and press
#include "voxo.h"
#include <cmath>
#include <cstdio>
#include <vector>
static bool finite(const std::vector<float>& b) { for (float x : b) if (!std::isfinite(x)) return false; return true; }
int main() {
    struct Case { uint32_t patch; int note; int bend; int breath; const char* name; } cases[] = {
        { 10, 82, 8192 + 8000, 126, "flute 10: note 82 bent +46.9 semitones in one jump (17.6 kHz)" },
        { 10, 82, 8192 + 6000, 126, "flute 10: note 82 bent +35 semitones (8.9 kHz)" },
        { 10, 82, 8192 + 4000, 126, "flute 10: note 82 bent +23 semitones (4.5 kHz)" },
        { 2, 82, 8192 + 8000, 126, "flute 2: note 82 bent +46.9 semitones" },
        { 3, 82, 8192 + 8000, 126, "sax 3: note 82 bent +46.9 semitones" },
        { 4, 82, 8192 + 8000, 126, "trumpet 4: note 82 bent +46.9 semitones" },
        { 10, 36, 8192 - 8000, 126, "flute 10: note 36 bent -46.9 semitones (4.3 Hz)" },
        { 4, 36, 8192 - 8000, 126, "trumpet 4: note 36 bent -46.9 semitones" },
    };
    for (const Case& c : cases) {
        voxo_config_t cfg{}; cfg.sample_rate = 48000; cfg.block_frames = 128; cfg.max_voices = 16;
        voxo_t* v = voxo_create(&cfg); voxo_set_input_mode(v, 1); voxo_set_source(v, VOXO_SOURCE_SUZU);
        voxo_suzu_params_t sp; voxo_suzu_patch(c.patch, &sp); voxo_set_suzu_params(v, &sp);
        voxo_push_midi(v, 0xB0, 101, 0); voxo_push_midi(v, 0xB0, 100, 6); voxo_push_midi(v, 0xB0, 6, 15);
        voxo_push_midi(v, 0xB1, 2, (uint8_t)c.breath); voxo_push_midi(v, 0x91, (uint8_t)c.note, 100); voxo_push_midi(v, 0xD1, 120, 0);
        std::vector<float> buf(256); int first_nan = -1; float peak = 0.0f;
        for (int b = 0; b < 800; b++) {
            if (b == 100) voxo_push_midi(v, 0xE1, (uint8_t)(c.bend & 0x7F), (uint8_t)(c.bend >> 7));
            voxo_render(v, buf.data(), 128);
            if (first_nan < 0 && !finite(buf)) first_nan = b;
            for (float x : buf) if (std::isfinite(x)) peak = std::fmax(peak, std::fabs(x));
        }
        std::printf("%-62s %s%s peak %.3f\n", c.name, first_nan < 0 ? "finite" : "NON-FINITE at block ", first_nan < 0 ? "" : std::to_string(first_nan).c_str(), peak);
        voxo_destroy(v);
    }
    return 0;
}
