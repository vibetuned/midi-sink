// sumi_rtpmidi.cpp — see sumi_rtpmidi.h. RFC 6295's payload with Apple's session protocol (the control and
// data ports, 'IN' / 'OK' / 'NO' / 'BY' / 'CK' / 'RS' prefixed 0xFFFF), POSIX UDP, no threads.
#include "sumi_rtpmidi.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <new>

namespace {

constexpr uint16_t CMD_IN = 0x494E, CMD_OK = 0x4F4B, CMD_NO = 0x4E4F, CMD_BY = 0x4259, CMD_CK = 0x434B, CMD_RS = 0x5253;
constexpr uint32_t PROTOCOL_VERSION = 2;
constexpr double   PARTICIPANT_TIMEOUT_S = 60.0;
constexpr double   INVITE_REPEAT_S = 1.0;
constexpr int      INVITE_TRIES = 10;
constexpr double   SYNC_PERIOD_S = 10.0;
constexpr size_t   MAX_DATAGRAM = 1472;

enum State : uint8_t {
    FREE = 0,
    LISTENER_CONTROL_INVITED,   // we answered 'OK' on the control port; the data port's invitation is due
    INITIATOR_CONTROL_SENT,     // our 'IN' on the control port is out
    INITIATOR_DATA_SENT,        // theirs 'OK' came; our 'IN' on the data port is out
    ESTABLISHED,
};

struct Participant {
    State    state = FREE;
    bool     initiator = false;      // we invited them
    uint32_t ssrc = 0;
    uint32_t token = 0;
    char     name[SUMI_RTPMIDI_NAME_MAX] = {};
    sockaddr_in control{}, data{};
    double   last_rx = 0.0, last_invite = 0.0, last_sync = 0.0;
    int      invites = 0;
    int64_t  clock_offset = 0;       // their 100 µs clock minus ours (the initiator's estimate)
};

uint64_t ticks(double now_s) { return (uint64_t)(now_s * 10000.0); }   // the 100 µs clock

void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
void put64(uint8_t* p, uint64_t v) { put32(p, (uint32_t)(v >> 32)); put32(p + 4, (uint32_t)v); }
uint16_t get16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
uint32_t get32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
uint64_t get64(const uint8_t* p) { return ((uint64_t)get32(p) << 32) | get32(p + 4); }

// A MIDI 1.0 message's length from its status; 0 for SysEx and unknown bytes.
size_t midi_len(uint8_t status) {
    if (status < 0x80) return 0;
    if (status < 0xC0) return 3;
    if (status < 0xE0) return 2;
    if (status < 0xF0) return 3;
    switch (status) {
        case 0xF1: case 0xF3: return 2;
        case 0xF2: return 3;
        case 0xF6: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFE: case 0xFF: return 1;
        default: return 0;   // 0xF0 (SysEx), 0xF7, 0xF4, 0xF5, 0xFD
    }
}

} // namespace

struct sumi_rtpmidi_t {
    int      control_fd = -1, data_fd = -1;
    uint16_t port = 0;
    uint32_t ssrc = 0;
    uint16_t seq = 0;
    uint32_t rng = 0;
    char     name[SUMI_RTPMIDI_NAME_MAX] = {};
    Participant parts[SUMI_RTPMIDI_MAX_PARTICIPANTS];
    sumi_rtpmidi_stats_t st{};

    uint32_t random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

    Participant* by_ssrc(uint32_t s) { for (Participant& p : parts) if (p.state != FREE && p.ssrc == s) return &p; return nullptr; }
    Participant* by_token(uint32_t t) { for (Participant& p : parts) if (p.state != FREE && p.token == t) return &p; return nullptr; }
    // The data port's sender by address — an implementation that stamps its data-port packets with a second SSRC
    // (pymidi gives each port its own) is still the participant whose data address this is.
    Participant* by_data_addr(const sockaddr_in& a) {
        for (Participant& p : parts) if (p.state != FREE && p.data.sin_addr.s_addr == a.sin_addr.s_addr && p.data.sin_port == a.sin_port) return &p;
        return nullptr;
    }
    Participant* free_slot() { for (Participant& p : parts) if (p.state == FREE) return &p; return nullptr; }
    uint32_t established() const { uint32_t n = 0; for (const Participant& p : parts) if (p.state == ESTABLISHED) n++; return n; }

