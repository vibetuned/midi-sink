// settings_ui.cpp — Dear ImGui settings window (see settings_ui.h).
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "settings_ui.h"
#include "app_settings.h"
#include "midi_harness.h"
#include "print_export.h"
#include "dev_tools.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// The one GLSL version every desktop can give a settings window: GL 3.2
// core is the macOS ceiling for a forward-compatible context (DECISIONS_4 #2).
static const char* GLSL_VERSION = "#version 150";

bool SettingsUi::init(GLFWwindow* main_window, const SettingsUiInfo& info) {
    info_ = info;
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    // Desktop-entry identity (Linux): same app_id/class as the canvas so the
    // compositor groups the two windows under one icon (DECISIONS_2 #39).
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "midi-sink");
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "midi-sink");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "midi-sink");

    // HiDPI (found on Windows at 125%, Step 29): window sizes are PHYSICAL
    // pixels on Windows/X11, so the window and the ImGui style must scale by
    // the content scale there. The backend helper is platform-correct: it
    // returns 1.0 on Apple (Retina lives in FramebufferScale) and on Wayland
    // (the compositor scales) — so this is a no-op exactly where it must be.
    scale_ = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    if (!(scale_ > 0.0f)) scale_ = 1.0f;   // virtual monitors can report 0
    const int SETTINGS_W = (int)(560.0f * scale_), SETTINGS_H = (int)(760.0f * scale_);
    window_ = glfwCreateWindow(SETTINGS_W, SETTINGS_H, "midi-sink \xe2\x80\x94 Settings", nullptr, nullptr);
    glfwDefaultWindowHints();
    if (!window_) {
        std::fprintf(stderr, "[settings] no OpenGL 3.2 context for the settings window\n");
        return false;
    }
    // Beside the canvas, top-aligned — right of it when the monitor has room,
    // left otherwise, and never past the work area's edges.
    if (main_window) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetWindowPos(main_window, &mx, &my);
        glfwGetWindowSize(main_window, &mw, &mh);
        // A hidden window has no laid-out frame yet on macOS (its size query
        // can read 0) — use the requested size for the placement math.
        const int sw = SETTINGS_W, sh = SETTINGS_H;
        int wax = 0, way = 0, waw = 0, wah = 0;
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        if (mon) glfwGetMonitorWorkarea(mon, &wax, &way, &waw, &wah);
        int x = mx + mw + 16, y = my;
        if (waw > 0) {
            if (x + sw > wax + waw) x = mx - sw - 16;          // no room right: go left
            if (x < wax) x = wax + waw - sw;                   // no room left: flush right
            if (x < wax) x = wax;
            if (y + sh > way + wah) y = way + wah - sh;
            if (y < way) y = way;
        }
        glfwSetWindowPos(window_, x, y);
        pos_x_ = x; pos_y_ = y;
        std::fprintf(stderr, "[settings] window at %d,%d (canvas %d,%d %dx%d; work area %d,%d %dx%d)\n",
                     x, y, mx, my, mw, mh, wax, way, waw, wah);
    }
    GLFWwindow* prev = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // our own INI holds the app settings; ImGui keeps none
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.ScaleAllSizes(scale_);              // paddings/spacing follow the DPI
    style.FontScaleMain = 1.15f * scale_;     // a touch larger than ImGui's 13 px default

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(GLSL_VERSION);
    glfwMakeContextCurrent(prev);
    return true;
}

void SettingsUi::shutdown() {
    if (!window_) return;
    GLFWwindow* prev = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwMakeContextCurrent(prev == window_ ? nullptr : prev);
    glfwDestroyWindow(window_);
    window_ = nullptr;
}

void SettingsUi::show() {
    if (!window_) return;
    visible_ = true;
    glfwShowWindow(window_);
    if (!placed_) {
        // macOS gives a never-shown window its own frame on first show, which
        // overrides a position set while hidden — apply ours once it is up.
        glfwSetWindowPos(window_, pos_x_, pos_y_);
        placed_ = true;
    }
    glfwFocusWindow(window_);
}

void SettingsUi::hide() {
    if (!window_) return;
    visible_ = false;
    glfwHideWindow(window_);
}

