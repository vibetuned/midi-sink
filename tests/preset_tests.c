/* preset_tests.c — Phase 6 step 43 (QOL §3): the preset serializer's headless
 * suite, strict C11, the library alone (no core). */
#include "sumi_preset.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void fill(sumi_preset_t* p) {
    sumi_params_t d; memset(&d, 0, sizeof d);
    d.fluid_viscosity = 0.37f; d.expansion_rate = 1.25f; d.paper_roughness = 0.6f; d.smoothing_ms = 30.0f;
    d.active_palette_id = 3u; d.pitch_layout = 2u; d.sim_scale = 1.0f; d.bpm = 123.5f; d.roll_speed = 0.8f;
    d.bend_mode = 255u; d.press_mode = 2u; d.chladni_cell = 1.25f; d.burst_order = 3u; d.medium = 1u; d.anod_glow = 0.75f;
    for (int i = 0; i < 12; i++) d.burst_order_by_class[i] = (uint32_t)(2 + (i % 7));
    d.anod_pitch = 1.0f / 144.0f; d.chladni_mode = 1u;
    d.paper_tint[0] = 0.955f; d.paper_tint[1] = 0.95f; d.paper_tint[2] = 0.935f; d.fiber_scale = 1.4f; d.anod_dark = 0.7f; d.anod_grain = 0.2f;
    sumi_palette_t pal; memset(&pal, 0, sizeof pal);
    pal.stop_count = 3;
    pal.stops[0].rgb[0] = 0.98f; pal.stops[0].rgb[1] = 0.81f; pal.stops[0].rgb[2] = 0.02f; pal.stops[0].position = 0.0f;
    pal.stops[1].rgb[0] = 0.016f; pal.stops[1].rgb[1] = 0.28f; pal.stops[1].rgb[2] = 0.26f; pal.stops[1].position = 0.5f;
    pal.stops[2].rgb[0] = 0.054f; pal.stops[2].rgb[1] = 0.0f; pal.stops[2].rgb[2] = 0.089f; pal.stops[2].position = 1.0f;
    for (uint32_t i = 3; i < SUMI_PALETTE_MAX_STOPS; i++) pal.stops[i] = pal.stops[2];
    pal.depth_gamma = 1.5f; pal.depth_floor = 0.1f; pal.hue_drift = 0.2f;
    pal.accent_rgb[0] = 0.016f; pal.accent_rgb[1] = 0.28f; pal.accent_rgb[2] = 0.26f;
    pal.clear_rgb[0] = 0.85f; pal.clear_rgb[1] = 0.85f; pal.clear_rgb[2] = 0.8f;
    sumi_preset_init(p, &d, &pal);
    strcpy(p->name, "Indigo \"evening\" \\ tab\t");
    p->input_mode = 3u;
    p->cc_count = 3; p->cc[0].channel = 0xFF; p->cc[0].cc = 1; p->cc[0].target = 5u;
    p->cc[1].channel = 2; p->cc[1].cc = 74; p->cc[1].target = 8u; p->cc[2].channel = 0xFF; p->cc[2].cc = 106; p->cc[2].target = 16u;
    p->control_count = 2; p->controls[0].ctl = 9u; p->controls[0].value = 32; p->controls[1].ctl = 18u; p->controls[1].value = 127;
    p->strip_assign_a = 23; p->strip_assign_b = 24;
    p->layout_state.buttons = 5u; p->layout_state.slider = 0.75f;
}

static int same(const sumi_preset_t* a, const sumi_preset_t* b) {
    if (memcmp(&a->params, &b->params, sizeof a->params)) return 0;          /* floats and u32s only: no padding */
    if (memcmp(&a->palette, &b->palette, sizeof a->palette)) return 0;
    if (strcmp(a->name, b->name) || a->input_mode != b->input_mode) return 0;
    if (a->cc_count != b->cc_count || a->control_count != b->control_count) return 0;
    for (uint32_t i = 0; i < a->cc_count; i++) if (a->cc[i].channel != b->cc[i].channel || a->cc[i].cc != b->cc[i].cc || a->cc[i].target != b->cc[i].target) return 0;
    for (uint32_t i = 0; i < a->control_count; i++) if (a->controls[i].ctl != b->controls[i].ctl || a->controls[i].value != b->controls[i].value) return 0;
    if (a->strip_assign_a != b->strip_assign_a || a->strip_assign_b != b->strip_assign_b) return 0;
    if (a->layout_state.buttons != b->layout_state.buttons || a->layout_state.slider != b->layout_state.slider) return 0;
    return 1;
}