    void send_to(int fd, const sockaddr_in& to, const uint8_t* b, size_t n) {
        (void)::sendto(fd, b, n, 0, (const sockaddr*)&to, sizeof to);
    }
    // 'IN' / 'OK' / 'NO' (with our name) and 'BY' (without), on either port
    void send_session(int fd, const sockaddr_in& to, uint16_t cmd, uint32_t token) {
        uint8_t b[16 + SUMI_RTPMIDI_NAME_MAX];
        put16(b, 0xFFFF); put16(b + 2, cmd); put32(b + 4, PROTOCOL_VERSION); put32(b + 8, token); put32(b + 12, ssrc);
        size_t n = 16;
        if (cmd != CMD_BY) { const size_t ln = std::strlen(name); std::memcpy(b + 16, name, ln + 1); n += ln + 1; }
        send_to(fd, to, b, n);
    }
    void send_ck(const sockaddr_in& to, uint8_t count, uint64_t t1, uint64_t t2, uint64_t t3) {
        uint8_t b[36];
        put16(b, 0xFFFF); put16(b + 2, CMD_CK); put32(b + 4, ssrc); b[8] = count; b[9] = b[10] = b[11] = 0;
        put64(b + 12, t1); put64(b + 20, t2); put64(b + 28, t3);
        send_to(data_fd, to, b, sizeof b);
    }
    void drop(Participant& p, bool bye, double now) {
        if (bye && p.state != FREE) { send_session(control_fd, p.control, CMD_BY, p.token); }
        if (p.state == ESTABLISHED) st.left++;
        p = Participant{};
        (void)now;
    }

    void on_control(const uint8_t* b, size_t n, const sockaddr_in& from, double now) {
        if (n < 4 || get16(b) != 0xFFFF) { st.rejected++; return; }
        const uint16_t cmd = get16(b + 2);
        if (cmd == CMD_IN && n >= 16) {
            const uint32_t token = get32(b + 8), their = get32(b + 12);
            Participant* p = by_ssrc(their);
            if (!p) { p = free_slot(); if (!p) { send_session(control_fd, from, CMD_NO, token); st.rejected++; return; } *p = Participant{}; }
            p->state = p->state == ESTABLISHED ? ESTABLISHED : LISTENER_CONTROL_INVITED;
            p->initiator = false; p->ssrc = their; p->token = token; p->control = from; p->last_rx = now;
            p->data = from; p->data.sin_port = htons((uint16_t)(ntohs(from.sin_port) + 1));
            const size_t ln = n > 16 ? n - 16 : 0;
            size_t k = 0; while (k < ln && k < SUMI_RTPMIDI_NAME_MAX - 1 && b[16 + k]) { p->name[k] = (char)b[16 + k]; k++; }
            p->name[k] = 0;
            send_session(control_fd, from, CMD_OK, token);
        } else if (cmd == CMD_OK && n >= 16) {
            Participant* p = by_token(get32(b + 8));
            if (!p || !p->initiator || p->state != INITIATOR_CONTROL_SENT) return;
            p->ssrc = get32(b + 12); p->last_rx = now;
            const size_t ln = n > 16 ? n - 16 : 0;
            size_t k = 0; while (k < ln && k < SUMI_RTPMIDI_NAME_MAX - 1 && b[16 + k]) { p->name[k] = (char)b[16 + k]; k++; }
            p->name[k] = 0;
            p->state = INITIATOR_DATA_SENT; p->invites = 0; p->last_invite = now;
            send_session(data_fd, p->data, CMD_IN, p->token);
        } else if (cmd == CMD_NO && n >= 16) {
            Participant* p = by_token(get32(b + 8));
            if (p) { st.rejected++; drop(*p, false, now); }
        } else if (cmd == CMD_BY && n >= 16) {
            Participant* p = by_ssrc(get32(b + 12));
            if (p) drop(*p, false, now);
        } else if (cmd == CMD_RS || cmd == CMD_CK) {
            Participant* p = by_ssrc(get32(b + 4)); if (p) p->last_rx = now;   // feedback: noted, not needed
        } else st.rejected++;
    }

