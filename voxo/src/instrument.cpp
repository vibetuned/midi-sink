// instrument.cpp — see instrument.h. Shell-thread code (the STL is fine here).
#include "instrument.h"
#include "ds_preset.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace voxo_inst {

namespace {

inline float note_to_hz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }
inline float db_to_lin(float db) { return std::pow(10.0f, db / 20.0f); }

void curve_from_binding(Curve* c, const voxo_ds::Binding& b, Target target, int32_t group) {
    c->target = target;
    c->group = group;
    for (int k = 0; k <= 32; k++) c->points[k] = b.translate((float)k / 32.0f);
}
void curve_identity(Curve* c, Target target, int32_t group, float lo, float hi) {
    c->target = target; c->group = group;
    for (int k = 0; k <= 32; k++) c->points[k] = lo + (hi - lo) * (float)k / 32.0f;
}
void curve_none(Curve* c) { c->target = Target::None; c->group = -1; for (float& p : c->points) p = 0.0f; }

// Step 88: the format's generic effect parameters (FX_MIX, FX_MOD_DEPTH, FX_MOD_RATE, FX_FEEDBACK, FX_CENTER_FREQUENCY)
// name the effect by its position, so the binding's target depends on WHICH effect sits there — `kind` is that
// effect's; the unambiguous names (FX_REVERB_*, FX_DELAY_*, FX_FILTER_*, ENV_*, AMP_VOLUME) resolve without it.
Target target_of(const std::string& parameter, voxo_ds::EffectKind kind = voxo_ds::EffectKind::Unknown) {
    using K = voxo_ds::EffectKind;
    if (parameter == "FX_MIX") return kind == K::Chorus ? Target::ChorusMix : kind == K::Phaser ? Target::PhaserMix : Target::None;
    if (parameter == "FX_MOD_DEPTH") return kind == K::Chorus ? Target::ChorusDepth : kind == K::Phaser ? Target::PhaserDepth : Target::None;
    if (parameter == "FX_MOD_RATE") return kind == K::Chorus ? Target::ChorusRate : kind == K::Phaser ? Target::PhaserRate : Target::None;
    if (parameter == "FX_CENTER_FREQUENCY") return kind == K::Phaser ? Target::PhaserCenter : Target::None;
    if (parameter == "FX_FEEDBACK") return kind == K::Phaser ? Target::PhaserFeedback : kind == K::Delay ? Target::DelayFeedback : Target::None;
    if (parameter == "FX_REVERB_WET_LEVEL") return Target::ReverbWet;
    if (parameter == "FX_REVERB_ROOM_SIZE") return Target::ReverbRoom;
    if (parameter == "FX_REVERB_DAMPING") return Target::ReverbDamping;
    if (parameter == "FX_DELAY_WET_LEVEL") return Target::DelayWet;
    if (parameter == "FX_DELAY_TIME") return Target::DelayTime;
    if (parameter == "FX_DELAY_FEEDBACK") return Target::DelayFeedback;
    if (parameter == "AMP_VOLUME") return Target::AmpVolume;
    if (parameter == "FX_FILTER_FREQUENCY") return Target::FilterCutoff;
    if (parameter == "FX_FILTER_RESONANCE") return Target::FilterResonance;
    if (parameter == "ENV_ATTACK") return Target::EnvAttack;
    if (parameter == "ENV_DECAY") return Target::EnvDecay;
    if (parameter == "ENV_SUSTAIN") return Target::EnvSustain;
    if (parameter == "ENV_RELEASE") return Target::EnvRelease;
    return Target::None;
}
// Step 88: the effect a binding points at — the instrument's by `position`, a group's by `effectIndex`.
voxo_ds::EffectKind binding_kind(const voxo_ds::Instrument* model, const voxo_ds::Binding& b) {
    if (b.level == "group") {
        if (b.group_index >= 0 && (size_t)b.group_index < model->groups.size()) {
            const auto& fx = model->groups[(size_t)b.group_index].effects;
            if (b.effect_index >= 0 && (size_t)b.effect_index < fx.size()) return fx[(size_t)b.effect_index].kind;
        }
        return voxo_ds::EffectKind::Unknown;
    }
    if (b.position >= 0 && (size_t)b.position < model->effects.size()) return model->effects[(size_t)b.position].kind;
    if (b.effect_index >= 0 && (size_t)b.effect_index < model->effects.size()) return model->effects[(size_t)b.effect_index].kind;
    return voxo_ds::EffectKind::Unknown;
}