int main(void) {
    static char buf[16384];
    sumi_preset_t a, b, z;
    /* the round trip: every field survives, byte for byte */
    fill(&a);
    const size_t need = sumi_preset_write(&a, (1u << 16) | (1u << 8) | 0u, NULL, 0);
    CHECK(need > 500 && need + 1 < sizeof buf);
    const size_t got = sumi_preset_write(&a, (1u << 16) | (1u << 8) | 0u, buf, sizeof buf);
    CHECK(got == need && strlen(buf) == need);
    CHECK(strstr(buf, "\"midi_sink_preset\": 1") != NULL);
    CHECK(strstr(buf, "\"sumi_version\": [1, 1, 0]") != NULL);
    CHECK(strstr(buf, "\"burst_order_by_class\": [2, 3, 4, 5, 6, 7, 8, 2, 3, 4, 5, 6]") != NULL);
    CHECK(strstr(buf, "\\\"evening\\\"") != NULL && strstr(buf, "\\\\ tab\\u0009") != NULL);   /* the escapes: quotes, a backslash, a control as \u00XX */
    sumi_preset_init(&b, NULL, NULL);
    CHECK(sumi_preset_read(buf, 0, &b));
    CHECK(same(&a, &b));
    CHECK(b.schema == 1u && b.sumi_version == ((1u << 16) | (1u << 8)));
    /* the size contract: a buffer one short truncates, NUL-terminated, and the length is still the need */
    {
        char small[100];
        const size_t n2 = sumi_preset_write(&a, 0, small, sizeof small);
        CHECK(n2 == need && strlen(small) == sizeof small - 1);
    }
    /* the schema rule: unknown keys everywhere are ignored, missing keys keep the defaults */
    fill(&z);
    {
        const char* newer = "{ \"midi_sink_preset\": 7, \"sumi_version\": [9, 3, 1], \"future\": {\"deep\": [1, 2, {\"x\": null}]},"
                            "  \"params\": { \"bpm\": 99, \"unknown_knob\": 12.5, \"paper_tint\": [0.1, 0.2, 0.3, 0.4, 0.5], \"burst_order_by_class\": [7, 7] },"
                            "  \"palette\": { \"hue_drift\": 0.9, \"stops\": [[1, 0, 0, 0], [0, 0, 1, 1]], \"glow_mode\": true },"
                            "  \"strip\": {\"assign_a\": 55, \"assign_c\": 1}, \"name\": \"n\\u00e9w\", \"extra\": \"ignored\" }";
        sumi_preset_t c = z;
        CHECK(sumi_preset_read(newer, 0, &c));
        CHECK(c.schema == 7u && c.sumi_version == ((9u << 16) | (3u << 8) | 1u));
        CHECK(c.params.bpm == 99.0f);
        CHECK(c.params.fluid_viscosity == z.params.fluid_viscosity);                /* missing: kept */
        CHECK(c.params.paper_tint[0] == 0.1f && c.params.paper_tint[2] == 0.3f);   /* the extras dropped */
        CHECK(c.params.burst_order_by_class[0] == 7u && c.params.burst_order_by_class[1] == 7u && c.params.burst_order_by_class[2] == z.params.burst_order_by_class[2]);
        CHECK(c.palette.hue_drift == 0.9f && c.palette.stop_count == 2u && c.palette.stops[1].rgb[2] == 1.0f && c.palette.stops[7].rgb[2] == 1.0f);
        CHECK(c.palette.depth_gamma == z.palette.depth_gamma);                       /* kept */
        CHECK(c.strip_assign_a == 55 && c.strip_assign_b == z.strip_assign_b);
        CHECK(strcmp(c.name, "n?w") == 0);                                            /* \u beyond ASCII becomes ? */
        CHECK(c.cc_count == z.cc_count && c.input_mode == z.input_mode);
    }
    /* malformed input is refused and the target untouched */
    {
        sumi_preset_t c = z;
        CHECK(!sumi_preset_read("{ \"params\": [1, 2 }", 0, &c) && same(&c, &z));
        CHECK(!sumi_preset_read("[1, 2, 3]", 0, &c) && same(&c, &z));
        CHECK(!sumi_preset_read("{}", 0, &c) && same(&c, &z));
        CHECK(!sumi_preset_read("{ \"name\": \"unterminated", 0, &c) && same(&c, &z));
        CHECK(!sumi_preset_read("", 0, &c) && same(&c, &z));
        CHECK(!sumi_preset_read(buf, need / 2, &c) && same(&c, &z));               /* truncated */
    }
    /* whitespace, exponents, negatives, an empty map */
    {
        sumi_preset_t c = z;
        CHECK(sumi_preset_read("\n{\r\n\t\"params\":{\"bpm\":1.2e2,\"sim_scale\":-1},\"cc_map\":[],\"controls\":[[9,300]]}\n", 0, &c));
        CHECK(c.params.bpm == 120.0f && c.params.sim_scale == -1.0f && c.cc_count == 0u && c.control_count == 1u && c.controls[0].value == 127);
    }
    CHECK(sumi_preset_schema() == 1u);
    printf("preset_tests: %d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