    void on_data(const uint8_t* b, size_t n, const sockaddr_in& from, double now, sumi_rtpmidi_midi_fn on_midi, void* user) {
        if (n >= 4 && get16(b) == 0xFFFF) {
            const uint16_t cmd = get16(b + 2);
            if (cmd == CMD_IN && n >= 16) {
                Participant* p = by_ssrc(get32(b + 12));
                if (!p || p->initiator) { send_session(data_fd, from, CMD_NO, get32(b + 8)); st.rejected++; return; }
                p->data = from; p->token = get32(b + 8); p->last_rx = now;
                send_session(data_fd, from, CMD_OK, p->token);
                if (p->state != ESTABLISHED) { p->state = ESTABLISHED; st.joined++; }
            } else if (cmd == CMD_OK && n >= 16) {
                Participant* p = by_token(get32(b + 8));
                if (!p || !p->initiator || p->state != INITIATOR_DATA_SENT) return;
                p->last_rx = now; p->state = ESTABLISHED; st.joined++;
                p->last_sync = now; send_ck(p->data, 0, ticks(now), 0, 0);
            } else if (cmd == CMD_CK && n >= 36) {
                Participant* p = by_ssrc(get32(b + 4)); if (!p) p = by_data_addr(from);
                if (!p) { st.rejected++; return; }
                p->last_rx = now;
                const uint8_t count = b[8];
                const uint64_t t1 = get64(b + 12), t2 = get64(b + 20), t3 = get64(b + 28);
                if (count == 0) send_ck(p->data, 1, t1, ticks(now), 0);
                else if (count == 1) {
                    const uint64_t t3n = ticks(now);
                    send_ck(p->data, 2, t1, t2, t3n);
                    p->clock_offset = (int64_t)t2 - (int64_t)((t1 + t3n) / 2);
                    st.syncs++;
                } else if (count == 2) {
                    p->clock_offset = (int64_t)((t1 + t3) / 2) - (int64_t)t2;
                    st.syncs++;
                }
            } else if (cmd == CMD_BY && n >= 16) {
                Participant* p = by_ssrc(get32(b + 12));
                if (p) drop(*p, false, now);
            } else if (cmd == CMD_RS && n >= 8) {
                Participant* p = by_ssrc(get32(b + 4)); if (!p) p = by_data_addr(from); if (p) p->last_rx = now;   // receiver feedback on the data port: noted
            } else st.rejected++;
            return;
        }
        uint8_t midi[MAX_DATAGRAM]; uint16_t seq; uint32_t ts, their;
        const size_t m = sumi_rtpmidi_decode(b, n, midi, sizeof midi, &seq, &ts, &their);
        if (m == 0 && n > 12) { st.rejected++; return; }
        Participant* p = by_ssrc(their); if (!p) p = by_data_addr(from);
        if (!p || p->state != ESTABLISHED) { st.rejected++; return; }
        p->last_rx = now; st.packets_rx++;
        size_t i = 0;
        while (i < m) {
            const size_t len = midi_len(midi[i]);
            if (len == 0 || i + len > m) break;
            if (on_midi) on_midi(user, midi[i], len > 1 ? midi[i + 1] : 0, len > 2 ? midi[i + 2] : 0);
            st.midi_rx++; i += len;
        }
    }

    void housekeeping(double now) {
        for (Participant& p : parts) {
            if (p.state == FREE) continue;
            if (p.initiator && (p.state == INITIATOR_CONTROL_SENT || p.state == INITIATOR_DATA_SENT) && now - p.last_invite >= INVITE_REPEAT_S) {
                if (p.invites >= INVITE_TRIES) { drop(p, false, now); continue; }
                p.last_invite = now; p.invites++;
                send_session(p.state == INITIATOR_CONTROL_SENT ? control_fd : data_fd, p.state == INITIATOR_CONTROL_SENT ? p.control : p.data, CMD_IN, p.token);
            }
            if (p.state == ESTABLISHED && p.initiator && now - p.last_sync >= SYNC_PERIOD_S) { p.last_sync = now; send_ck(p.data, 0, ticks(now), 0, 0); }
            if (p.state != FREE && now - p.last_rx > PARTICIPANT_TIMEOUT_S) drop(p, true, now);
        }
    }
};

namespace {

int open_udp(uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    int one = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
    if (::bind(fd, (const sockaddr*)&a, sizeof a) != 0) { ::close(fd); return -1; }
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}
uint16_t bound_port(int fd) {
    sockaddr_in a{}; socklen_t l = sizeof a;
    if (::getsockname(fd, (sockaddr*)&a, &l) != 0) return 0;
    return ntohs(a.sin_port);
}
bool bind_pair(sumi_rtpmidi_t* s, uint16_t control) {
    const int c = open_udp(control);
    if (c < 0) return false;
    const uint16_t cp = control ? control : bound_port(c);
    if (cp == 0 || cp == 65535) { ::close(c); return false; }
    const int d = open_udp((uint16_t)(cp + 1));
    if (d < 0) { ::close(c); return false; }
    s->control_fd = c; s->data_fd = d; s->port = cp;
    return true;
}

} // namespace

