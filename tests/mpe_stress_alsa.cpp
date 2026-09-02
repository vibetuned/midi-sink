// mpe_stress_alsa.cpp — Linux/ALSA port of tests/osmose_stress.swift (same
// schedule as tests/mpe_stress_win.cpp; step-12 handoff). Scripted
// Osmose-density MPE stress source: 10 voices (member channels 2..11) x 200
// channel-pressure messages per second each (= 2000 press/s), plus per-voice
// glide bends and CC74, after an MCM configuring the lower zone.
//
// Unlike Windows, ALSA has virtual ports built in: this tool creates an
// snd_seq source port ("mpe-stress"); midi-sink's libremidi/ALSA backend sees
// it as an input (the 1 Hz rescan + track_virtual open it automatically).
//
// Build: part of the CMake tests on Linux.
// Run while midi-sink is open:  mpe_stress_alsa [seconds]   (default 30)
// Expects: 60+ fps, `dropped MIDI messages: 0` at app exit.
#include <alsa/asoundlib.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

static snd_seq_t* g_seq = nullptr;
static int g_port = -1;
static snd_midi_event_t* g_enc = nullptr;
static long g_sent = 0;

// Raw MIDI bytes -> seq event -> all subscribers, immediately (no queue).
static void send_bytes(const uint8_t* bytes, int n) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    if (snd_midi_event_encode(g_enc, bytes, n, &ev) < n ||
        ev.type == SND_SEQ_EVENT_NONE) {
        return;
    }
    snd_seq_ev_set_source(&ev, g_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output_direct(g_seq, &ev);
    g_sent++;
}

static void send(uint8_t status, uint8_t d1, uint8_t d2) {
    const uint8_t kind = status & 0xF0;
    const uint8_t msg[3] = {status, d1, d2};
    // Channel pressure and program change are 2-byte messages.
    send_bytes(msg, (kind == 0xD0 || kind == 0xC0) ? 2 : 3);
}

static uint8_t clamp7(double v) {
    if (v < 0.0) return 0;
    if (v > 127.0) return 127;
    return (uint8_t)v;
}

int main(int argc, char** argv) {
    const double duration = argc > 1 ? atof(argv[1]) : 30.0;

    if (snd_seq_open(&g_seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        fprintf(stderr, "snd_seq_open failed\n");
        return 1;
    }
    snd_seq_set_client_name(g_seq, "mpe-stress");
    g_port = snd_seq_create_simple_port(
        g_seq, "mpe-stress-out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (g_port < 0 || snd_midi_event_new(16, &g_enc) < 0) {
        fprintf(stderr, "seq port/encoder creation failed\n");
        return 1;
    }
    printf("feeding client %d port %d (\"mpe-stress\") for %.0fs\n",
           snd_seq_client_id(g_seq), g_port, duration);

    using clock = std::chrono::steady_clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));   // let the 1 Hz rescan open us

    // MCM: lower zone, 15 member channels (RPN 6 on ch 1).
    send(0xB0, 101, 0);
    send(0xB0, 100, 6);
    send(0xB0, 6, 15);

    // 10 held voices on member channels 2..11 (indices 1..10), spread pitches.
    static const uint8_t notes[10] = {48, 55, 60, 64, 67, 72, 76, 79, 84, 91};
    for (int i = 0; i < 10; i++) {
        send((uint8_t)(0x90 | (1 + i)), notes[i], (uint8_t)(60 + i * 6));
    }

    // 200 Hz per voice: pressure sine (dense, Osmose-like), plus 20 Hz bend
    // and 10 Hz CC74 per voice — the exact schedule of osmose_stress.swift.
    long tick = 0;
    const auto start = clock::now();
    auto next = start;
    for (;;) {
        const double t = std::chrono::duration<double>(clock::now() - start).count();
        if (t >= duration) break;
        for (int v = 0; v < 10; v++) {
            const uint8_t ch = (uint8_t)(1 + v);
            const uint8_t press = clamp7(64.0 + 60.0 * sin(t * (1.1 + 0.13 * v) + v));
            send((uint8_t)(0xD0 | ch), press, 0);
            if (tick % 10 == v) {   // staggered 20 Hz bends
                const int bend = (int)(8192.0 + 2000.0 * sin(t * 0.7 + v * 0.9));
                send((uint8_t)(0xE0 | ch), (uint8_t)(bend & 0x7F), (uint8_t)((bend >> 7) & 0x7F));
            }
            if (tick % 20 == 2 * v) {   // staggered 10 Hz CC74
                const uint8_t slide = clamp7(64.0 + 60.0 * sin(t * 0.5 + v));
                send((uint8_t)(0xB0 | ch), 74, slide);
            }
        }
        tick++;
        next += std::chrono::milliseconds(5);   // absolute pacing: no drift
        std::this_thread::sleep_until(next);
    }
    for (int i = 0; i < 10; i++) {
        send((uint8_t)(0x80 | (1 + i)), notes[i], 64);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    snd_midi_event_free(g_enc);
    snd_seq_close(g_seq);
    const double total = std::chrono::duration<double>(clock::now() - start).count();
    printf("mpe_stress_alsa: %ld messages in %.1fs\n", g_sent, total);
    return 0;
}
