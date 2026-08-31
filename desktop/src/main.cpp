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

// Gesture tuning (harness-side defaults; normalized canvas-height units).
static const float DROP_RADIUS       = 0.06f;
static const float TINE_ALPHA        = 0.035f;
static const float TINE_MAG_SCALE    = 1.0f;    // magnitude = segment length
static const float VORTEX_RADIUS     = 0.18f;
static const float VORTEX_STRENGTH   = 4.0f;    // radians per unit drag speed
static const double DRAG_THRESHOLD_PX = 5.0;

struct AppState {
    sumi_instance_t* inst = nullptr;
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
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);
    glfwSetMouseButtonCallback(window, mouse_button_cb);
    glfwSetCursorPosCallback(window, cursor_pos_cb);

    double start = glfwGetTime();
    double last = start;
    uint64_t frames = 0;
    int resize_step = 0;
    long drops_done = 0;
    int demo_frame = 0;
    double dt_min = 1e9, dt_max = 0.0;   // first frame excluded (startup cost)

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

    std::printf("dropped MIDI messages: %u\n", sumi_dropped_midi_count(inst));
    sumi_midi_harness_stop(midi);
    sumi_destroy(inst);
    sumi_macos_detach_metal_layer(window, layer);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
