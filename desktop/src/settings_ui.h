// settings_ui.h — the shared settings window (Phase 5 §2, DECISIONS_4 #2):
// Dear ImGui in a second GLFW window with its own OpenGL context — one
// implementation for macOS, Windows and Linux. The window shows the iOS
// sheet's rows, the CC-map editor, the MIDI port list, the dip button, About
// and the first-run hint; with --dev it also shows the lab bench.
#pragma once

#include "sumi_core.h"

struct GLFWwindow;
struct AppSettings;

struct SettingsUiInfo {
    const char* app_version;   // SUMI_APP_VERSION (tag / git describe)
    const char* git_commit;    // short hash
    bool        dev;           // --dev: show the lab bench section
};

class SettingsUi {
public:
    // Creates the window (hidden) and the ImGui context. `main_window` is used
    // to place the settings beside the canvas. Returns false if the platform
    // could not give us a GL 3.2 context (the app still runs; settings are
    // then unreachable and a line says so).
    bool init(GLFWwindow* main_window, const SettingsUiInfo& info);
    void shutdown();

    void show();
    void hide();
    bool visible() const { return visible_; }
    GLFWwindow* window() const { return window_; }

    // One frame: draws the window if visible. Returns true when a setting
    // changed (the caller applies + persists). Restores the caller's GL
    // context on platforms where the main window owns one (Linux).
    bool frame(AppSettings& s, sumi_instance_t* inst, void* midi, GLFWwindow* main_window);

    // Set when the user closed the settings window this frame (so the caller
    // can persist `settings_open = false`).
    bool consume_closed() { const bool c = closed_; closed_ = false; return c; }

private:
    bool draw(AppSettings& s, sumi_instance_t* inst, void* midi);
    GLFWwindow*   window_ = nullptr;
    SettingsUiInfo info_{};
    bool visible_ = false;
    bool closed_ = false;
    bool placed_ = false;
    int  pos_x_ = 0, pos_y_ = 0;
    float scale_ = 1.0f;
    // CC-map editor scratch state
    int  new_channel_ = 0;     // 0 = any, 1..16
    int  new_cc_ = 30;
    int  new_target_ = 0;
    char print_dir_buf_[1024] = {};
    bool print_dir_synced_ = false;
    char status_[256] = {};
    double status_until_ = 0.0;
};