bool SettingsUi::frame(AppSettings& s, sumi_instance_t* inst, void* midi, GLFWwindow* main_window) {
    if (!window_ || !visible_) return false;
    if (glfwWindowShouldClose(window_)) {
        glfwSetWindowShouldClose(window_, GLFW_FALSE);
        hide();
        closed_ = true;
        return false;
    }
    GLFWwindow* prev = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    const bool changed = draw(s, inst, midi);
    ImGui::Render();
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(window_, &fw, &fh);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());   // the backend clears + binds
    glfwSwapBuffers(window_);
    // Linux: the main window's context is the CORE'S (§5.1 GL exception) —
    // hand it back before the next sumi_update. Elsewhere `prev` is null.
    glfwMakeContextCurrent(prev == window_ ? nullptr : prev);
    (void)main_window;
    return changed;
}

// ---- widgets --------------------------------------------------------------

static bool combo_u32(const char* label, uint32_t* value, uint32_t count,
                      const char* (*name)(uint32_t)) {
    bool changed = false;
    const uint32_t cur = *value < count ? *value : 0;
    if (ImGui::BeginCombo(label, name(cur))) {
        for (uint32_t i = 0; i < count; i++) {
            const bool sel = i == cur;
            if (ImGui::Selectable(name(i), sel)) { *value = i; changed = true; }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

// Three choices whose enum values need not be 0/1/2 (the vortex profile
// skips 2: Lamb-Oseen is a gesture, never a CC-routed profile).
static bool radio_tri(const char* label, uint32_t* value,
                      const char* a, uint32_t va, const char* b, uint32_t vb, const char* c, uint32_t vc) {
    bool changed = false;
    ImGui::TextUnformatted(label);
    ImGui::SameLine(200.0f);
    ImGui::PushID(label);
    int v = *value == vb ? 1 : (*value == vc ? 2 : 0);
    if (ImGui::RadioButton(a, &v, 0)) changed = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(b, &v, 1)) changed = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(c, &v, 2)) changed = true;
    ImGui::PopID();
    if (changed) *value = v == 1 ? vb : (v == 2 ? vc : va);
    return changed;
}

static bool radio_pair(const char* label, uint32_t* value, const char* a, const char* b) {
    bool changed = false;
    ImGui::TextUnformatted(label);
    ImGui::SameLine(200.0f);
    // Scope the buttons under the row's label: a choice named "Ripple" must
    // not share an ID with the "Ripple" section header (ImGui IDs are label
    // hashes within the window).
    ImGui::PushID(label);
    int v = *value ? 1 : 0;
    if (ImGui::RadioButton(a, &v, 0)) changed = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(b, &v, 1)) changed = true;
    ImGui::PopID();
    if (changed) *value = (uint32_t)v;
    return changed;
}

