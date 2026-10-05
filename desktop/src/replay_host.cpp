// replay_host.cpp — Phase 9 step 65 (DECISIONS_8 #22–#23): see the header.
#include "replay_host.h"

#include "app_settings.h"
#include "midi_harness.h"
#include "print_ledger.h"
#include "sys_info.h"
#include "voxo.h"
#if defined(__APPLE__)
#include "metal_layer_glue.h"
#endif

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

static const char* platform_name() {
#if defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "linux";
#endif
}
static const char* backend_name(uint32_t b) {
    return b == SUMI_BACKEND_GL ? "gl" : b == SUMI_BACKEND_D3D11 ? "d3d11" : b == SUMI_BACKEND_METAL ? "metal" : "?";
}

void ReplayHost::init(sumi_instance_t* inst, void* midi, voxo_t* voxo, GLFWwindow* window, void* metal_layer,
                      uint32_t backend, const char* app_version) {
    inst_ = inst; midi_ = midi; voxo_ = voxo; window_ = window; layer_ = metal_layer; backend_ = backend;
    app_ = app_version ? app_version : "";
}

void ReplayHost::shutdown(const AppSettings& s) {
    if (rec_) { std::string p; stop_recording(&p); }
    if (play_) stop_playback(s);
}

// ---- recording ---------------------------------------------------------------

void ReplayHost::push_core(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t /*src*/) {
    sumi_push_midi((sumi_instance_t*)user, s, d1, d2);
}
void ReplayHost::stage_cb(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t src) {
    ReplayHost* h = (ReplayHost*)user;
    if (h->rec_) sumi_replay_rec_midi(h->rec_, glfwGetTime(), s, d1, d2, src);   // any thread; the harness serialises
}

bool ReplayHost::start_recording(const AppSettings& s) {
    if (!inst_ || rec_ || play_) return false;
    sumi_replay_info_t info; std::memset(&info, 0, sizeof info);
    std::snprintf(info.platform, sizeof info.platform, "%s", platform_name());
    std::snprintf(info.backend, sizeof info.backend, "%s", backend_name(backend_));
    std::snprintf(info.device, sizeof info.device, "%s", sys_machine_name().c_str());
    std::snprintf(info.app, sizeof info.app, "%s", app_.c_str());
    info.sumi_version = sumi_version();
    sumi_replay_timestamp(info.recorded, sizeof info.recorded);
    int fw = 0, fh = 0; float xs = 1.0f, ys = 1.0f;
    if (window_) { glfwGetFramebufferSize(window_, &fw, &fh); glfwGetWindowContentScale(window_, &xs, &ys); }
    info.width = fw > 0 ? (uint32_t)fw : 0u; info.height = fh > 0 ? (uint32_t)fh : 0u; info.pixel_ratio = xs > 0.0f ? xs : 1.0f;
    const std::string session = app_settings_session_json(s);
    rec_ = sumi_replay_rec_create(&info, session.c_str(), session.size());
    if (!rec_) return false;
    // from here the harness stages: the render thread is the core's producer until the stop
    if (midi_) sumi_midi_harness_set_stage(midi_, stage_cb, this);
    // the sheet as it stands is kept, then dipped: a recording starts on fresh paper (the dip is its first event)
    if (ledger_) ledger_->dip(inst_, s); else { sumi_trigger_paper_dip(inst_); on_dip(); }
    // the routed controls' values ride the harness as their CCs (staged, so frame 0 carries them)
    app_settings_apply(s, inst_, midi_);
    status_ = "Recording";
    return true;
}

bool ReplayHost::stop_recording(std::string* path_out) {
    if (!rec_) return false;
    if (midi_) sumi_midi_harness_set_stage(midi_, nullptr, nullptr);   // no byte is staged after this returns
    sumi_replay_rec_flush(rec_, push_core, inst_);                      // what was in flight goes to the core, unrecorded
    char stamp[32]; sumi_replay_timestamp(stamp, sizeof stamp);
    std::string name = stamp; for (char& c : name) if (c == ':') c = '-';
    const std::string path = replays_dir() + "/" + name + SUMI_REPLAY_EXT;
    const bool ok = sumi_replay_rec_save(rec_, path.c_str());
    const uint32_t frames = sumi_replay_rec_frames(rec_), dropped = sumi_replay_rec_dropped(rec_);
    const double seconds = sumi_replay_rec_seconds(rec_);
    sumi_replay_rec_destroy(rec_); rec_ = nullptr;
    char line[1400];
    if (ok) std::snprintf(line, sizeof line, "Saved %s (%u frames, %.1f s%s)", path.c_str(), frames, seconds, dropped ? ", some bytes dropped" : "");
    else std::snprintf(line, sizeof line, "Could not write %s", path.c_str());
    status_ = line;
    std::printf("[replay] %s\n", line);
    if (path_out) *path_out = ok ? path : "";
    return ok;
}