// The default MPE curves (DECISIONS_6 #17): pressure to expression (0.35 + 0.65 p),
// timbre to the cutoff as a multiplier 2^((t - 0.5) * 6) — stored as the multiplier.
void default_pressure(Curve* c, int32_t group) { curve_identity(c, Target::AmpVolume, group, 0.35f, 1.0f); }
void default_timbre(Curve* c, int32_t group) {
    c->target = Target::FilterCutoff; c->group = group;
    for (int k = 0; k <= 32; k++) c->points[k] = std::exp2(((float)k / 32.0f - 0.5f) * 6.0f);
}

// Re-lay a decoded sample with the padding the reads need, into `dst`.
const float* lay_padded(std::vector<float>& dst, const float* src, size_t frames, uint32_t channels) {
    const size_t at = dst.size();
    dst.resize(at + (frames + PAD_BEFORE + PAD_AFTER) * channels, 0.0f);
    std::memcpy(dst.data() + at + PAD_BEFORE * channels, src, frames * channels * sizeof(float));
    return dst.data() + at + PAD_BEFORE * channels;
}

} // namespace

Instrument::~Instrument() { delete model; }

Instrument* compile_sample(const float* frames, uint32_t frame_count, uint32_t channels, uint32_t sample_rate, float root_note) {
    Instrument* inst = new Instrument;
    inst->own_frames.reserve((frame_count + PAD_BEFORE + PAD_AFTER) * channels);
    const float* data = lay_padded(inst->own_frames, frames, frame_count, channels);
    Zone z{};
    z.data = data; z.frames = frame_count; z.channels = channels; z.rate = sample_rate;
    z.root_hz = note_to_hz(root_note);
    z.lo_note = 0; z.hi_note = 127; z.lo_vel = 0; z.hi_vel = 127;
    z.gain = 1.0f; z.pan = 0.0f; z.start = 0; z.end = frame_count;
    z.loop = false; z.seq_position = 1; z.group = 0; z.release_trigger = false;
    z.on_cc = z.cond_cc = 255; z.on_lo = z.on_hi = z.cond_lo = z.cond_hi = 0;   // step 87: a note zone, unconditional
    z.key_track = 1.0f; z.tag_mask = 0; z.silenced_by = 0; z.silence_s = -1.0f;
    inst->zones.push_back(z);
    std::memset(inst->note_mask, 0xFF, sizeof(inst->note_mask));   // a raw sample answers every note
    Group g{};
    g.attack = 0.003f; g.decay = 0.0f; g.sustain = 1.0f; g.release = 0.04f;
    g.sustain_decay = 0.0f;
    g.amp_vel_track = 1.0f;
    g.seq_mode = SeqMode::Always; g.seq_length = 0;
    g.has_filter = true; g.cutoff = 22000.0f; g.resonance = 0.0f;
    default_pressure(&g.pressure, 0); g.pressure_is_default = true;
    default_timbre(&g.timbre, 0); g.timbre_is_default = true;
    curve_none(&g.swirl); curve_none(&g.velocity_to_cutoff);
    g.pressure_rise_ms = 20.0f; g.pressure_fall_ms = 40.0f; g.timbre_rise_ms = 20.0f; g.timbre_fall_ms = 40.0f;
    g.chorus = false; g.chorus_mix = 0.5f; g.chorus_depth = 0.2f; g.chorus_rate = 0.2f;   // step 88
    inst->groups.push_back(g);
    GroupState st{}; st.rr_counter = 1; st.rng = 0x1234567u; st.gain = 1.0f; st.cutoff = g.cutoff; st.resonance = g.resonance;
    st.attack = g.attack; st.decay = g.decay; st.sustain = g.sustain; st.release = g.release;
    st.chorus_mix = g.chorus_mix; st.chorus_depth = g.chorus_depth; st.chorus_rate = g.chorus_rate;
    inst->state.push_back(st);
    return inst;
}

