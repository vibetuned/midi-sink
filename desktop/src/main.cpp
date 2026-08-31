// Desktop harness (PROJECT_SPEC.md §1): GLFW_NO_API window, CAMetalLayer via
// metal_layer_glue, frame loop driving sumi_update/sumi_render, sumi_resize on
// framebuffer-size changes. MIDI (midi_harness.cpp) arrives in a later step.
//
// Harness-only test flags (for automated DONE evidence, see DECISIONS.md):
//   --exit-after <seconds>   close the window cleanly after N seconds
//   --resize-test            programmatically resize the window twice while running
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "sumi_core.h"
#include "metal_layer_glue.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void log_cb(int level, const char* msg, void* /*user*/) {
    static const char* names[] = {"PANIC", "ERROR", "WARN", "INFO"};
    const char* name = (level >= 0 && level <= 3) ? names[level] : "?";
    std::fprintf(stderr, "[sumi %s] %s\n", name, msg);
}

static void glfw_error_cb(int code, const char* desc) {
    std::fprintf(stderr, "[glfw %d] %s\n", code, desc);
}

static void framebuffer_size_cb(GLFWwindow* window, int w, int h) {
    sumi_instance_t* inst = (sumi_instance_t*)glfwGetWindowUserPointer(window);
    if (!inst || w <= 0 || h <= 0) return;
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    (void)yscale;
    std::printf("resize -> %dx%d px (scale %.1f)\n", w, h, xscale);
    sumi_resize(inst, (uint32_t)w, (uint32_t)h, xscale);
}

int main(int argc, char** argv) {
    double exit_after = 0.0;   // 0 = run until the window is closed
    bool resize_test = false;
    float sim_scale = 0.0f;    // 0 = leave the core default
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--exit-after") == 0 && i + 1 < argc) {
            exit_after = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--resize-test") == 0) {
            resize_test = true;
        } else if (std::strcmp(argv[i], "--sim-scale") == 0 && i + 1 < argc) {
            sim_scale = (float)std::atof(argv[++i]);
        } else {
            std::fprintf(stderr,
                         "usage: %s [--exit-after <seconds>] [--resize-test] [--sim-scale <f>]\n",
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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Suminagashi", nullptr, nullptr);
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

    glfwSetWindowUserPointer(window, inst);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_cb);

    double start = glfwGetTime();
    double last = start;
    uint64_t frames = 0;
    int resize_step = 0;
    double dt_min = 1e9, dt_max = 0.0;   // first frame excluded (startup cost)

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        sumi_update(inst, dt);
        sumi_render(inst);
        frames++;
        if (frames > 1) {
            if (dt < dt_min) dt_min = dt;
            if (dt > dt_max) dt_max = dt;
        }

        glfwPollEvents();

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

    sumi_destroy(inst);
    sumi_macos_detach_metal_layer(window, layer);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
