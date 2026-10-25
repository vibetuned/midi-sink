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
    NOTE_MEMORY          = 1u << 10,   // step 52: larger than the advised budget; loaded anyway
};
// The canonical sentence of one note bit (voxo/COMPAT_REPORT.md quotes these).
const char* note_copy(uint32_t bit);

enum class SeqMode { Always, RoundRobin, Random, TrueRandom };
enum class Trigger { Attack, Release, First, Legato, Continuous };
// Step 87 (TO_PROJECT_SPEC §12.7): how a sample silenced through `silencedByTags` stops — the format's
// `silencingMode`: fast (the default, a short fade) or normal (the sample's release phase).
enum class SilencingMode { Fast, Normal };
enum class EffectKind { Lowpass, Highpass, Bandpass, Peak, Notch, LowShelf, HighShelf, Gain, WaveShaper, Reverb, Delay, Chorus, Phaser, Convolution, Unknown };   // step 88: the phaser; step 89: the shelves and the wave shaper
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
    // Step 89: an <envelope> modulator's own attributes (seconds, 0..1, 0..1; the format's modBehavior as written).
    float attack_s = 0.0f, decay_s = 0.0f, sustain_l = 1.0f, release_s = 0.0f, mod_amount = 1.0f;
    std::string mod_behavior;
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
    float mix = 0.5f, mod_depth = 0.2f, mod_rate = 0.2f, center_frequency = 400.0f;   // step 88: chorus / phaser (the format's defaults)
    bool enabled = true;         // step 88: the format's `enabled` attribute (false = bypassed)
    // Step 89: the filter family, the EQ, the gain and the wave shaper — the format's attributes.
    bool  one_pole = false;      // lowpass_1pl (no resonance)
    float gain_lin = 1.0f;       // peak / low_shelf / high_shelf: `gain` (linear, or dB under gainUnit="decibels") as a linear factor
    float level_lin = 0.5011872f; // gain: `level` (dB by default, "-6dB" allowed; linear under levelUnit="linear") as a factor — the format's default −6 dB
    float drive = 1.0f, drive_boost = 1.0f, output_level = 0.1f;   // wave_shaper: drive 1..1000, driveBoost 0..1, outputLevel 0..1
    bool supported = false;      // played (the low-pass, reverb, delay, gain; chorus and phaser since step 88)
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
    // Step 87 (TO_PROJECT_SPEC §12.7; DECISIONS_14 #1) — the pedal mechanics, the format's attributes:
    int on_cc = -1, on_lo = -1, on_hi = -1;       // onLoCCN / onHiCCN: the zone fires when CC N ENTERS [lo, hi] (a pedal noise);
                                                  //   such a zone never answers a note. Cascades from the group.
    int cond_cc = -1, cond_lo = -1, cond_hi = -1; // loCCN / hiCCN: a note-on plays the zone only with CC N's last value in [lo, hi]
                                                  //   (the pedal-down resonance group). Cascades from the group.
    float pitch_key_track = 1.0f;                 // pitchKeyTrack: 1 the note's ratio, 0 the root whatever the note (the noises)
    std::string tags;                             // the zone's tags: the group's and its own
    std::string silenced_by_tags;                 // silencedByTags: a zone carrying one of these starting stops this one
    SilencingMode silencing_mode = SilencingMode::Fast;
    float silencing_decay = 0.0f;                 // silencingDecay, seconds; 0 = by the mode
};

struct Group {
    std::string name, tags;
    float attack = 0.0f, decay = 0.0f, sustain = 1.0f, release = 0.0f;   // seconds, 0..1
    float amp_vel_track = 1.0f;
    float volume_db = 0.0f, pan = 0.0f, tuning = 0.0f;
    SeqMode seq_mode = SeqMode::Always;
    int seq_length = 0;
    Trigger trigger = Trigger::Attack;
    // Step 87: the group's own copies of the pedal attributes (the zones carry the resolved ones).
    std::string silenced_by_tags;
    SilencingMode silencing_mode = SilencingMode::Fast;
    float silencing_decay = 0.0f;
    float pitch_key_track = 1.0f;
    float sustain_decay = 0.0f;   // sustainDecay — VOXO'S OWN attribute (the format has none): the seconds a note released
                                  //   under the full pedal takes to decay; 0 = held, as a pedalled note always was. The
                                  //   half-pedal interpolates the decay RATE between the release (pedal up) and this.
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
    uint64_t memory_estimate = 0;        // step 52: the decoded size read off the headers before decoding
    uint64_t memory_budget = 0;          // the shell's advice (0 = none)
    std::string report;                  // the calm text, one line per finding
};

// A source of bytes by relative path (a folder beside the preset, or the zip).
struct Reader {
    virtual ~Reader() {}
    virtual bool read(const std::string& relative, std::vector<uint8_t>* out) = 0;
    // The first `max_bytes` of a file (the headers the size estimate needs) and its full size.
    virtual bool read_head(const std::string& relative, size_t max_bytes, std::vector<uint8_t>* out, uint64_t* file_size) = 0;
};

// The XML alone (no samples): false with `why` when it is not a preset.
bool parse(const char* text, size_t len, Instrument* out, std::string* why);
// The decoded size the samples will take, read off their headers (WAV data
// chunk, FLAC STREAMINFO, AIFF COMM; twice the file size when a header cannot
// be read) — the advisory gate's number (DECISIONS_6 #20), before any decoding.
uint64_t estimate_decoded_bytes(const Instrument& inst, Reader& reader);
// The samples through `reader`, the notes and the report text; the model must
// have been parsed. Unreadable samples are counted, never fatal.
void load_samples(Instrument* inst, Reader& reader);
void build_report(Instrument* inst);
// Everything: `path` is a .dspreset (its folder is the reader) or a .dslibrary
// (the zip is the reader; its first .dspreset is the preset).
// `budget` is the shell's memory advice in bytes (0 = no gate): an estimate
// above it becomes the memory note — a warning, never a wall.
bool load(const std::string& path, Instrument* out, std::string* why, uint64_t budget = 0);

// Decoders (decoders.cpp): WAV / FLAC / AIFF from memory into interleaved float.
bool decode_audio(const std::string& name_hint, const uint8_t* bytes, size_t size,
                  std::vector<float>* frames, uint32_t* channels, uint32_t* rate, std::string* why);

} // namespace voxo_ds
