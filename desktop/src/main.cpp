// Desktop harness (PROJECT_SPEC.md §1): GLFW_NO_API window, CAMetalLayer via
// metal_layer_glue, frame loop driving sumi_update/sumi_render, sumi_resize on
// framebuffer-size changes. MIDI (midi_harness.cpp) arrives in a later step.
//
// Mouse gestures (roadmap step 3):
//   left click  -> sumi_add_drop   (ink; ring parity alternates via the core's
//                                   drop counter)
//   left drag   -> sumi_add_tine   (one segment per cursor move)
//   right drag  -> sumi_add_vortex (at the cursor, strength ~ drag speed)
//
// Harness-only test flags (automated DONE evidence, see DECISIONS.md):
//   --exit-after <seconds>   close the window cleanly after N seconds
//   --resize-test            programmatically resize the window twice
//   --sim-scale <f>          set params.sim_scale after create
//   --drop-test <n>          one centered drop per frame until n drops
//   --demo-chevron           concentric drops, then a vertical tine drag
//   --demo-vortex            concentric drops, then a building vortex
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "sumi_core.h"
#include "metal_layer_glue.h"
#include "midi_harness.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


// Gesture tuning (harness-side defaults; normalized canvas-height units).
static const float DROP_RADIUS       = 0.06f;
static const float TINE_ALPHA        = 0.035f;
static const float TINE_MAG_SCALE    = 1.0f;    // magnitude = segment length
static const float VORTEX_RADIUS     = 0.18f;
static const float VORTEX_STRENGTH   = 4.0f;    // radians per unit drag speed
static const double DRAG_THRESHOLD_PX = 5.0;

struct AppState {
    sumi_instance_t* inst = nullptr;
    char print_path[1024] = "print.png";
    // left button
    bool   left_down = false;
    bool   left_dragged = false;
    double lx = 0.0, ly = 0.0;      // last emitted position (window coords)
    // right button
    bool   right_down = false;
    double rx = 0.0, ry = 0.0;
};

static void log_cb(int level, const char* msg, void* /*user*/) {
    static const char* names[] = {"PANIC", "ERROR", "WARN", "INFO"};
    const char* name = (level >= 0 && level <= 3) ? names[level] : "?";
    std::fprintf(stderr, "[sumi %s] %s\n", name, msg);
}

static void glfw_error_cb(int code, const char* desc) {
    std::fprintf(stderr, "[glfw %d] %s\n", code, desc);
}

static void framebuffer_size_cb(GLFWwindow* window, int w, int h) {
    AppState* app = (AppState*)glfwGetWindowUserPointer(window);
    if (!app || !app->inst || w <= 0 || h <= 0) return;
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    (void)yscale;
    std::printf("resize -> %dx%d px (scale %.1f)\n", w, h, xscale);
    sumi_resize(app->inst, (uint32_t)w, (uint32_t)h, xscale);
}

// Cursor position -> normalized [0,1] canvas coords (v grows downward, which
// matches the field's texture space).
static void norm_pos(GLFWwindow* window, double px, double py, float* nx, float* ny) {
    int w = 1, h = 1;
    glfwGetWindowSize(window, &w, &h);
    *nx = (float)(px / (double)(w > 0 ? w : 1));
    *ny = (float)(py / (double)(h > 0 ? h : 1));
}

// Segment length in aspect-corrected space (canvas-height units).
static float segment_len_ac(GLFWwindow* window, double x0, double y0, double x1, double y1) {
    int w = 1, h = 1;
    glfwGetWindowSize(window, &w, &h);
    const float dx = (float)((x1 - x0) / (double)(h > 0 ? h : 1));   // /h: ac units
    const float dy = (float)((y1 - y0) / (double)(h > 0 ? h : 1));
    return __builtin_sqrtf(dx * dx + dy * dy);
}

