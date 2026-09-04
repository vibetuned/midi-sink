// midi_capture_alsa.cpp — Linux ALSA-sequencer capture for the Step-22 evidence
// (the Linux box is the "DAW" end of the tablet's transports). Subscribes to
// every readable sequencer port whose client or port name contains one of the
// --match substrings (or every non-system port with --all), and logs each
// channel message as CSV with a CLOCK_MONOTONIC receive timestamp:
//
//     t_s,port,port_name,status,d1,d2
//
// One clock for every transport: the USB gadget port and the BLE-MIDI port
// (BlueZ's MIDI GATT profile / PipeWire's bluez-midi monitor) are captured in
// the same file, so arrival-time DIFFERENCES between transports are
// meaningful without any device clock sync — that is how "USB beats BLE" is
// asserted. The capture also subscribes to the System Announce port and
// connects to matching ports that appear MID-CAPTURE (the USB mode flip test:
// the tablet's port appears while the recording runs).
//
// Usage: midi_capture_alsa [--seconds N] [--out file.csv] [--match substr]... [--all]
//        (default: 60 s, stdout, --all)
// Build: part of the CMake tests on Linux (tests/CMakeLists.txt).
#include <alsa/asoundlib.h>
#include <poll.h>
#include <time.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

static snd_seq_t* g_seq = nullptr;
static int g_port = -1;
static snd_midi_event_t* g_dec = nullptr;
static std::vector<std::string> g_match;
static bool g_all = true;
static std::vector<std::pair<int, int>> g_connected;   // client:port already subscribed
static std::map<std::pair<int, int>, std::string> g_names;   // -> "Client Port", commas stripped

static double mono_s() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static bool wanted(const char* client_name, const char* port_name) {
    if (g_all) return true;
    for (const auto& m : g_match) {
        if (strstr(client_name, m.c_str()) || strstr(port_name, m.c_str())) return true;
    }
    return false;
}

static void try_connect(int client, int port) {
    if (client == snd_seq_client_id(g_seq)) return;     // never ourselves
    if (client == SND_SEQ_CLIENT_SYSTEM) return;
    for (const auto& c : g_connected) if (c.first == client && c.second == port) return;
    snd_seq_port_info_t* pinfo;
    snd_seq_port_info_alloca(&pinfo);
    if (snd_seq_get_any_port_info(g_seq, client, port, pinfo) < 0) return;
    const unsigned caps = snd_seq_port_info_get_capability(pinfo);
    if (!(caps & SND_SEQ_PORT_CAP_READ) || !(caps & SND_SEQ_PORT_CAP_SUBS_READ)) return;
    if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) return;
    snd_seq_client_info_t* cinfo;
    snd_seq_client_info_alloca(&cinfo);
    const char* cname = "";
    if (snd_seq_get_any_client_info(g_seq, client, cinfo) >= 0) cname = snd_seq_client_info_get_name(cinfo);
    const char* pname = snd_seq_port_info_get_name(pinfo);
    if (!wanted(cname, pname)) return;
    if (snd_seq_connect_from(g_seq, g_port, client, port) < 0) {
        fprintf(stderr, "# connect %d:%d (%s / %s) failed\n", client, port, cname, pname);
        return;
    }
    g_connected.push_back({client, port});
    std::string label = std::string(cname) + " " + pname;
    for (char& c : label) if (c == ',' || c == '\n') c = ' ';
    g_names[{client, port}] = label;
    fprintf(stderr, "# t=%.3f connected %d:%d  %s / %s\n", mono_s(), client, port, cname, pname);
}

static void scan_all() {
    snd_seq_client_info_t* cinfo;
    snd_seq_port_info_t* pinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(g_seq, cinfo) >= 0) {
        const int client = snd_seq_client_info_get_client(cinfo);
        snd_seq_port_info_set_client(pinfo, client);
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(g_seq, pinfo) >= 0) {
            try_connect(client, snd_seq_port_info_get_port(pinfo));
        }
    }
}