Instrument* compile(voxo_ds::Instrument* model) {
    Instrument* inst = new Instrument;
    inst->model = model;
    // 1. Every readable sample re-laid with padding (one big block).
    size_t total = 0;
    for (const voxo_ds::SampleData& s : model->samples) if (s.ok) total += (s.frames.size() / s.channels + PAD_BEFORE + PAD_AFTER) * s.channels;
    inst->own_frames.reserve(total);
    std::vector<const float*> laid(model->samples.size(), nullptr);
    std::vector<uint32_t> frames_of(model->samples.size(), 0);
    for (size_t i = 0; i < model->samples.size(); i++) {
        voxo_ds::SampleData& s = model->samples[i];
        if (!s.ok) continue;
        frames_of[i] = (uint32_t)(s.frames.size() / s.channels);
        laid[i] = lay_padded(inst->own_frames, s.frames.data(), frames_of[i], s.channels);
        std::vector<float>().swap(s.frames);   // the padded block is the one the voices read
    }

    // 2. The tags' volumes from the UI controls' starting values (TAG_VOLUME bindings).
    std::vector<std::pair<std::string, float>> tag_volume;
    for (const voxo_ds::Source& src : model->sources) {
        if (src.kind != voxo_ds::SourceKind::UiControl) continue;
        for (const voxo_ds::Binding& b : src.bindings) {
            if (b.parameter == "TAG_VOLUME" && !b.tags.empty()) tag_volume.push_back({b.tags, b.translate(src.value)});
            else if (b.parameter == "TAG_VOLUME" && b.level == "tag") {
                // identifier= carries the tag in some presets; the tags attribute in others
            }
        }
    }
    auto tag_gain = [&](const std::string& tags) {
        float g = 1.0f;
        if (tags.empty()) return g;
        for (const auto& tv : tag_volume) {
            // a group carries its tags comma-separated; a match on any of them applies
            size_t p = 0;
            while (p <= tags.size()) {
                size_t q = tags.find(',', p); if (q == std::string::npos) q = tags.size();
                if (tags.substr(p, q - p) == tv.first) { g *= tv.second; break; }
                p = q + 1;
            }
        }
        return g;
    };

    // 2b. Step 87: the tag dictionary — every distinct tag a zone carries or is silenced by, one bit each, 64 at most
    //     (a tag past the 64th is dropped from the masks: it neither silences nor is silenced).
    std::vector<std::string> tag_names;
    auto mask_of = [&](const std::string& tags) {
        uint64_t m = 0;
        size_t p = 0;
        while (p <= tags.size() && !tags.empty()) {
            size_t q = tags.find(',', p); if (q == std::string::npos) q = tags.size();
            size_t a = p, b = q;
            while (a < b && (tags[a] == ' ' || tags[a] == '\t')) a++;
            while (b > a && (tags[b - 1] == ' ' || tags[b - 1] == '\t')) b--;
            if (b > a) {
                const std::string t = tags.substr(a, b - a);
                size_t k = 0;
                for (; k < tag_names.size(); k++) if (tag_names[k] == t) break;
                if (k == tag_names.size() && tag_names.size() < 64) tag_names.push_back(t);
                if (k < 64 && k < tag_names.size()) m |= (uint64_t)1 << k;
            }
            p = q + 1;
        }
        return m;
    };

    // 3. Groups and zones.
    const int32_t ngroups = (int32_t)model->groups.size();
    for (int32_t gi = 0; gi < ngroups; gi++) {
        const voxo_ds::Group& mg = model->groups[(size_t)gi];
        Group g{};
        g.attack = std::max(0.0f, mg.attack); g.decay = std::max(0.0f, mg.decay);
        g.sustain = std::clamp(mg.sustain, 0.0f, 1.0f); g.release = std::max(0.0f, mg.release);
        g.sustain_decay = std::max(0.0f, mg.sustain_decay);   // step 87
        g.amp_vel_track = std::clamp(mg.amp_vel_track, 0.0f, 1.0f);
        g.seq_mode = mg.seq_mode == voxo_ds::SeqMode::RoundRobin ? SeqMode::RoundRobin
                   : (mg.seq_mode == voxo_ds::SeqMode::Random || mg.seq_mode == voxo_ds::SeqMode::TrueRandom) ? SeqMode::Random : SeqMode::Always;
        uint16_t max_pos = 0;
        for (const voxo_ds::Zone& z : mg.zones) max_pos = std::max<uint16_t>(max_pos, (uint16_t)z.seq_position);
        g.seq_length = mg.seq_length > 0 ? (uint16_t)mg.seq_length : max_pos;
        // The low-pass: the group's first, else the instrument's first.
        g.has_filter = false; g.cutoff = 22000.0f; g.resonance = 0.0f;
        const voxo_ds::Effect* lp = nullptr;
        for (const voxo_ds::Effect& e : mg.effects) if (e.kind == voxo_ds::EffectKind::Lowpass) { lp = &e; break; }
        if (!lp) for (const voxo_ds::Effect& e : model->effects) if (e.kind == voxo_ds::EffectKind::Lowpass) { lp = &e; break; }
        if (lp) { g.has_filter = true; g.cutoff = std::clamp(lp->frequency, 20.0f, 22000.0f); g.resonance = std::clamp(lp->resonance - 0.7f, 0.0f, 1.0f); }
        else g.has_filter = true;   // every voice has the filter: CC74 must move something (SOUND §3)
        default_pressure(&g.pressure, gi); g.pressure_is_default = true;
        default_timbre(&g.timbre, gi); g.timbre_is_default = true;
        curve_none(&g.swirl); curve_none(&g.velocity_to_cutoff);
        g.pressure_rise_ms = 20.0f; g.pressure_fall_ms = 40.0f; g.timbre_rise_ms = 20.0f; g.timbre_fall_ms = 40.0f;
        // Step 88: the group's chorus — the format's group-level effect, one instance a note (the first enabled one).
        g.chorus = false; g.chorus_mix = 0.5f; g.chorus_depth = 0.2f; g.chorus_rate = 0.2f;
        for (const voxo_ds::Effect& e : mg.effects) {
            if (e.kind != voxo_ds::EffectKind::Chorus || !e.enabled) continue;
            g.chorus = true; g.chorus_mix = std::clamp(e.mix, 0.0f, 1.0f); g.chorus_depth = std::clamp(e.mod_depth, 0.0f, 1.0f); g.chorus_rate = std::clamp(e.mod_rate, 0.0f, 10.0f);
            inst->group_chorus = true;
            break;
        }
        const float group_gain = db_to_lin(mg.volume_db) * tag_gain(mg.tags);
        for (const voxo_ds::Zone& mz : mg.zones) {
            if (mz.sample < 0 || !laid[(size_t)mz.sample]) continue;
            const voxo_ds::SampleData& sd = model->samples[(size_t)mz.sample];
            const uint32_t frames = frames_of[(size_t)mz.sample];
            Zone z{};
            z.data = laid[(size_t)mz.sample];
            z.channels = sd.channels; z.rate = sd.rate; z.frames = frames;
            inst->zones.push_back(z);
            Zone& zz = inst->zones.back();
            zz.root_hz = note_to_hz((float)mz.root_note - mz.tuning);
            zz.lo_note = (uint8_t)mz.lo_note; zz.hi_note = (uint8_t)mz.hi_note;
            zz.lo_vel = (uint8_t)mz.lo_vel; zz.hi_vel = (uint8_t)mz.hi_vel;
            zz.gain = db_to_lin(mz.volume_db) * group_gain;
            zz.pan = std::clamp(mz.pan / 100.0f, -1.0f, 1.0f);
            zz.loop = mz.loop_enabled;
            zz.seq_position = (uint16_t)std::max(1, mz.seq_position);
            zz.group = (uint16_t)gi;
            zz.release_trigger = mz.trigger == voxo_ds::Trigger::Release;
            // Step 87: the pedal mechanics (the resolved attributes of the zone).
            zz.on_cc = (uint8_t)(mz.on_cc >= 0 && mz.on_cc <= 127 ? mz.on_cc : 255);
            zz.on_lo = (uint8_t)std::clamp(mz.on_lo, 0, 127); zz.on_hi = (uint8_t)std::clamp(mz.on_hi, 0, 127);
            zz.cond_cc = (uint8_t)(mz.cond_cc >= 0 && mz.cond_cc <= 127 ? mz.cond_cc : 255);
            zz.cond_lo = (uint8_t)std::clamp(mz.cond_lo, 0, 127); zz.cond_hi = (uint8_t)std::clamp(mz.cond_hi, 0, 127);
            zz.key_track = std::clamp(mz.pitch_key_track, 0.0f, 1.0f);
            zz.tag_mask = mask_of(mz.tags);
            zz.silenced_by = mask_of(mz.silenced_by_tags);
            zz.silence_s = mz.silencing_decay > 0.0f ? mz.silencing_decay : (mz.silencing_mode == voxo_ds::SilencingMode::Normal ? -1.0f : 0.005f);
            if (zz.silenced_by) inst->choke = true;
            zz.start = (uint32_t)std::min<int64_t>(std::max<int64_t>(0, mz.start), frames);
            zz.end = (mz.end < 0 || mz.end > frames) ? frames : (uint32_t)mz.end;
            if (zz.start >= zz.end) zz.start = zz.end > 0 ? zz.end - 1 : 0;
            zz.loop_start = (uint32_t)std::min<int64_t>(std::max<int64_t>(0, mz.loop_start), frames);
            zz.loop_end = (mz.loop_end < 0 || mz.loop_end > frames) ? frames : (uint32_t)mz.loop_end;
            if (zz.loop_start >= zz.loop_end) zz.loop = false;
            zz.loop_xfade = (uint32_t)std::max<int64_t>(0, mz.loop_crossfade);
            if (zz.loop) {
                zz.loop_xfade = std::min(zz.loop_xfade, (zz.loop_end - zz.loop_start) / 2);
                zz.loop_xfade = std::min(zz.loop_xfade, zz.loop_start);   // the crossfade reads before loop_start
            }
        }
        inst->groups.push_back(g);
        GroupState st{}; st.rr_counter = 1; st.rng = 0x9E3779B9u ^ (uint32_t)gi; st.gain = 1.0f; st.cutoff = g.cutoff; st.resonance = g.resonance;
        st.attack = g.attack; st.decay = g.decay; st.sustain = g.sustain; st.release = g.release;
        st.chorus_mix = g.chorus_mix; st.chorus_depth = g.chorus_depth; st.chorus_rate = g.chorus_rate;   // step 88
        inst->state.push_back(st);
    }
    // 4. Velocity crossfades: where two zones of a group share notes and overlap in
    //    velocity, each fades across the overlap (DECISIONS_6 #16).
    for (size_t a = 0; a < inst->zones.size(); a++) {
        Zone& za = inst->zones[a];
        for (size_t b = 0; b < inst->zones.size(); b++) {
            if (a == b) continue;
            const Zone& zb = inst->zones[b];
            if (za.group != zb.group || za.release_trigger != zb.release_trigger) continue;
            if (zb.hi_note < za.lo_note || zb.lo_note > za.hi_note) continue;      // no shared notes
            if (za.lo_vel == zb.lo_vel && za.hi_vel == zb.hi_vel) continue;         // stacked: both play in full
            const int lo = std::max(za.lo_vel, zb.lo_vel), hi = std::min(za.hi_vel, zb.hi_vel);
            if (hi < lo) continue;                                                  // disjoint: a hard split
            const float w = (float)(hi - lo + 1);
            if (zb.lo_vel < za.lo_vel) za.fade_lo = std::max(za.fade_lo, w);        // the neighbour below: fade in at the bottom
            if (zb.hi_vel > za.hi_vel) za.fade_hi = std::max(za.fade_hi, w);        // the neighbour above: fade out at the top
        }
    }
    // 4a. The reach (step 53): every note an attack zone covers (a CC-triggered zone answers no note — step 87).
    std::memset(inst->note_mask, 0, sizeof(inst->note_mask));
    for (const Zone& z : inst->zones) {
        if (z.release_trigger || z.on_cc != 255) continue;
        for (int n = z.lo_note; n <= z.hi_note; n++) inst->note_mask[n / 8] |= (uint8_t)(1u << (n % 8));
    }
    // 4c. Step 87: the CC-triggered zones (the pedal noises) and the CCs they watch, for the callback's lookup.
    inst->cc_zones.clear();
    std::memset(inst->cc_trigger_mask, 0, sizeof(inst->cc_trigger_mask));
    for (uint32_t i = 0; i < (uint32_t)inst->zones.size(); i++) {
        const Zone& z = inst->zones[i];
        if (z.on_cc == 255) continue;
        inst->cc_zones.push_back(i);
        inst->cc_trigger_mask[z.on_cc / 8] |= (uint8_t)(1u << (z.on_cc % 8));
    }
    // 4b. The bus (step 52): the instrument's first reverb and first delay, DS's parameters; step 88: its first chorus
    //     and first phaser; an effect with enabled="false" is passed over.
    for (const voxo_ds::Effect& e : model->effects) {
        if (!e.enabled) continue;
        if (e.kind == voxo_ds::EffectKind::Reverb && !inst->bus.reverb_on) {
            inst->bus.reverb_on = true;
            inst->bus.room_size = std::clamp(e.room_size, 0.0f, 1.0f);
            inst->bus.damping = std::clamp(e.damping, 0.0f, 1.0f);
            inst->bus.reverb_wet = std::clamp(e.wet_level, 0.0f, 1.0f);
        } else if (e.kind == voxo_ds::EffectKind::Delay && !inst->bus.delay_on) {
            inst->bus.delay_on = true;
            inst->bus.delay_time = std::clamp(e.delay_time, 0.001f, 2.0f);
            inst->bus.feedback = std::clamp(e.feedback, 0.0f, 0.95f);
            inst->bus.stereo_offset = std::clamp(e.stereo_offset, -0.5f, 0.5f);
            inst->bus.delay_wet = std::clamp(e.wet_level, 0.0f, 1.0f);
        } else if (e.kind == voxo_ds::EffectKind::Chorus && !inst->bus.chorus_on) {
            inst->bus.chorus_on = true;
            inst->bus.chorus_mix = std::clamp(e.mix, 0.0f, 1.0f);
            inst->bus.chorus_depth = std::clamp(e.mod_depth, 0.0f, 1.0f);
            inst->bus.chorus_rate = std::clamp(e.mod_rate, 0.0f, 10.0f);
        } else if (e.kind == voxo_ds::EffectKind::Phaser && !inst->bus.phaser_on) {
            inst->bus.phaser_on = true;
            inst->bus.phaser_mix = std::clamp(e.mix, 0.0f, 1.0f);
            inst->bus.phaser_depth = std::clamp(e.mod_depth, 0.0f, 1.0f);
            inst->bus.phaser_rate = std::clamp(e.mod_rate, 0.0f, 10.0f);
            inst->bus.phaser_center = std::clamp(e.center_frequency, 20.0f, 22000.0f);
            inst->bus.phaser_feedback = std::clamp(e.feedback, -1.0f, 1.0f);
        }
    }
    // 5. Bindings: the MPE sources, the velocity and CC bindings, the UI starting values.
    auto set_bus = [&](Target t, float out) {
        voxo_bus::Params& b = inst->bus;
        switch (t) {
            case Target::ReverbWet: b.reverb_wet = std::clamp(out, 0.0f, 1.0f); break;
            case Target::ReverbRoom: b.room_size = std::clamp(out, 0.0f, 1.0f); break;
            case Target::ReverbDamping: b.damping = std::clamp(out, 0.0f, 1.0f); break;
            case Target::DelayWet: b.delay_wet = std::clamp(out, 0.0f, 1.0f); break;
            case Target::DelayTime: b.delay_time = std::clamp(out, 0.001f, 2.0f); break;
            case Target::DelayFeedback: b.feedback = std::clamp(out, 0.0f, 0.95f); break;
            case Target::ChorusMix: b.chorus_mix = std::clamp(out, 0.0f, 1.0f); break;          // step 88
            case Target::ChorusDepth: b.chorus_depth = std::clamp(out, 0.0f, 1.0f); break;
            case Target::ChorusRate: b.chorus_rate = std::clamp(out, 0.0f, 10.0f); break;
            case Target::PhaserMix: b.phaser_mix = std::clamp(out, 0.0f, 1.0f); break;
            case Target::PhaserDepth: b.phaser_depth = std::clamp(out, 0.0f, 1.0f); break;
            case Target::PhaserRate: b.phaser_rate = std::clamp(out, 0.0f, 10.0f); break;
            case Target::PhaserCenter: b.phaser_center = std::clamp(out, 20.0f, 22000.0f); break;
            case Target::PhaserFeedback: b.phaser_feedback = std::clamp(out, -1.0f, 1.0f); break;
            default: break;
        }
    };
    auto apply_static = [&](const voxo_ds::Binding& b, float value01) {
        const Target t = target_of(b.parameter, binding_kind(model, b));
        if (t == Target::None) return;
        const float out = b.translate(value01);
        if (t >= Target::ReverbWet || (target_is_chorus(t) && b.level != "group")) { set_bus(t, out); return; }
        auto set = [&](Group& g, GroupState& st) {
            switch (t) {
                case Target::AmpVolume: st.gain = std::clamp(out, 0.0f, 2.0f); break;
                case Target::FilterCutoff: g.cutoff = st.cutoff = std::clamp(out, 20.0f, 22000.0f); g.has_filter = true; break;
                case Target::FilterResonance: g.resonance = st.resonance = std::clamp(out - 0.7f, 0.0f, 1.0f); break;
                case Target::EnvAttack: g.attack = st.attack = std::max(0.0f, out); break;
                case Target::EnvDecay: g.decay = st.decay = std::max(0.0f, out); break;
                case Target::EnvSustain: g.sustain = st.sustain = std::clamp(out, 0.0f, 1.0f); break;
                case Target::EnvRelease: g.release = st.release = std::max(0.0f, out); break;
                case Target::ChorusMix: g.chorus_mix = st.chorus_mix = std::clamp(out, 0.0f, 1.0f); break;      // step 88
                case Target::ChorusDepth: g.chorus_depth = st.chorus_depth = std::clamp(out, 0.0f, 1.0f); break;
                case Target::ChorusRate: g.chorus_rate = st.chorus_rate = std::clamp(out, 0.0f, 10.0f); break;
                default: break;
            }
        };
        if (b.level == "group" && b.group_index >= 0 && b.group_index < ngroups) set(inst->groups[(size_t)b.group_index], inst->state[(size_t)b.group_index]);
        else for (int32_t gi = 0; gi < ngroups; gi++) set(inst->groups[(size_t)gi], inst->state[(size_t)gi]);
    };
    for (const voxo_ds::Source& src : model->sources) {
        switch (src.kind) {
            case voxo_ds::SourceKind::UiControl:
                for (const voxo_ds::Binding& b : src.bindings) apply_static(b, src.value);
                break;
            case voxo_ds::SourceKind::MpePressure:
            case voxo_ds::SourceKind::MpeTimbre:
            case voxo_ds::SourceKind::Aftertouch:
                for (const voxo_ds::Binding& b : src.bindings) {
                    const Target t = target_of(b.parameter);
                    if (t != Target::AmpVolume && t != Target::FilterCutoff) continue;
                    const bool timbre = src.kind == voxo_ds::SourceKind::MpeTimbre;
                    auto put = [&](Group& g, int32_t gi) {
                        Curve* c = timbre ? &g.timbre : &g.pressure;
                        curve_from_binding(c, b, t, gi);
                        if (timbre) { g.timbre_is_default = false; if (src.rising_ms > 0) g.timbre_rise_ms = src.rising_ms; if (src.falling_ms > 0) g.timbre_fall_ms = src.falling_ms; }
                        else { g.pressure_is_default = false; if (src.rising_ms > 0) g.pressure_rise_ms = src.rising_ms; if (src.falling_ms > 0) g.pressure_fall_ms = src.falling_ms; }
                    };
                    if (b.level == "group" && b.group_index >= 0 && b.group_index < ngroups) put(inst->groups[(size_t)b.group_index], b.group_index);
                    else for (int32_t gi = 0; gi < ngroups; gi++) put(inst->groups[(size_t)gi], gi);
                }
                break;
            case voxo_ds::SourceKind::Velocity:
                for (const voxo_ds::Binding& b : src.bindings) {
                    if (target_of(b.parameter) != Target::FilterCutoff) continue;
                    // DS's velocity-to-cutoff: modAmount of the cutoff follows the velocity.
                    auto put = [&](Group& g, int32_t gi) {
                        g.velocity_to_cutoff.target = Target::FilterCutoff; g.velocity_to_cutoff.group = gi;
                        const float m = std::clamp(b.mod_amount, 0.0f, 1.0f);
                        for (int k = 0; k <= 32; k++) g.velocity_to_cutoff.points[k] = 1.0f - m + m * (float)k / 32.0f;
                    };
                    if (b.level == "group" && b.group_index >= 0 && b.group_index < ngroups) put(inst->groups[(size_t)b.group_index], b.group_index);
                    else for (int32_t gi = 0; gi < ngroups; gi++) put(inst->groups[(size_t)gi], gi);
                }
                break;
            case voxo_ds::SourceKind::Cc:
                if (src.number < 0 || src.number > 127) break;
                for (const voxo_ds::Binding& b : src.bindings) {
                    const Target t = target_of(b.parameter, binding_kind(model, b));
                    if (t == Target::None || inst->cc_bindings.size() >= 128) continue;
                    CcBinding cb; cb.cc = (uint8_t)src.number;
                    const bool grouped = b.level == "group" && b.group_index >= 0 && b.group_index < ngroups;
                    curve_from_binding(&cb.curve, b, t, grouped ? b.group_index : (target_is_chorus(t) ? BUS_GROUP : -1));   // step 88: an ungrouped chorus curve is the bus's
                    inst->cc_bindings.push_back(cb);
                }
                break;
            default: break;
        }
    }
    return inst;
}

} // namespace voxo_inst