// Save the last paper-dip print via sumi_read_print (SPEC 5.3) as PNG.
static bool save_print(sumi_instance_t* inst, const char* path) {
    uint32_t w = 0, h = 0;
    if (!sumi_read_print(inst, nullptr, 0, &w, &h)) {
        std::fprintf(stderr, "[print] no print ready (dip first: key 9 or CC64)\n");
        return false;
    }
    const size_t bytes = (size_t)w * h * 4;
    uint8_t* pixels = (uint8_t*)std::malloc(bytes);
    if (!pixels || !sumi_read_print(inst, pixels, bytes, &w, &h)) {
        std::free(pixels);
        return false;
    }
    // Encode on a background thread: a 2560x1440 PNG takes ~1 s and must not
    // stall the render loop (step-7 DONE: no hitch at dip/export time).
    std::string path_copy(path);
    std::thread([pixels, w, h, path_copy]() {
        const int ok = stbi_write_png(path_copy.c_str(), (int)w, (int)h, 4, pixels, (int)w * 4);
        std::printf("[print] %s %ux%u -> %s\n", ok ? "saved" : "FAILED to save",
                    w, h, path_copy.c_str());
        std::fflush(stdout);
        std::free(pixels);
    }).detach();
    return true;
}

static void print_params(const sumi_params_t* p) {
    std::printf("[params] viscosity %.2f  expansion %.2f  roughness %.2f  palette %u  layout %u\n",
                (double)p->fluid_viscosity, (double)p->expansion_rate,
                (double)p->paper_roughness, p->active_palette_id, p->pitch_layout);
}

// Live param tuning (roadmap step 7): 1/2 viscosity, 3/4 expansion,
// 5/6 roughness, 7 palette, 8 layout, 9 paper dip, S save print.
static void key_cb(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    if (action != GLFW_PRESS) return;
    AppState* app = (AppState*)glfwGetWindowUserPointer(window);
    if (!app || !app->inst) return;
    sumi_params_t p;
    sumi_get_params(app->inst, &p);
    bool changed = true;
    switch (key) {
        case GLFW_KEY_1: p.fluid_viscosity -= 0.1f; if (p.fluid_viscosity < 0) p.fluid_viscosity = 0; break;
        case GLFW_KEY_2: p.fluid_viscosity += 0.1f; if (p.fluid_viscosity > 1) p.fluid_viscosity = 1; break;
        case GLFW_KEY_3: p.expansion_rate *= 0.8f; break;
        case GLFW_KEY_4: p.expansion_rate *= 1.25f; break;
        case GLFW_KEY_5: p.paper_roughness -= 0.1f; if (p.paper_roughness < 0) p.paper_roughness = 0; break;
        case GLFW_KEY_6: p.paper_roughness += 0.1f; if (p.paper_roughness > 1) p.paper_roughness = 1; break;
        case GLFW_KEY_7: p.active_palette_id = (p.active_palette_id + 1) % 3; break;
        case GLFW_KEY_8: p.pitch_layout = (p.pitch_layout + 1) % 3; break;
        case GLFW_KEY_9: sumi_trigger_paper_dip(app->inst); changed = false; break;
        case GLFW_KEY_L: p.pitch_layout = (p.pitch_layout + 1) % 5; break;   // all layouts incl. rolls
        case GLFW_KEY_B:   // BPM nudge for metronome eyeballing (Shift = down)
            p.bpm += (mods & GLFW_MOD_SHIFT) ? -5.0f : 5.0f;
            if (p.bpm < 20.0f) p.bpm = 20.0f;
            if (p.bpm > 300.0f) p.bpm = 300.0f;
            std::printf("[params] bpm %.0f\n", (double)p.bpm);
            break;
        case GLFW_KEY_S: save_print(app->inst, app->print_path); changed = false; break;
        default: changed = false; return;
    }
    if (changed) {
        sumi_set_params(app->inst, &p);
        print_params(&p);
    }
}