int main(int argc, char** argv) {
    double seconds = 60.0;
    const char* out_path = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atof(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out_path = argv[++i];
        else if (!strcmp(argv[i], "--match") && i + 1 < argc) { g_match.push_back(argv[++i]); g_all = false; }
        else if (!strcmp(argv[i], "--all")) g_all = true;
        else { fprintf(stderr, "usage: %s [--seconds N] [--out f.csv] [--match s]... [--all]\n", argv[0]); return 2; }
    }
    FILE* out = out_path ? fopen(out_path, "w") : stdout;
    if (!out) { perror("fopen"); return 1; }

    if (snd_seq_open(&g_seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "snd_seq_open failed\n");
        return 1;
    }
    snd_seq_set_client_name(g_seq, "midi-capture");
    g_port = snd_seq_create_simple_port(g_seq, "capture",
                                        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (g_port < 0) { fprintf(stderr, "create port failed\n"); return 1; }
    snd_midi_event_new(64, &g_dec);
    snd_midi_event_no_status(g_dec, 1);   // always full status bytes

    // System Announce: ports appearing mid-capture get connected too.
    snd_seq_connect_from(g_seq, g_port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    scan_all();

    fprintf(out, "t_s,port,port_name,status,d1,d2\n");
    fflush(out);
    const double t_end = mono_s() + seconds;
    const int npfd = snd_seq_poll_descriptors_count(g_seq, POLLIN);
    std::vector<pollfd> pfds((size_t)npfd);
    snd_seq_poll_descriptors(g_seq, pfds.data(), (unsigned)npfd, POLLIN);
    long logged = 0;
    while (mono_s() < t_end) {
        const int r = poll(pfds.data(), (nfds_t)npfd, 200);
        if (r <= 0) continue;
        snd_seq_event_t* ev = nullptr;
        while (snd_seq_event_input(g_seq, &ev) >= 0 && ev) {
            const double t = mono_s();
            if (ev->type == SND_SEQ_EVENT_PORT_START) {
                try_connect(ev->data.addr.client, ev->data.addr.port);
            } else if (ev->type == SND_SEQ_EVENT_PORT_EXIT) {
                for (size_t i = 0; i < g_connected.size(); i++) {
                    if (g_connected[i].first == ev->data.addr.client &&
                        g_connected[i].second == ev->data.addr.port) {
                        fprintf(stderr, "# t=%.3f port %d:%d left\n", t, ev->data.addr.client, ev->data.addr.port);
                        g_connected.erase(g_connected.begin() + (long)i);
                        break;
                    }
                }
            } else if (ev->type != SND_SEQ_EVENT_CLIENT_START && ev->type != SND_SEQ_EVENT_CLIENT_EXIT &&
                       ev->type != SND_SEQ_EVENT_CLIENT_CHANGE && ev->type != SND_SEQ_EVENT_PORT_CHANGE &&
                       ev->type != SND_SEQ_EVENT_PORT_SUBSCRIBED && ev->type != SND_SEQ_EVENT_PORT_UNSUBSCRIBED) {
                unsigned char buf[16];
                const long n = snd_midi_event_decode(g_dec, buf, sizeof(buf), ev);
                if (n >= 2 && buf[0] >= 0x80 && buf[0] < 0xF0) {
                    const auto it = g_names.find({ev->source.client, ev->source.port});
                    fprintf(out, "%.6f,%d:%d,%s,%d,%d,%d\n", t, ev->source.client, ev->source.port,
                            it != g_names.end() ? it->second.c_str() : "?",
                            buf[0], buf[1], n >= 3 ? buf[2] : 0);
                    logged++;
                }
            }
            snd_seq_free_event(ev);
            if (snd_seq_event_input_pending(g_seq, 0) <= 0) break;
        }
        fflush(out);
    }
    fprintf(stderr, "# captured %ld messages from %zu port(s)\n", logged, g_connected.size());
    if (out != stdout) fclose(out);
    snd_midi_event_free(g_dec);
    snd_seq_close(g_seq);
    return 0;
}