void ReplayHost::frame_begin(double now, double dt) {
    if (rec_) sumi_replay_rec_frame(rec_, now, dt, push_core, inst_);
}
void ReplayHost::on_settings_applied(const AppSettings& s) {
    if (!rec_) return;
    const std::string json = app_settings_session_json(s);
    sumi_replay_rec_state(rec_, json.c_str(), json.size());
}
void ReplayHost::on_resize(uint32_t w, uint32_t h, float ratio) { if (rec_) sumi_replay_rec_resize(rec_, w, h, ratio); }
void ReplayHost::on_dip() { if (rec_) sumi_replay_rec_dip(rec_); }
void ReplayHost::on_gesture(uint32_t kind, const float* args, uint32_t n) { if (rec_) sumi_replay_rec_gesture(rec_, kind, args, n); }

void ReplayHost::tap(float x, float y, float r) {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_gesture_tap(inst_, x, y, r);
    const float a[3] = { x, y, r }; on_gesture(SUMI_REPLAY_G_TAP, a, 3);
}
void ReplayHost::pinch(float x, float y, float k, float angle, float span) {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_gesture_pinch(inst_, x, y, k, angle, span);
    const float a[5] = { x, y, k, angle, span }; on_gesture(SUMI_REPLAY_G_PINCH, a, 5);
}
void ReplayHost::twist(float x, float y, float strength, float radius, uint32_t profile) {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_gesture_twist(inst_, x, y, strength, radius, profile);
    const float a[5] = { x, y, strength, radius, (float)profile }; on_gesture(SUMI_REPLAY_G_TWIST, a, 5);
}
float ReplayHost::press(float x, float y, float R, float up, float down, double dt) {
    if (!inst_ || play_) return R;   // a replay plays: the viewer watches
    const float a[6] = { x, y, R, up, down, (float)dt }; on_gesture(SUMI_REPLAY_G_PRESS, a, 6);   // the inputs, as the core gets them
    return sumi_gesture_press(inst_, x, y, R, up, down, (double)(float)dt);                      // the replay passes dt as a float: so do we
}
void ReplayHost::press_end() {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_gesture_press_end(inst_);
    on_gesture(SUMI_REPLAY_G_PRESS_END, nullptr, 0);
}
void ReplayHost::tine(float x0, float y0, float x1, float y1, float alpha, float magnitude) {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_add_tine(inst_, x0, y0, x1, y1, alpha, magnitude);
    const float a[6] = { x0, y0, x1, y1, alpha, magnitude }; on_gesture(SUMI_REPLAY_G_TINE, a, 6);
}
void ReplayHost::wake(float x0, float y0, float x1, float y1, float tip) {
    if (!inst_ || play_) return;   // a replay plays: the viewer watches
    sumi_add_wake(inst_, x0, y0, x1, y1, tip);
    const float a[5] = { x0, y0, x1, y1, tip }; on_gesture(SUMI_REPLAY_G_WAKE, a, 5);
}
uint32_t ReplayHost::rec_frames() const  { return sumi_replay_rec_frames(rec_); }
double   ReplayHost::rec_seconds() const { return sumi_replay_rec_seconds(rec_); }
bool     ReplayHost::rec_full() const    { return sumi_replay_rec_full(rec_); }

// ---- playback ----------------------------------------------------------------

void ReplayHost::push_core_and_sound(void* user, uint8_t s, uint8_t d1, uint8_t d2, uint8_t /*src*/) {
    ReplayHost* h = (ReplayHost*)user;
    sumi_push_midi(h->inst_, s, d1, d2);
    if (h->voxo_) voxo_push_midi(h->voxo_, s, d1, d2);   // the replay re-sounds: the render thread is Voxo's producer while the harness is muted
}