// Disabled-colour paragraph that wraps to the window (TextDisabled does not).
static void note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static void help(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

bool SettingsUi::draw(AppSettings& s, sumi_instance_t* inst, void* midi) {
    bool changed = false;
    sumi_params_t& p = s.params;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("settings", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ---- first-run hint (one dismissible line, Phase 5 §2) ----
    if (!s.first_run_dismissed) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.93f, 0.82f, 1.0f));
        ImGui::BeginChild("hint", ImVec2(0, ImGui::GetFontSize() * 4.6f), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("Plug in a MIDI instrument - it appears under \"MIDI inputs\" "
                           "below within a second and plays straight away. Mouse: click = drop, "
                           "drag = comb, right-drag = vortex. This window is Settings; close it "
                           "any time and bring it back with %s.",
#if defined(__APPLE__)
                           "Cmd ,");
#else
                           "Ctrl ,");
#endif
        if (ImGui::Button("Got it")) { s.first_run_dismissed = true; changed = true; }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ---- canvas (first: the paper dip is the most-used control, #78) ----
    if (ImGui::CollapsingHeader("Canvas", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Paper dip (fresh sheet)")) {
            if (inst) sumi_trigger_paper_dip(inst);
            std::snprintf(status_, sizeof(status_), "Dipped - the print is ready to save.");
            status_until_ = glfwGetTime() + 4.0;
        }
        help("Freezes and snapshots the canvas as a print, then starts a clean sheet.");
        if (!print_dir_synced_) {
            std::snprintf(print_dir_buf_, sizeof(print_dir_buf_), "%s", s.print_dir.c_str());
            print_dir_synced_ = true;
        }
        if (ImGui::InputText("Print folder", print_dir_buf_, sizeof(print_dir_buf_))) {
            s.print_dir = print_dir_buf_;
            changed = true;
        }
        if (ImGui::Button("Save last print as PNG")) {
            const std::string path = default_print_path(s.print_dir);
            if (inst && save_print_png(inst, path.c_str())) {
                std::snprintf(status_, sizeof(status_), "Saving %s", path.c_str());
            } else {
                std::snprintf(status_, sizeof(status_), "No print yet - dip the paper first.");
            }
            status_until_ = glfwGetTime() + 5.0;
        }
        if (status_[0] && glfwGetTime() < status_until_) {
            ImGui::TextDisabled("%s", status_);
        }
    }

    // ---- layout & look ----
    if (ImGui::CollapsingHeader("Layout & look", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= combo_u32("Pitch layout", &p.pitch_layout, 8, app_layout_name);
        changed |= combo_u32("Palette", &p.active_palette_id, 3, app_palette_name);
        changed |= ImGui::SliderFloat("Viscosity", &p.fluid_viscosity, 0.0f, 1.0f, "%.2f");
        changed |= ImGui::SliderFloat("Ink feed (pressure)", &p.expansion_rate, 0.1f, 4.0f, "%.2f");
        changed |= ImGui::SliderFloat("Paper roughness", &p.paper_roughness, 0.0f, 1.0f, "%.2f");
        if (p.pitch_layout == SUMI_LAYOUT_ROLL_H || p.pitch_layout == SUMI_LAYOUT_ROLL_V ||
            p.pitch_layout == SUMI_LAYOUT_ROLL_H_RIGHT || p.pitch_layout == SUMI_LAYOUT_ROLL_V_BOTTOM) {
            changed |= ImGui::SliderFloat("Tempo (BPM)", &p.bpm, 20.0f, 300.0f, "%.0f");
            changed |= ImGui::SliderFloat("Roll speed", &p.roll_speed, 0.02f, 0.25f, "%.4f");
            help("Canvas lengths per beat. 1/16 keeps 4 bars of 4/4 on screen.");
        }
        bool full = p.sim_scale >= 0.99f;
        if (ImGui::Checkbox("Full-resolution simulation", &full)) {
            p.sim_scale = full ? 1.0f : 0.75f;
            changed = true;
        }
        help("Off = 0.75x field for laptops that run warm under dense MPE streams.");
    }

    // ---- expression routing (the iOS sheet's rows) ----
    if (ImGui::CollapsingHeader("Expression routing", ImGuiTreeNodeFlags_DefaultOpen)) {
        // #60: the input dialect is a setting (MPE default), not a detection.
        {
            ImGui::TextUnformatted("Input");
            ImGui::SameLine(200.0f);
            ImGui::PushID("Input");
            int im = (int)s.input_mode;
            if (ImGui::RadioButton("MPE", &im, 1)) changed = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Classic keyboard", &im, 2)) changed = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Wind", &im, 3)) changed = true;
            ImGui::PopID();
            s.input_mode = (uint32_t)im;
        }
        if (s.input_mode == 3) {
            help("Wind: one voice, played exactly as MPE - each note a strike drop, breath "
                 "(CC 2 / 7 / 11 or channel pressure) the unbounded feed, CC 74 / poly pressure / "
                 "a member-channel bend the IMU layer - plus a wake dragging the sounding drop "
                 "to the next note on every legato change.");
        } else if (s.input_mode == 2) {
            help("Classic: every note is its own voice on any channel; bend is the global shear "
                 "tine and the mod wheel the vortex. For a keyboard sending inside the member "
                 "zone (channels 2-16).");
        } else {
            help("MPE (default): per-note bend, pressure and CC 74 on the member channels; a "
                 "plain keyboard on channel 1 still plays chords. CC 64 never touches the canvas.");
        }
        if (radio_pair("Per-note bend", &p.bend_mode, "Glide", "Ripple")) {
            p.ripple_bake = p.bend_mode;   // the Ripple choice bakes (DECISIONS_3 #36)
            changed = true;
        }
        help("Glide drags the note's drop along the pitch axis. Ripple shimmers the "
             "water by the bend's distance from center and the drop holds.");
        changed |= radio_pair("Channel pressure", &p.press_mode, "Ink feed", "Swirl");
        help("Which effect aftertouch (0xD0) plays. Poly pressure (0xA0) always swirls.");
        changed |= radio_tri("Slide (CC 74)", &p.slide_mode, "Hue", 0, "Pinch", 1, "Spark k", 2);   // v0.13: the third is the Anod default to come
        if (p.slide_mode == 1) {
            changed |= radio_pair("Pinch style", &p.pinch_variant, "Saddle", "Crossed tines");
        }
        changed |= radio_tri("Vortex profile", &p.vortex_profile,
                             "Exponential", SUMI_VORTEX_EXPONENTIAL, "Rankine", SUMI_VORTEX_RANKINE,
                             "Torsion", SUMI_VORTEX_TORSION);
        help(p.vortex_profile == SUMI_VORTEX_TORSION
                 ? "Torsion: rings of alternating angular shear, theta' = theta + A sin(k r - phi) e^(-r/R) - "
                   "exact at any amplitude. Wavelength and phase ride CC 104 / 105 (Phase 6)."
                 : "Exponential: diffuse, breath-like. Rankine: a rigid core that spins as a disk. "
                   "Torsion: rings of angular shear (Phase 6).");
        {
            bool sweep = p.torsion_sweep == 1;
            if (ImGui::Checkbox("Torsion sweep on note-on", &sweep)) { p.torsion_sweep = sweep ? 1u : 0u; changed = true; }
            help("Every strike also fires an outward wave of torsion around its drop, fading over about "
                 "two seconds (Phase 6 preview - the Anod medium's binding tables will own this).");
        }
        changed |= radio_pair("Stylus wake", &p.wake_profile, "Inviscid doublet", "Viscous stroke");
        if (p.wake_profile == 1) {
            if (ImGui::SliderFloat("Spread (l/a)", &p.wake_spread, 1.5f, 12.0f, "%.1f")) changed = true;
            help("How far the stroke's momentum has diffused, in tip radii: small = sharp and "
                 "close, large = soft and far-reaching (v0.7, the 2-D Stokeslet stroke).");
        } else {
            help("Doublet: the exact potential flow around a rigid tip. Viscous: the impulse of "
                 "a tip in a Stokes layer, spread by viscosity.");
        }
    }

    // ---- ripple ----
    if (ImGui::CollapsingHeader("Ripple", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int amp_cc = app_settings_route_for(s, SUMI_CTL_RIPPLE_AMP);
        const int frq_cc = app_settings_route_for(s, SUMI_CTL_RIPPLE_FREQ);
        ImGui::BeginDisabled(amp_cc < 0);
        if (ImGui::SliderInt("Amount", &s.ripple_amp_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        ImGui::BeginDisabled(frq_cc < 0);
        if (ImGui::SliderInt("Wavelength", &s.ripple_freq_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        if (amp_cc < 0 || frq_cc < 0) {
            note("Route a CC to the ripple dimensions in the CC map to use these.");
        } else {
            char line[160];
            std::snprintf(line, sizeof(line), "Sent as CC %d / CC %d through the MIDI path (the same "
                          "route a controller would use).", amp_cc, frq_cc);
            note(line);
        }
        float deg = p.ripple_angle * 57.29578f;
        if (ImGui::SliderFloat("Angle", &deg, 0.0f, 180.0f, "%.0f\xc2\xb0")) {
            p.ripple_angle = deg / 57.29578f;
            changed = true;
        }
    }

    // ---- Chladni cellular flow (Phase 6 step 37, MEDIUM §2.2) ----
    if (ImGui::CollapsingHeader("Chladni", ImGuiTreeNodeFlags_DefaultOpen)) {
        const int a_cc = app_settings_route_for(s, SUMI_CTL_CHLADNI_A);
        const int b_cc = app_settings_route_for(s, SUMI_CTL_CHLADNI_B);
        ImGui::BeginDisabled(a_cc < 0);
        if (ImGui::SliderInt("Stir (A)", &s.chladni_a_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        help("The layout is the plate: an eddy in every cell (neighbours counter-rotate) and the cell boundaries "
             "the lines between them - a note's drop spins in place and the ink is stretched along the boundaries "
             "into the figure that outlines the grid. Bakes into the field while held; the dip is the undo.");
        ImGui::BeginDisabled(b_cc < 0);
        if (ImGui::SliderInt("Balance (B)", &s.chladni_b_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        help("Between the two diagonal waves the flow is made of: 0 the cellular flow, 64 a single diagonal "
             "wave, 127 the cells turning the other way.");
        if (a_cc < 0 || b_cc < 0) note("Route a CC to the Chladni dimensions in the CC map to use these.");
        if (ImGui::SliderFloat("Cell size", &p.chladni_cell, 0.5f, 1.5f, "%.2f cells")) changed = true;
        help("The eddies' pitch as a multiple of the layout's cell, anchored on the cell centres: 1 puts one "
             "eddy in every cell, 0.5 four, 1.5 spans a cell and a half.");
    }

    // ---- Viscous multipole burst (Phase 6 step 38, MEDIUM §2.3) ----
    if (ImGui::CollapsingHeader("Burst", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("Age", &p.burst_age, 1.5f, 12.0f, "%.1f x core")) changed = true;
        help("How far the discharge diffuses before it dies: the final diffusion length as a multiple of the "
             "strike's core. 1.5 keeps the lobes sharp and close, 12 soft and far-reaching. The amplitude is "
             "measured over the burst's own age, so this shapes the strike rather than scaling it.");
        if (ImGui::SliderFloat("Life", &p.burst_life, 0.0f, 4.0f, "%.2f s")) changed = true;
        help("The release: seconds for the age to grow from the core to its final value (0 = the whole burst "
             "at once). The diffusion length squared grows linearly in time, so most of the motion lands "
             "early - the discharge blooms sharp and dies soft.");
        {
            int order = (int)p.burst_order;
            if (ImGui::SliderInt("Order", &order, 2, 8, "m = %d")) { p.burst_order = (uint32_t)order; changed = true; }
        }
        help("The multipole order: 2 is the quadrupole - two lobes eject along the axis, two draw in across "
             "it - and m lobes eject for order m; the higher orders keep their motion close to the core. 8 is "
             "the top the pass budget covers. The binding tables will pick the order per note (step 42).");
        note("Fired by the lab bench (U at the cursor, the axis toward the centre) and the web scene; the "
             "strike route arrives with the medium's binding tables (step 42).");
    }

    // ---- Spark shear & the composed strike (Phase 6 step 39, MEDIUM §2.4) ----
    if (ImGui::CollapsingHeader("Spark", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("Shear", &p.spark_shear, 0.0f, 2.0f, "%.2f x radius")) changed = true;
        help("The jagged streamers' total kick as a multiple of the strike radius, spent over the release along "
             "and across the strike's axis. 0 leaves the spark to its blast and its lobes.");
        if (ImGui::SliderFloat("Decay", &p.spark_tau, 0.05f, 2.0f, "%.2f s")) changed = true;
        help("The time constant: the kick decays as e^(-t/tau) and the episode is over after four of them.");
        {
            int stack = (int)p.spark_stack;
            if (ImGui::SliderInt("Octaves", &stack, 1, 4, "%d")) { p.spark_stack = (uint32_t)stack; changed = true; }
        }
        help("How many octaves the profile stacks: k, 2k, 4k, 8k with halving weights - one is a plain triangle "
             "wave, four the most jagged.");
        {
            ImGui::TextUnformatted("Profile");
            ImGui::SameLine(200.0f);
            if (ImGui::RadioButton("triangle", p.spark_profile == 0)) { p.spark_profile = 0; changed = true; }
            ImGui::SameLine();
            if (ImGui::RadioButton("noise", p.spark_profile == 1)) { p.spark_profile = 1; changed = true; }
        }
        help("Triangle waves or piecewise-linear noise: both are exact shears - the kinks become creases.");
        const int k_cc = app_settings_route_for(s, SUMI_CTL_SPARK_K);
        ImGui::BeginDisabled(k_cc < 0);
        if (ImGui::SliderInt("Frequency (k)", &s.spark_k_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        help("The streamers' wavenumber, from a thick channel (0) to fine streamers (127). The slide (CC 74) "
             "drives it when the slide mode is the spark - the Anod default to come with the binding tables.");
        if (k_cc < 0) note("Route a CC to the spark frequency in the CC map to use the slider.");
        note("Fired by the lab bench (Z at the cursor, the axis toward the centre) and the web scene; the strike "
             "route arrives with the medium's binding tables (step 42).");
    }

    // ---- Chirikov standard map (Phase 6 step 40, MEDIUM §2.5) ----
    if (ImGui::CollapsingHeader("Chirikov", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("K max", &p.chirikov_kmax, 0.0f, 2.0f, "%.2f")) changed = true;
        help("The chaos parameter of ONE step of the standard map for a full throw of the control in one frame. "
             "Greene's threshold is 0.9716: below it the ink follows smooth invariant sheets, above it filaments "
             "and island chains. A throw eased over several frames is that many gentler steps (the pendulum flow); "
             "a wheel thrown hard is one chaotic kick. The core caps each step at its erosion ceiling. 0 disables.");
        {
            int periods = (int)p.chirikov_periods;
            if (ImGui::SliderInt("Periods", &periods, 1, 8, "%d")) { p.chirikov_periods = (uint32_t)periods; changed = true; }
        }
        help("Kick waves per canvas height along x: the island chain sits half a period from the centre.");
        if (ImGui::SliderFloat("Drift", &p.chirikov_eps, 0.05f, 1.0f, "%.2f")) changed = true;
        help("The drift's scale: the kick amplitude follows as K/(k x drift) - a small drift means steep kicks, a large "
             "one shears the whole canvas.");
        const int k_cc = app_settings_route_for(s, SUMI_CTL_CHIRIKOV_K);
        ImGui::BeginDisabled(k_cc < 0);
        if (ImGui::SliderInt("Throw", &s.chirikov_k_cc, 0, 127)) changed = true;
        ImGui::EndDisabled();
        help("The control itself: every change is a throw and applies a step scaled by the change; moving it back "
             "applies the inverse steps. The mod wheel takes this under the Anod binding table (step 42).");
        if (k_cc < 0) note("Route a CC to the Chirikov throw in the CC map to use the slider.");
        note("The lab bench's I key applies one full step at the cursor at K max (Shift+I its exact inverse).");
    }

    // ---- CC map ----
    if (ImGui::CollapsingHeader("CC map", ImGuiTreeNodeFlags_DefaultOpen)) {
        note("Any controller's CC can drive a global dimension. Channel-specific routes "
             "override any-channel ones.");
        if (ImGui::BeginTable("ccmap", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("CC", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("Drives");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();
            int remove_at = -1;
            for (int i = 0; i < (int)s.cc_routes.size(); i++) {
                const CcRoute& r = s.cc_routes[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (r.channel == 0xFF) ImGui::TextUnformatted("any");
                else ImGui::Text("%u", (unsigned)r.channel + 1);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", (unsigned)r.cc);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(app_ctl_name(r.target));
                ImGui::TableSetColumnIndex(3);
                ImGui::PushID(i);
                if (ImGui::SmallButton("remove")) remove_at = i;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (remove_at >= 0) {
                s.cc_routes.erase(s.cc_routes.begin() + remove_at);
                changed = true;
            }
        }
        // add a route
        ImGui::SetNextItemWidth(90.0f);
        const char* ch_label = new_channel_ == 0 ? "any" : nullptr;
        char ch_buf[8];
        if (!ch_label) { std::snprintf(ch_buf, sizeof(ch_buf), "%d", new_channel_); ch_label = ch_buf; }
        if (ImGui::BeginCombo("##ch", ch_label)) {
            if (ImGui::Selectable("any", new_channel_ == 0)) new_channel_ = 0;
            for (int c = 1; c <= 16; c++) {
                char b[8];
                std::snprintf(b, sizeof(b), "%d", c);
                if (ImGui::Selectable(b, new_channel_ == c)) new_channel_ = c;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::InputInt("##cc", &new_cc_, 0, 0);
        if (new_cc_ < 0) new_cc_ = 0;
        if (new_cc_ > 127) new_cc_ = 127;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        uint32_t tgt = (uint32_t)new_target_;
        combo_u32("##target", &tgt, SUMI_CTL_COUNT, app_ctl_name);
        new_target_ = (int)tgt;
        ImGui::SameLine();
        if (ImGui::Button("Add route")) {
            const uint8_t ch = new_channel_ == 0 ? 0xFF : (uint8_t)(new_channel_ - 1);
            // Replace an identical (channel, cc) route rather than duplicating it.
            bool replaced = false;
            for (CcRoute& r : s.cc_routes) {
                if (r.channel == ch && r.cc == (uint8_t)new_cc_) { r.target = tgt; replaced = true; }
            }
            if (!replaced) s.cc_routes.push_back({ch, (uint8_t)new_cc_, tgt});
            changed = true;
        }
        if (ImGui::Button("Restore default map")) {
            app_settings_default_routes(s.cc_routes);
            changed = true;
        }
        note("CC 64 (sustain) and CC 74 on member channels are reserved by the engine.");
    }

    // ---- MIDI inputs ----
    if (ImGui::CollapsingHeader("MIDI inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!midi) {
            ImGui::TextColored(ImVec4(0.7f, 0.2f, 0.2f, 1.0f),
                               "The MIDI system failed to start; running without input.");
        } else {
            const int n = sumi_midi_harness_input_count(midi);
            if (n == 0) {
                ImGui::TextDisabled("No MIDI inputs found. Devices are opened automatically when they appear.");
            }
            for (int i = 0; i < n; i++) {
                char name[256];
                if (sumi_midi_harness_input_name(midi, i, name, sizeof(name))) {
                    ImGui::BulletText("%s", name);
                }
            }
            const double age = sumi_midi_harness_seconds_since_rescan(midi);
            ImGui::TextDisabled("Rescanned %.0f s ago (every second while running).", age);
            ImGui::SameLine();
            if (ImGui::SmallButton("Rescan now")) sumi_midi_harness_rescan_now(midi);
        }
    }

    // ---- window (#58) ----
    if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= ImGui::Checkbox("Fullscreen", &s.fullscreen);
#if defined(__APPLE__)
        help("The canvas fills its display. Toggle from the canvas with Control + Command + F.");
#else
        help("The canvas fills its display. Toggle from the canvas with F11.");
#endif
    }

    // ---- lab bench (--dev only) ----
    if (info_.dev && ImGui::CollapsingHeader("Lab bench (--dev)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Debug keys are live on the canvas window:");
        ImGui::TextUnformatted(dev_key_legend());
        changed |= radio_pair("Ripple insertion", &p.ripple_bake, "Live (view)", "Bake (field)");
        changed |= ImGui::SliderFloat("Smoothing (ms)", &p.smoothing_ms, 1.0f, 200.0f, "%.0f");
        if (midi) {
            bool raw = sumi_midi_harness_raw_log(midi);
            if (ImGui::Checkbox("Log every MIDI message to stderr", &raw)) {
                sumi_midi_harness_set_raw_log(midi, raw);
            }
        }
    }

    // ---- about ----
    if (ImGui::CollapsingHeader("About", ImGuiTreeNodeFlags_DefaultOpen)) {
        const uint32_t v = sumi_version();
        ImGui::Text("midi-sink %s", info_.app_version ? info_.app_version : "?");
        ImGui::TextDisabled("commit %s  |  engine libsumi %u.%u.%u",
                            info_.git_commit ? info_.git_commit : "?",
                            v >> 16, (v >> 8) & 0xFF, v & 0xFF);
        ImGui::TextDisabled("A suminagashi visualizer driven by expressive MIDI. AGPL-3.0.");
        if (!info_.dev) {
            ImGui::TextDisabled("Support: run with --dev for the lab bench and diagnostics.");
        }
    }

    ImGui::End();
    return changed;
}
