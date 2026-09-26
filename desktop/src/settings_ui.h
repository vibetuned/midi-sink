// settings_ui.h — the shared settings window (Phase 5 §2, DECISIONS_4 #2):
// Dear ImGui in a second GLFW window with its own OpenGL context — one
// implementation for macOS, Windows and Linux. The window shows the iOS
// sheet's rows, the CC-map editor, the MIDI port list, the dip button, About
// and the first-run hint; with --dev it also shows the lab bench.
#pragma once

#include "sumi_core.h"

struct GLFWwindow;
struct AppSettings;
class PrintLedger;   // Phase 6 step 43 (QOL §4)
typedef struct voxo_t voxo_t;   // Phase 7 step 47: the internal sound

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
    // Phase 6 step 43 (QOL §4): the print ledger the dip button feeds and the "Prints" section shows.
    void set_ledger(PrintLedger* l) { ledger_ = l; }
    // Phase 7 step 47 (SOUND §1): Voxo, for the "Sound" section's status line.
    void set_voxo(voxo_t* v) { voxo_ = v; }
    // Step 49: what main.cpp's sample load said (shown under the Sample row).
    void set_sample_status(const char* text);
    // Step 50: the compat report of the last preset load (shown once, until the next load).
    void set_preset_report(const char* text);

private:
    bool draw(AppSettings& s, sumi_instance_t* inst, void* midi);
    PrintLedger*  ledger_ = nullptr;
    voxo_t*       voxo_ = nullptr;
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
    char sample_buf_[1024] = {};       // step 49: the sample path being edited
    bool sample_synced_ = false;
    char sample_status_[256] = {};
    char preset_buf_[1024] = {};       // step 50: the preset path being edited
    bool preset_synced_ = false;
    char preset_report_[2200] = {};
    char status_[1200] = {};
    unsigned write_serial_seen_ = 0;   // #86: the last background write whose outcome was shown
    double status_until_ = 0.0;
};