void ReplayHost::set_display_sync(bool on) {
#if defined(__APPLE__)
    if (layer_) sumi_macos_set_display_sync(layer_, on ? 1 : 0);
#else
    (void)on;
#endif
}
void ReplayHost::set_title(const std::string& t) { if (window_) glfwSetWindowTitle(window_, t.c_str()); }

bool ReplayHost::play(const std::string& path, std::string* why) {
    if (!inst_ || rec_) { if (why) *why = rec_ ? "a recording is running" : "no instance"; return false; }
    if (play_) { sumi_replay_close(play_); play_ = nullptr; }
    sumi_replay_t* r = sumi_replay_load(path.c_str());
    if (!r) { if (why) *why = "not a replay file (or truncated): " + path; status_ = "Could not open " + path; return false; }
    play_ = r;
    if (midi_) sumi_midi_harness_set_muted(midi_, true);   // the replay owns the loopback and the sound
    sumi_replay_begin(play_, inst_, 0u);                     // the viewer's size and palette stay: the replay re-dips at theirs
    acc_ = 0.0; play_done_ = false;
    set_display_sync(false);                                 // several recorded frames may render per display frame
    set_title("midi-sink — " + banner());
    status_ = banner();
    std::printf("[replay] %s (%u frames, %.1f s)\n", banner().c_str(), play_frames(), play_duration());
    return true;
}

void ReplayHost::stop_playback(const AppSettings& s) {
    if (!play_) return;
    sumi_replay_close(play_); play_ = nullptr;
    set_display_sync(true);
    set_title("midi-sink");
    if (midi_) sumi_midi_harness_set_muted(midi_, false);
    app_settings_apply(s, inst_, midi_);   // the viewer's settings back (the replayed field stays on the sheet: dip and print it at this size)
    status_ = play_done_ ? "Replay finished" : "Replay stopped";
}

int ReplayHost::playback_frame(double dt_display) {
    if (!play_ || !inst_) return 0;
    acc_ += dt_display;
    if (acc_ > 0.25) acc_ = 0.25;   // never more than a quarter second behind: the sound follows the frames
    int n = 0;
    while (n < 8 && acc_ > 0.0) {
        double dt = 0.0;
        if (!sumi_replay_step(play_, inst_, 0u, push_core_and_sound, this, &dt)) { play_done_ = true; break; }
        sumi_update(inst_, dt);
        sumi_render(inst_);
        acc_ -= dt;
        n++;
    }
    if (sumi_replay_peek_dt(play_) <= 0.0) play_done_ = true;
    return n;
}

std::string ReplayHost::banner() const {
    if (!play_) return "";
    const sumi_replay_info_t* i = sumi_replay_info(play_);
    char line[256];
    std::snprintf(line, sizeof line, "Replaying %s (%s, %s) · midi-sink %s · %s",
                  i->device[0] ? i->device : "?", i->platform[0] ? i->platform : "?", i->backend[0] ? i->backend : "?",
                  i->app[0] ? i->app : "?", i->recorded[0] ? i->recorded : "");
    return line;
}
double   ReplayHost::play_elapsed() const  { return sumi_replay_elapsed(play_); }
double   ReplayHost::play_duration() const { return sumi_replay_duration(play_); }
uint32_t ReplayHost::play_position() const { return sumi_replay_position(play_); }
uint32_t ReplayHost::play_frames() const   { return sumi_replay_frame_count(play_); }

// ---- files -------------------------------------------------------------------

std::string ReplayHost::replays_dir() { return app_replays_dir(); }
std::string ReplayHost::replay_path(const std::string& name) { return app_replays_dir() + "/" + name; }
std::vector<std::string> ReplayHost::replay_names() {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(app_replays_dir(), ec)) {
        if (!e.is_regular_file(ec)) continue;
        const std::string n = e.path().filename().string();
        if (n.size() > std::strlen(SUMI_REPLAY_EXT) && n.compare(n.size() - std::strlen(SUMI_REPLAY_EXT), std::string::npos, SUMI_REPLAY_EXT) == 0) names.push_back(n);
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return a > b; });   // the stamp sorts: newest first
    return names;
}