static void mouse_button_cb(GLFWwindow* window, int button, int action, int /*mods*/) {
    AppState* app = (AppState*)glfwGetWindowUserPointer(window);
    if (!app || !app->inst) return;
    double cx = 0.0, cy = 0.0;
    glfwGetCursorPos(window, &cx, &cy);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            app->left_down = true;
            app->left_dragged = false;
            app->lx = cx; app->ly = cy;
        } else if (action == GLFW_RELEASE && app->left_down) {
            app->left_down = false;
            if (!app->left_dragged) {
                float nx, ny;
                norm_pos(window, cx, cy, &nx, &ny);
                sumi_add_drop(app->inst, nx, ny, DROP_RADIUS, 0);
            }
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            app->right_down = true;
            app->rx = cx; app->ry = cy;
        } else if (action == GLFW_RELEASE) {
            app->right_down = false;
        }
    }
}

static void cursor_pos_cb(GLFWwindow* window, double cx, double cy) {
    AppState* app = (AppState*)glfwGetWindowUserPointer(window);
    if (!app || !app->inst) return;

    if (app->left_down) {
        const double dx = cx - app->lx, dy = cy - app->ly;
        if (dx * dx + dy * dy >= DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX) {
            float x0, y0, x1, y1;
            norm_pos(window, app->lx, app->ly, &x0, &y0);
            norm_pos(window, cx, cy, &x1, &y1);
            const float mag = segment_len_ac(window, app->lx, app->ly, cx, cy) * TINE_MAG_SCALE;
            sumi_add_tine(app->inst, x0, y0, x1, y1, TINE_ALPHA, mag);
            app->left_dragged = true;
            app->lx = cx; app->ly = cy;
        }
    }
    if (app->right_down) {
        const double dx = cx - app->rx, dy = cy - app->ry;
        if (dx * dx + dy * dy >= DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX) {
            float nx, ny;
            norm_pos(window, cx, cy, &nx, &ny);
            const float speed = segment_len_ac(window, app->rx, app->ry, cx, cy);
            sumi_add_vortex(app->inst, nx, ny, speed * VORTEX_STRENGTH, VORTEX_RADIUS);
            app->rx = cx; app->ry = cy;
        }
    }
}

