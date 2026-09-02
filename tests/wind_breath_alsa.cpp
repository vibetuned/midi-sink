// wind_breath_alsa.cpp — Linux/ALSA port of tests/wind_breath.swift (step-6
// DONE check, re-run for step 12): a legato melody on ONE channel with a
// dense CC2 breath stream (~150 msgs/s), preceded by a CC2 burst so §2.5
// auto-detection settles on wind mode before the first note. Absolute-clock
// pacing (like mpe_stress_alsa) instead of the Swift original's relative
// sleeps.
//
// Run while midi-sink is open:  wind_breath_alsa [seconds]   (default 20)
// Expects: one continuous wandering ink line whose thickness follows breath.
#include <alsa/asoundlib.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

static snd_seq_t* g_seq = nullptr;
static int g_port = -1;
static snd_midi_event_t* g_enc = nullptr;
static long g_sent = 0;

static void send(uint8_t status, uint8_t d1, uint8_t d2) {
    const uint8_t msg[3] = {status, d1, d2};
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    if (snd_midi_event_encode(g_enc, msg, 3, &ev) < 3 ||
        ev.type == SND_SEQ_EVENT_NONE) {
        return;
    }
    snd_seq_ev_set_source(&ev, g_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output_direct(g_seq, &ev);
    g_sent++;
}

int main(int argc, char** argv) {
    const double duration = argc > 1 ? atof(argv[1]) : 20.0;

    if (snd_seq_open(&g_seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        fprintf(stderr, "snd_seq_open failed\n");
        return 1;
    }
    snd_seq_set_client_name(g_seq, "wind-breath");
    g_port = snd_seq_create_simple_port(
        g_seq, "wind-breath-out",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (g_port < 0 || snd_midi_event_new(16, &g_enc) < 0) {
        fprintf(stderr, "seq port/encoder creation failed\n");
        return 1;
    }
    printf("feeding client %d port %d (\"wind-breath\") for %.0fs\n",
           snd_seq_client_id(g_seq), g_port, duration);

    using clock = std::chrono::steady_clock;
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));   // let the 1 Hz rescan open us

    for (int i = 0; i < 20; i++) send(0xB0, 2, 0);   // breath prelude: settles wind detection

    // A wandering legato line (overlapping note-ons before offs, one channel).
    static const uint8_t melody[15] = {55, 57, 60, 62, 64, 67, 64, 69, 67, 72, 70, 65, 62, 58, 60};
    int idx = 0;
    uint8_t current = melody[0];
    send(0x90, current, 80);

    long tick = 0;
    const auto start = clock::now();
    auto next = start;
    while (std::chrono::duration<double>(clock::now() - start).count() < duration) {
        const double t = std::chrono::duration<double>(clock::now() - start).count();
        // ~150 Hz breath: slow swells with vibrato-ish ripple.
        double b = 70.0 + 50.0 * sin(t * 0.9) + 8.0 * sin(t * 7.0);
        if (b < 2.0) b = 2.0;
        if (b > 127.0) b = 127.0;
        send(0xB0, 2, (uint8_t)b);
        // Legato note change every ~1.2 s: new note ON first, old note OFF after.
        if (tick % 180 == 179) {
            idx++;
            const uint8_t nn = melody[idx % 15];
            send(0x90, nn, 80);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send(0x80, current, 40);
            current = nn;
        }
        tick++;
        next += std::chrono::microseconds(6600);
        std::this_thread::sleep_until(next);
    }
    send(0x80, current, 40);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    snd_midi_event_free(g_enc);
    snd_seq_close(g_seq);
    printf("wind_breath_alsa: %ld messages in %.1fs\n", g_sent,
           std::chrono::duration<double>(clock::now() - start).count());
    return 0;
}
