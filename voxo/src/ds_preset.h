// ds_preset.h — Voxo's Decent Sampler front end (Phase 7 step 50, SOUND §3;
// DECISIONS_6 #13–#15): the .dspreset XML into an instrument model, the
// samples decoded to memory (WAV / FLAC / AIFF, from a folder or a
// .dslibrary zip), and THE COMPAT REPORT — what the preset asks for that this
// version plays without. Shell-thread code: STL inside, nothing here is ever
// touched from the callback (the voice reads only what voxo.cpp publishes).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace voxo_ds {

// The compat notes, one bit each — voxo.h's VOXO_NOTE_* mirror these.
enum : uint32_t {
    NOTE_CHORUS          = 1u << 0,
    NOTE_CONVOLUTION     = 1u << 1,
    NOTE_MODULATORS      = 1u << 2,
    NOTE_UI              = 1u << 3,
    NOTE_STREAMING       = 1u << 4,
    NOTE_SEQUENCES       = 1u << 5,
    NOTE_OTHER_FILTERS   = 1u << 6,
    NOTE_UNKNOWN_EFFECT  = 1u << 7,
    NOTE_MISSING_SAMPLES = 1u << 8,
    NOTE_UNKNOWN_BINDING = 1u << 9,
};
// The canonical sentence of one note bit (voxo/COMPAT_REPORT.md quotes these).
const char* note_copy(uint32_t bit);

enum class SeqMode { Always, RoundRobin, Random, TrueRandom };
enum class Trigger { Attack, Release, First, Legato };
enum class EffectKind { Lowpass, Highpass, Bandpass, Peak, Notch, Gain, Reverb, Delay, Chorus, Convolution, Unknown };
enum class Translation { Linear, Table, FixedValue };
enum class SourceKind { Cc, Note, Velocity, MpePressure, MpeTimbre, Aftertouch, PitchBend, Lfo, Envelope, Sequence, UiControl, Unknown };

struct Binding {
    std::string type;        // amp | effect | general | control | modulator | ...
    std::string level;       // instrument | group | tag | ui
    std::string parameter;   // ENV_ATTACK, FX_FILTER_FREQUENCY, AMP_VOLUME, ...
    int position = -1;       // effect index at instrument level (DS "position")
    int group_index = -1;
    int effect_index = -1;
    std::string tags;
    Translation translation = Translation::Linear;
    float out_min = 0.0f, out_max = 1.0f;
    float fixed_value = 0.0f;
    float factor = 1.0f;
    float mod_amount = 1.0f;
    std::string mod_behavior;    // set | add | multiply
    std::vector<std::pair<float, float>> table;
    bool known = true;
    // The binding's output for an input in 0..1 (the knob's normalised value,
    // the CC / 127, the pressure): the translation applied.
    float translate(float in) const;
};

struct Source {                  // a <cc>, <note>, <velocity>, <mpePressure>, <mpeTimbre>, <lfo>...; or a UI control
    SourceKind kind = SourceKind::Unknown;
    std::string element;         // the tag name as written
    int number = -1;             // cc number
    int note_lo = -1, note_hi = -1;
    float value = 0.0f;          // a UI control's starting value (already normalised to 0..1 by min/max)
    float rising_ms = 0.0f, falling_ms = 0.0f;   // the DS smoothing times
    std::string scope;
    std::vector<Binding> bindings;
};

struct Effect {
    EffectKind kind = EffectKind::Unknown;
    std::string type;            // as written
    float frequency = 22000.0f, resonance = 0.7f, q = 0.7f, gain = 1.0f;
    float room_size = 0.7f, damping = 0.3f, wet_level = 0.3f;
    float delay_time = 0.5f, feedback = 0.3f, stereo_offset = 0.0f;
    std::string delay_time_format;
    float level = 1.0f;
    bool supported = false;      // v1 plays it (the low-pass, reverb, delay, gain)
};

struct Zone {
    std::string path;            // as written in the preset
    int sample = -1;             // index into Instrument::samples, -1 when unreadable
    int lo_note = 0, hi_note = 127, root_note = 60;
    int lo_vel = 0, hi_vel = 127;
    float tuning = 0.0f;         // semitones; the effective root is root_note - tuning
    float volume_db = 0.0f;
    float pan = 0.0f;            // -100..100
    int64_t start = 0, end = -1; // frames; -1 = the sample's end
    bool loop_enabled = false;
    int64_t loop_start = 0, loop_end = -1;
    int64_t loop_crossfade = 0;
    int seq_position = 1;
    float release = -1.0f;       // a per-zone release override, seconds; <0 = the group's
    Trigger trigger = Trigger::Attack;
};

struct Group {
    std::string name, tags;
    float attack = 0.0f, decay = 0.0f, sustain = 1.0f, release = 0.0f;   // seconds, 0..1
    float amp_vel_track = 1.0f;
    float volume_db = 0.0f, pan = 0.0f, tuning = 0.0f;
    SeqMode seq_mode = SeqMode::Always;
    int seq_length = 0;
    Trigger trigger = Trigger::Attack;
    std::vector<Effect> effects;
    std::vector<Zone> zones;
};

struct SampleData {
    std::string path;            // as written; unique across zones
    std::vector<float> frames;   // interleaved
    uint32_t channels = 0, rate = 0;
    bool ok = false;
    std::string why;
};

struct Instrument {
    std::string name, min_version, source_path;
    std::vector<Group> groups;
    std::vector<Effect> effects;         // instrument level (the bus)
    std::vector<Source> sources;         // <midi>, <modulators>, the UI controls
    std::vector<SampleData> samples;
    std::vector<std::string> unknown_effects, unknown_bindings;
    uint32_t notes = 0;
    uint32_t zone_count = 0, missing = 0;
    uint64_t memory_bytes = 0;
    std::string report;                  // the calm text, one line per finding
};

// A source of bytes by relative path (a folder beside the preset, or the zip).
struct Reader {
    virtual ~Reader() {}
    virtual bool read(const std::string& relative, std::vector<uint8_t>* out) = 0;
};

// The XML alone (no samples): false with `why` when it is not a preset.
bool parse(const char* text, size_t len, Instrument* out, std::string* why);
// The samples through `reader`, the notes and the report text; the model must
// have been parsed. Unreadable samples are counted, never fatal.
void load_samples(Instrument* inst, Reader& reader);
void build_report(Instrument* inst);
// Everything: `path` is a .dspreset (its folder is the reader) or a .dslibrary
// (the zip is the reader; its first .dspreset is the preset).
bool load(const std::string& path, Instrument* out, std::string* why);

// Decoders (decoders.cpp): WAV / FLAC / AIFF from memory into interleaved float.
bool decode_audio(const std::string& name_hint, const uint8_t* bytes, size_t size,
                  std::vector<float>* frames, uint32_t* channels, uint32_t* rate, std::string* why);

} // namespace voxo_ds
