// replay_host.h — Phase 9 step 65 (QOL §1, DECISIONS_8 #22–#23): the desktop's
// recorder and player, on the replay library. Recording: the harness stages
// its bytes here (the render thread hands them to the core at each frame's
// start, so every byte's frame is exact), the gesture wrappers below call
// the core and record, the settings' apply and the ledger's dip report here.
// Playback: the harness is muted (the replay owns the loopback and the
// sound), the recorded frames run on the scripted clock — as many per
// display frame as the wall clock asks, each a sumi_update(dt recorded) +
// sumi_render — the viewer's window size and palette kept (the replay
// re-dips at theirs), the banner names the source.
#pragma once

#include "sumi_core.h"
#include "sumi_replay.h"

#include <cstdint>
#include <string>
#include <vector>

struct AppSettings;
struct GLFWwindow;
class PrintLedger;
typedef struct voxo_t voxo_t;

class ReplayHost {
public:
    void init(sumi_instance_t* inst, void* midi, voxo_t* voxo, GLFWwindow* window, void* metal_layer,
              uint32_t backend, const char* app_version);
    void set_ledger(PrintLedger* l) { ledger_ = l; }
    void shutdown(const AppSettings& s);

    // ---- recording ----
    bool recording() const { return rec_ != nullptr; }
    bool start_recording(const AppSettings& s);          // the sheet kept and dipped, the session as the header, the control CCs re-sent
    bool stop_recording(std::string* path_out);          // saved under replays_dir() as <stamp>.sumireplay
    void frame_begin(double now, double dt);             // every frame, right before sumi_update
    void on_settings_applied(const AppSettings& s);      // after app_settings_apply: a state event
    void on_resize(uint32_t w, uint32_t h, float ratio);
    void on_dip();                                       // the ledger's hook, at sumi_trigger_paper_dip
    void on_gesture(uint32_t kind, const float* args, uint32_t n);   // the orbit trace's hook
    // The gesture wrappers: the core's call, recorded when recording.
    void  tap(float x, float y, float r);
    void  pinch(float x, float y, float k, float angle, float span);
    void  twist(float x, float y, float strength, float radius, uint32_t profile);
    float press(float x, float y, float R, float up, float down, double dt);
    void  press_end();
    void  tine(float x0, float y0, float x1, float y1, float alpha, float magnitude);
    void  wake(float x0, float y0, float x1, float y1, float tip);
    uint32_t rec_frames() const;
    double   rec_seconds() const;
    bool     rec_full() const;

    // ---- playback ----
    bool playing() const { return play_ != nullptr; }
    bool play(const std::string& path, std::string* why);
    void stop_playback(const AppSettings& s);            // the harness back, the viewer's settings re-applied
    // One display frame while playing: runs the frames the wall clock asks for (each an update + render).
    // Returns how many ran; 0 = the caller re-composites (sumi_render alone keeps the field).
    int  playback_frame(double dt_display);
    bool playback_done() const { return play_done_; }
    std::string banner() const;
    double   play_elapsed() const;
    double   play_duration() const;
    uint32_t play_position() const;
    uint32_t play_frames() const;

    // ---- files ----
    static std::string replays_dir();
    static std::vector<std::string> replay_names();      // the files under replays_dir(), newest first
    static std::string replay_path(const std::string& name);
    const std::string& status() const { return status_; }

private:
    static void push_core(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src);
    static void push_core_and_sound(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src);
    static void stage_cb(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src);
    void set_display_sync(bool on);
    void set_title(const std::string& t);
    sumi_instance_t* inst_ = nullptr;
    void*            midi_ = nullptr;
    voxo_t*          voxo_ = nullptr;
    GLFWwindow*      window_ = nullptr;
    void*            layer_ = nullptr;
    PrintLedger*     ledger_ = nullptr;
    uint32_t         backend_ = 0;
    std::string      app_;
    sumi_replay_rec_t* rec_ = nullptr;
    sumi_replay_t*     play_ = nullptr;
    double           acc_ = 0.0;
    bool             play_done_ = false;
    std::string      status_;
};