extern "C" {

sumi_rtpmidi_t* sumi_rtpmidi_create(const char* name, uint16_t control_port, double now_s) {
    sumi_rtpmidi_t* s = new (std::nothrow) sumi_rtpmidi_t();
    if (!s) return nullptr;
    bool ok = control_port != 0 && bind_pair(s, control_port);
    if (!ok && control_port == 0) {
        for (uint16_t p = 5004; p <= 5100 && !ok; p = (uint16_t)(p + 2)) ok = bind_pair(s, p);
        for (int tries = 0; tries < 8 && !ok; tries++) ok = bind_pair(s, 0);
    }
    if (!ok) { delete s; return nullptr; }
    s->rng = (uint32_t)(ticks(now_s) ^ (uint64_t)::getpid() ^ (uint64_t)std::time(nullptr)) | 1u;
    for (int i = 0; i < 4; i++) s->random();
    s->ssrc = s->random(); s->seq = (uint16_t)s->random();
    if (name) { std::strncpy(s->name, name, SUMI_RTPMIDI_NAME_MAX - 1); s->name[SUMI_RTPMIDI_NAME_MAX - 1] = 0; }
    return s;
}

void sumi_rtpmidi_destroy(sumi_rtpmidi_t* s) {
    if (!s) return;
    if (s->control_fd >= 0) ::close(s->control_fd);
    if (s->data_fd >= 0) ::close(s->data_fd);
    delete s;
}

uint16_t sumi_rtpmidi_port(const sumi_rtpmidi_t* s) { return s ? s->port : 0; }

bool sumi_rtpmidi_connect(sumi_rtpmidi_t* s, const char* ipv4, uint16_t control_port, double now_s) {
    if (!s || !ipv4 || !control_port) return false;
    Participant* p = s->free_slot();
    if (!p) return false;
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(control_port);
    if (::inet_pton(AF_INET, ipv4, &a.sin_addr) != 1) return false;
    *p = Participant{};
    p->state = INITIATOR_CONTROL_SENT; p->initiator = true; p->token = s->random(); p->control = a;
    p->data = a; p->data.sin_port = htons((uint16_t)(control_port + 1));
    p->last_rx = now_s; p->last_invite = now_s; p->invites = 1;
    s->send_session(s->control_fd, p->control, CMD_IN, p->token);
    return true;
}

void sumi_rtpmidi_poll(sumi_rtpmidi_t* s, double now_s, sumi_rtpmidi_midi_fn on_midi, void* user) {
    if (!s) return;
    uint8_t buf[MAX_DATAGRAM];
    for (int which = 0; which < 2; which++) {
        const int fd = which == 0 ? s->control_fd : s->data_fd;
        for (int guard = 0; guard < 256; guard++) {
            sockaddr_in from{}; socklen_t fl = sizeof from;
            const ssize_t n = ::recvfrom(fd, buf, sizeof buf, 0, (sockaddr*)&from, &fl);
            if (n <= 0) break;
            if (which == 0) s->on_control(buf, (size_t)n, from, now_s);
            else s->on_data(buf, (size_t)n, from, now_s, on_midi, user);
        }
    }
    s->housekeeping(now_s);
    s->st.participants = s->established();
}

bool sumi_rtpmidi_send(sumi_rtpmidi_t* s, double now_s, const uint8_t* bytes, size_t n) {
    if (!s || !bytes || n == 0 || s->established() == 0) return false;
    uint8_t pkt[MAX_DATAGRAM];
    const size_t len = sumi_rtpmidi_encode(bytes, n, s->seq, (uint32_t)ticks(now_s), s->ssrc, pkt, sizeof pkt);
    if (len == 0) return false;
    s->seq++;
    uint32_t msgs = 0; for (size_t i = 0; i < n;) { const size_t l = midi_len(bytes[i]); if (!l) break; msgs++; i += l; }
    for (Participant& p : s->parts) if (p.state == ESTABLISHED) s->send_to(s->data_fd, p.data, pkt, len);
    s->st.packets_tx++; s->st.midi_tx += msgs;
    return true;
}

uint32_t sumi_rtpmidi_participants(const sumi_rtpmidi_t* s) { return s ? s->established() : 0; }

const char* sumi_rtpmidi_participant_name(const sumi_rtpmidi_t* s, uint32_t index) {
    if (!s) return nullptr;
    uint32_t k = 0;
    for (const Participant& p : s->parts) if (p.state == ESTABLISHED) { if (k == index) return p.name; k++; }
    return nullptr;
}

void sumi_rtpmidi_close(sumi_rtpmidi_t* s, double now_s) {
    if (!s) return;
    for (Participant& p : s->parts) if (p.state != FREE) s->drop(p, true, now_s);
    s->st.participants = 0;
}

void sumi_rtpmidi_stats(const sumi_rtpmidi_t* s, sumi_rtpmidi_stats_t* out) {
    if (!out) return;
    if (!s) { *out = sumi_rtpmidi_stats_t{}; return; }
    *out = s->st; out->participants = s->established();
}

size_t sumi_rtpmidi_encode(const uint8_t* midi, size_t n, uint16_t seq, uint32_t timestamp, uint32_t ssrc, uint8_t* out, size_t cap) {
    if (!midi || !out || n == 0) return 0;
    // the command list: message, 0x00 delta, message, … (Z = 0: the first has no delta)
    size_t list = 0, i = 0; uint32_t msgs = 0;
    while (i < n) { const size_t l = midi_len(midi[i]); if (l == 0 || i + l > n) return 0; list += l + (msgs ? 1 : 0); msgs++; i += l; }
    if (list > 4095) return 0;
    const size_t head = 12 + (list < 16 ? 1 : 2);
    if (head + list > cap) return 0;
    out[0] = 0x80; out[1] = 0x61; put16(out + 2, seq); put32(out + 4, timestamp); put32(out + 8, ssrc);
    size_t at = 12;
    if (list < 16) out[at++] = (uint8_t)list;
    else { out[at++] = (uint8_t)(0x80 | ((list >> 8) & 0x0F)); out[at++] = (uint8_t)(list & 0xFF); }
    i = 0; msgs = 0;
    while (i < n) {
        const size_t l = midi_len(midi[i]);
        if (msgs) out[at++] = 0x00;
        std::memcpy(out + at, midi + i, l); at += l; i += l; msgs++;
    }
    return at;
}

size_t sumi_rtpmidi_decode(const uint8_t* p, size_t n, uint8_t* midi_out, size_t cap, uint16_t* seq, uint32_t* timestamp, uint32_t* ssrc) {
    if (!p || n < 13 || (p[0] >> 6) != 2 || (p[1] & 0x7F) != 0x61) return 0;
    const size_t head = 12 + 4u * (p[0] & 0x0F) + (((p[0] & 0x10) != 0) ? 4u : 0u);   // CSRCs; an extension header's first word
    if (n <= head) return 0;
    if (seq) *seq = get16(p + 2);
    if (timestamp) *timestamp = get32(p + 4);
    if (ssrc) *ssrc = get32(p + 8);
    size_t at = head;
    const uint8_t flags = p[at++];
    const bool B = flags & 0x80, Z = flags & 0x20;
    size_t len = flags & 0x0F;
    if (B) { if (at >= n) return 0; len = (len << 8) | p[at++]; }
    if (at + len > n) return 0;
    const size_t end = at + len;
    size_t written = 0; uint8_t running = 0; bool first = true;
    while (at < end) {
        if (!first || Z) {   // the delta time, 1–4 bytes, the high bit a continuation
            int k = 0; while (at < end && (p[at] & 0x80) && k < 3) { at++; k++; }
            if (at >= end) break;
            at++;
        }
        first = false;
        uint8_t status;
        if (p[at] & 0x80) { status = p[at++]; if (status < 0xF8) running = status < 0xF0 ? status : 0; }
        else { if (!running) return written; status = running; }
        if (status == 0xF0) { while (at < end && p[at] != 0xF7) at++; if (at < end) at++; continue; }   // SysEx dropped
        const size_t l = midi_len(status);
        if (l == 0) return written;
        if (at + (l - 1) > end) break;
        if (written + l > cap) break;
        midi_out[written++] = status;
        for (size_t k = 1; k < l; k++) midi_out[written++] = p[at++];
    }
    return written;
}

}