int main(int argc, char** argv) {
    double exit_after = 0.0;   // 0 = run until the window is closed
    bool resize_test = false;
    float sim_scale = 0.0f;    // 0 = leave the core default
    long drop_test = 0;
    bool demo_chevron = false;
    bool demo_vortex = false;
    int map_cc = -1, map_target = -1;
    int layout_arg = -1;             // set params.pitch_layout after create
    double dip_at = 0.0;             // trigger a paper dip at t seconds
    const char* print_out = nullptr; // auto-save the print once ready
    bool cycle_visuals = false;      // palette/layout live-switch test
    double dip_burst = 0.0;          // t: dips at t, t+0.2, t+0.25; reads at t+1
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--exit-after") == 0 && i + 1 < argc) {
            exit_after = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--resize-test") == 0) {
            resize_test = true;
        } else if (std::strcmp(argv[i], "--sim-scale") == 0 && i + 1 < argc) {
            sim_scale = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--drop-test") == 0 && i + 1 < argc) {
            drop_test = std::atol(argv[++i]);
        } else if (std::strcmp(argv[i], "--demo-chevron") == 0) {
            demo_chevron = true;
        } else if (std::strcmp(argv[i], "--demo-vortex") == 0) {
            demo_vortex = true;
        } else if (std::strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
            layout_arg = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--dip-at") == 0 && i + 1 < argc) {
            dip_at = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--dip-burst") == 0 && i + 1 < argc) {
            dip_burst = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--print-out") == 0 && i + 1 < argc) {
            print_out = argv[++i];
        } else if (std::strcmp(argv[i], "--cycle-visuals") == 0) {
            cycle_visuals = true;
        } else if (std::strcmp(argv[i], "--map-cc") == 0 && i + 1 < argc) {
            int cc = -1, target = -1;
            if (std::sscanf(argv[++i], "%d:%d", &cc, &target) == 2 &&
                cc >= 0 && cc <= 127 && target >= 0 && target < SUMI_CTL_COUNT) {
                map_cc = cc;
                map_target = target;
            } else {
                std::fprintf(stderr, "bad --map-cc, expected <cc>:<target>\n");
                return 2;
            }
        } else {
            std::fprintf(stderr,
                         "usage: %s [--exit-after <s>] [--resize-test] [--sim-scale <f>]\n"
                         "          [--drop-test <n>] [--demo-chevron] [--demo-vortex]\n",
                         argv[0]);
            return 2;
        }
    }

    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "midi-sink", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    void* layer = sumi_macos_attach_metal_layer(window);
    if (!layer) {
        std::fprintf(stderr, "failed to attach CAMetalLayer\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    (void)yscale;

    sumi_config_t config = {};
    config.native_surface_handle = layer;
    config.backend = SUMI_BACKEND_METAL;
    config.width = (uint32_t)fbw;
    config.height = (uint32_t)fbh;
    config.pixel_ratio = xscale;
    config.log_cb = log_cb;
    config.log_user = nullptr;

    sumi_instance_t* inst = sumi_create(&config);
    if (!inst) {
        std::fprintf(stderr, "sumi_create failed\n");
        sumi_macos_detach_metal_layer(window, layer);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("sumi_version: %u.%u.%u\n",
                sumi_version() >> 16, (sumi_version() >> 8) & 0xFF, sumi_version() & 0xFF);

    if (sim_scale > 0.0f) {
        sumi_params_t params;
        sumi_get_params(inst, &params);
        params.sim_scale = sim_scale;
        sumi_set_params(inst, &params);
    }
    if (layout_arg >= 0) {
        sumi_params_t params;
        sumi_get_params(inst, &params);
        params.pitch_layout = (uint32_t)layout_arg;
        sumi_set_params(inst, &params);
        std::printf("layout: %d\n", layout_arg);
    }
    if (map_cc >= 0) {
        sumi_map_cc(inst, 0xFF, (uint8_t)map_cc, (sumi_ctl_t)map_target);
        std::printf("mapped CC%d -> ctl %d\n", map_cc, map_target);
    }

    void* midi = sumi_midi_harness_start(inst);
    if (!midi) {
        std::fprintf(stderr, "[midi] harness failed to start (continuing without MIDI)\n");
    }

    AppState app;
    app.inst = inst;
    if (print_out) std::snprintf(app.print_path, sizeof(app.print_path), "%s", print_out);
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetMouseButtonCallback(window, mouse_button_cb);
    glfwSetCursorPosCallback(window, cursor_pos_cb);
    glfwSetKeyCallback(window, key_cb);

    double start = glfwGetTime();
    double last = start;
    uint64_t frames = 0;
    int resize_step = 0;
    long drops_done = 0;
    int demo_frame = 0;
    double dt_min = 1e9, dt_max = 0.0;   // first frame excluded (startup cost)
    bool dip_done = false, print_saved = false;
    double dip_time = -1.0, dip_worst = 0.0;
    int visual_step = 0;
    int burst_step = 0;   // §5.3 double-buffer stress: 3 dips, delayed reads

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        // ---- scripted test/demo inputs (before update) ----
        if (drop_test > 0 && drops_done < drop_test) {
            sumi_add_drop(inst, 0.5f, 0.5f, 0.18f, 0);
            drops_done++;
            if (drops_done == drop_test) {
                std::printf("drop-test: %ld drops done\n", drops_done);
                std::fflush(stdout);
            }
        }
        if (demo_chevron || demo_vortex) {
            demo_frame++;
            if (demo_frame <= 12) {
                sumi_add_drop(inst, 0.5f, 0.5f, 0.15f, 0);
                if (demo_frame == 12) {
                    std::printf("demo: rings placed\n");
                    std::fflush(stdout);
                }
            } else if (demo_chevron && demo_frame >= 20 && demo_frame < 60) {
                // A wider comb tooth than the mouse default so the wake spans
                // several rings (the effect's width is exactly alpha).
                const float step = 0.8f / 40.0f;
                const float y = 0.1f + (float)(demo_frame - 20) * step;
                sumi_add_tine(inst, 0.5f, y, 0.5f, y + step, 0.09f, 0.3f / 40.0f);
                if (demo_frame == 59) { std::printf("demo: chevron done\n"); std::fflush(stdout); }
            } else if (demo_vortex && demo_frame >= 20 && demo_frame < 50) {
                // Offset from the ring center: rotation concentric with the
                // rings would be invisible (circles are rotation-invariant).
                sumi_add_vortex(inst, 0.60f, 0.38f, 0.10f, 0.30f);
                if (demo_frame == 49) { std::printf("demo: vortex done\n"); std::fflush(stdout); }
            }
        }

        sumi_update(inst, dt);
        sumi_render(inst);
        frames++;
        if (frames > 1) {
            if (dt < dt_min) dt_min = dt;
            if (dt > dt_max) dt_max = dt;
        }

        glfwPollEvents();
        sumi_midi_harness_poll(midi);

        const double elapsed = now - start;
        if (dip_burst > 0.0) {
            // Three dips at t, t+0.2, t+0.25 with reads deferred to t+1.0:
            // both buffers must fill, the third dip must be refused, and both
            // prints must read back intact afterwards.
            if (burst_step == 0 && elapsed >= dip_burst) {
                burst_step = 1;
                sumi_trigger_paper_dip(inst);
                std::printf("[burst] dip 1 at t=%.2fs\n", elapsed);
            } else if (burst_step == 1 && elapsed >= dip_burst + 0.2) {
                burst_step = 2;
                sumi_trigger_paper_dip(inst);
                std::printf("[burst] dip 2 at t=%.2fs\n", elapsed);
            } else if (burst_step == 2 && elapsed >= dip_burst + 0.25) {
                burst_step = 3;
                std::printf("[burst] dip 3 at t=%.2fs (expect refusal)\n", elapsed);
                sumi_trigger_paper_dip(inst);
            } else if (burst_step == 3 && elapsed >= dip_burst + 1.0) {
                burst_step = 4;
                save_print(inst, "burst_print_newest.png");   // consumes newest
                save_print(inst, "burst_print_oldest.png");   // then the other
            }
        }
        if (dip_at > 0.0 && !dip_done && elapsed >= dip_at) {
            dip_done = true;
            dip_time = now;
            sumi_trigger_paper_dip(inst);
            std::printf("[dip] triggered at t=%.2fs\n", elapsed);
        }
        if (dip_time > 0.0 && now - dip_time <= 1.0 && frames > 1 && dt > dip_worst) {
            dip_worst = dt;   // worst frame time in the second after the dip
        }
        if (dip_done && print_out && !print_saved) {
            uint32_t pw = 0, ph = 0;
            if (sumi_read_print(inst, nullptr, 0, &pw, &ph)) {
                print_saved = save_print(inst, print_out);
            }
        }
        if (cycle_visuals && frames % 180 == 0 && frames > 0) {
            sumi_params_t p;
            sumi_get_params(inst, &p);
            p.active_palette_id = (uint32_t)(visual_step % 3);
            p.pitch_layout = (uint32_t)(visual_step % 3);
            p.paper_roughness = 0.3f + 0.35f * (float)(visual_step % 3);
            sumi_set_params(inst, &p);
            print_params(&p);
            visual_step++;
        }
        if (resize_test) {
            if (resize_step == 0 && elapsed > 1.0) {
                glfwSetWindowSize(window, 900, 500);
                resize_step = 1;
            } else if (resize_step == 1 && elapsed > 2.0) {
                glfwSetWindowSize(window, 1440, 900);
                resize_step = 2;
            }
        }
        if (exit_after > 0.0 && elapsed >= exit_after) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    const double total = glfwGetTime() - start;
    if (total > 0.0 && frames > 1) {
        std::printf("frames: %llu in %.2fs (avg %.1f fps), frame time min/max %.2f/%.2f ms\n",
                    (unsigned long long)frames, total, (double)frames / total,
                    dt_min * 1000.0, dt_max * 1000.0);
    }

    if (dip_time > 0.0) {
        std::printf("dip window worst frame: %.2f ms\n", dip_worst * 1000.0);
    }
    std::printf("dropped MIDI messages: %u\n", sumi_dropped_midi_count(inst));
    sumi_midi_harness_stop(midi);
    sumi_destroy(inst);
    sumi_macos_detach_metal_layer(window, layer);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
