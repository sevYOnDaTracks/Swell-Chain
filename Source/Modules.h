#pragma once
// The modules of Swell Chain: the signal processing of seven Swell plugins behind one interface.
// Each module reads its knobs in real units (see the KnobSpec tables) and processes stereo in place.
#include <juce_dsp/juce_dsp.h>
#include "dsp/DSP.h"
#include "dsp/VoxDSP.h"
#include "dsp/LiftDSP.h"
#include "dsp/GlueDSP.h"
#include "dsp/AdLibApply.h"
#include "dsp/SpaceEngine.h"

namespace chain
{
enum class Type { empty = 0, match, vox, lift, adlib, space, knock, glue };
constexpr int typeCount = 8;
constexpr int maxKnobs = 7;

struct KnobSpec
{
    const char* label;
    float min, max, def;
    const char* unit;   // "%", "dB", "st", "Hz" or "" (toggle)
    bool toggle = false;
};

struct ModuleInfo
{
    const char* name;
    const char* role;
    juce::uint32 colour;
    std::vector<KnobSpec> knobs;
    std::vector<std::string> presets;   // empty: the module has no presets
};

inline const ModuleInfo& info (Type t)
{
    static const std::array<ModuleInfo, typeCount> table = []
    {
        std::array<ModuleInfo, typeCount> m;
        m[0] = { "Vide", "", 0xff3a3f4b, {}, {} };
        m[1] = { "Vocal Match", "EQ et compression", 0xffff9a4a,
                 { { "Intensité", 0, 100, 50, "%" }, { "Résonances", 0, 100, 50, "%" }, { "De-esser", 0, 100, 30, "%" } }, {} };
        for (const auto& p : vm::profiles()) m[1].presets.push_back (p.name);
        m[2] = { "VOX", "Chaîne vocale", 0xffaebeff,
                 { { "Clean", 0, 100, 35, "%" }, { "Compress", 0, 100, 25, "%" }, { "De-ess", 0, 100, 25, "%" },
                   { "Air", 0, 100, 25, "%" }, { "Warmth", 0, 100, 25, "%" }, { "Sortie", -12, 12, 0, "dB" } },
                 { "Natural Voice", "Upfront Lead", "Soft & Warm" } };
        m[3] = { "Vocal Lift", "Présence", 0xff6fd3c1, { { "Lift", 0, 100, 40, "%" } }, {} };
        m[4] = { "Ad-lib FX", "Effets créatifs", 0xffc9a0ff,
                 { { "Space", 0, 100, 50, "%" }, { "Dirt", 0, 100, 30, "%" }, { "Pitch", -12, 12, 0, "st" },
                   { "Width", 0, 100, 50, "%" }, { "Mix", 0, 100, 100, "%" } }, {} };
        for (const auto& p : AdLibPresets::all()) m[4].presets.push_back (p.name);
        m[5] = { "Space & Time", "Delay et reverb", 0xfff2b25c,
                 { { "Feedback", 0, 90, 24, "%" }, { "Delay", 0, 100, 17, "%" }, { "Size", 0, 100, 38, "%" },
                   { "Reverb", 0, 100, 22, "%" }, { "Width", 0, 150, 100, "%" }, { "Focus", 0, 100, 65, "%" },
                   { "Ping-pong", 0, 1, 0, "", true } }, {} };
        for (const auto& p : space::presets) m[5].presets.push_back (p.name);
        m[6] = { "Knock", "Saturation", 0xffff6b5a, { { "Saturation", 0, 100, 30, "%" }, { "Attaque", 0, 100, 15, "%" } }, {} };
        m[7] = { "Glue", "Compression de bus", 0xff8fb7ff,
                 { { "Glue", 0, 100, 40, "%" }, { "Mix", 0, 100, 85, "%" }, { "Sortie", -12, 12, 0, "dB" } }, {} };
        for (const auto& p : gluePresets) m[7].presets.push_back (p.name);
        return m;
    }();
    return table[(size_t) juce::jlimit (0, typeCount - 1, (int) t)];
}

/** Knob values (real units) a module preset sets. Modules without presets return their defaults. */
inline std::array<float, maxKnobs> presetKnobs (Type t, int preset)
{
    std::array<float, maxKnobs> k {};
    const auto& specs = info (t).knobs;
    for (size_t i = 0; i < specs.size(); ++i) k[i] = specs[i].def;
    switch (t)
    {
        case Type::vox:
        {
            const float v[3][6] { { 35, 25, 25, 25, 25, 50 }, { 45, 60, 50, 35, 30, 45 }, { 25, 40, 35, 10, 55, 50 } };
            const auto& p = v[juce::jlimit (0, 2, preset)];
            for (int i = 0; i < 5; ++i) k[(size_t) i] = p[i];
            k[5] = (p[5] - 50) * 24 / 100;   // VOX Output: 50 = 0 dB, range -12..+12 dB
            break;
        }
        case Type::adlib:
        {
            const auto& s = AdLibPresets::settingsTable()[(size_t) juce::jlimit (0, (int) AdLibPresets::all().size() - 1, preset)];
            k[0] = s.space * 100; k[1] = s.dirt * 100; k[2] = s.semitones; k[3] = s.width * 100; k[4] = 100;
            break;
        }
        case Type::space:
        {
            const auto& v = space::presets[(size_t) juce::jlimit (0, 19, preset)].values;
            k[0] = v[4]; k[1] = v[5]; k[2] = v[6]; k[3] = v[7]; k[4] = v[9]; k[5] = v[8]; k[6] = 0;
            break;
        }
        case Type::glue:
        {
            const auto& g = gluePresets[(size_t) juce::jlimit (0, 5, preset)];
            k[0] = g.glue; k[1] = g.mix; k[2] = g.output;
            break;
        }
        default: break;
    }
    return k;
}

/** What a module needs to know besides its own knobs. */
struct Context
{
    int preset = 0;
    std::array<float, maxKnobs> knobs {};   // real units
    double bpm = 120;
    int voiceType = 1;
    const vm::Settings* analysis = nullptr; // analysis fields only (analyzed, analysis, markers)
};

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double rate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual int latency() const { return 0; }
    /** Share of the processed signal in the slot's output (a Mix knob); 1 for most modules. */
    virtual float wetAmount (const Context&) const { return 1.0f; }
    virtual void process (float* left, float* right, int n, const Context&) = 0;
};

//==============================================================================
class MatchModule final : public Module
{
public:
    void prepare (double rate, int) override { engine.setSettings (settings ({})); engine.prepare (rate); }
    void reset() override { engine.reset(); }
    void process (float* left, float* right, int n, const Context& c) override
    {
        engine.setSettings (settings (c));
        float* ch[2] { left, right };
        engine.process (ch, 2, n);
    }

private:
    static vm::Settings settings (const Context& c)
    {
        vm::Settings s;
        if (c.analysis != nullptr)
        {
            s.analyzed = c.analysis->analyzed;
            s.analysis = c.analysis->analysis;
            s.markerCount = c.analysis->markerCount;
            s.markerHz = c.analysis->markerHz;
            s.markerDepth = c.analysis->markerDepth;
            s.markerOn = c.analysis->markerOn;
        }
        s.voiceType = c.voiceType;
        s.profile = juce::jlimit (0, 3, c.preset);
        s.intensity = c.knobs[0] / 100;
        s.resonance = c.knobs[1] / 100;
        s.deEss = c.knobs[2] / 100;
        s.autoGain = false;   // the chain has its own Volume auto
        return s;
    }
    vm::Engine engine;
};

//==============================================================================
/** Base of the modules that run 4x oversampled, like the original plugins (VOX, Vocal Lift, Knock, Glue). */
class OversampledModule : public Module
{
public:
    void prepare (double rate, int maxBlock) override
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampling->initProcessing ((size_t) maxBlock);
        prepareAt (rate * 4);
    }
    void reset() override { if (oversampling) oversampling->reset(); resetDsp(); }
    int latency() const override { return oversampling ? (int) oversampling->getLatencyInSamples() : 0; }
    void process (float* left, float* right, int n, const Context& c) override
    {
        float* ch[2] { left, right };
        juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
        auto up = oversampling->processSamplesUp (block);
        begin (c);
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            float frame[2] { up.getSample (0, (int) i), up.getSample (1, (int) i) };
            tick (frame);
            up.setSample (0, (int) i, frame[0]);
            up.setSample (1, (int) i, frame[1]);
        }
        oversampling->processSamplesDown (block);
        end (left, right, n);
    }

protected:
    virtual void prepareAt (double rate) = 0;
    virtual void resetDsp() = 0;
    virtual void begin (const Context&) = 0;     // reads the knobs once per block
    virtual void tick (float* frame) = 0;        // one 4x sample, stereo
    virtual void end (float*, float*, int) {}    // at the host rate, after downsampling
    static void smoothTo (juce::SmoothedValue<float>& s, float v, bool& primed)
    { if (primed) s.setTargetValue (v); else s.setCurrentAndTargetValue (v); }
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
};

class VoxModule final : public OversampledModule
{
    void prepareAt (double rate) override { dsp.prepare (rate); for (auto& s : v) s.reset (rate, 0.03); primed = false; }
    void resetDsp() override { dsp.prepare (rate4()); primed = false; }
    void begin (const Context& c) override
    {
        const float targets[6] { c.knobs[0] / 100, c.knobs[1] / 100, c.knobs[2] / 100, c.knobs[3] / 100, c.knobs[4] / 100,
                                 juce::jlimit (0.f, 1.f, c.knobs[5] / 24 + 0.5f) };
        for (int i = 0; i < 6; ++i) smoothTo (v[(size_t) i], targets[i], primed);
        primed = true;
    }
    void tick (float* frame) override
    {
        const float clean = v[0].getNextValue();
        float rest[5];
        for (int i = 0; i < 5; ++i) rest[i] = v[(size_t) i + 1].getNextValue();
        dsp.tick (frame, 2, clean, rest);
    }
    double rate4() const { return storedRate; }
public:
    void prepare (double rate, int maxBlock) override { storedRate = rate * 4; OversampledModule::prepare (rate, maxBlock); }
private:
    VoxDSP dsp;
    std::array<juce::SmoothedValue<float>, 6> v;
    double storedRate = 192000;
    bool primed = false;
};

class LiftModule final : public OversampledModule
{
    void prepareAt (double rate) override { dsp.prepare (rate); amount.reset (rate, 0.03); primed = false; }
    void resetDsp() override { dsp.reset(); primed = false; }
    void begin (const Context& c) override { smoothTo (amount, c.knobs[0] / 100, primed); primed = true; }
    void tick (float* frame) override { dsp.tick (frame, 2, amount.getNextValue()); }
    LiftDSP dsp;
    juce::SmoothedValue<float> amount;
    bool primed = false;
};

/** Knock for the voice: the saturation and the attack of Swell Knock as two separate controls; no low boost. */
class KnockModule final : public OversampledModule
{
    void prepareAt (double r) override
    {
        auto coeff = [r] (double ms) { return float (std::exp (-1.0 / (ms * 0.001 * r))); };
        fa = coeff (0.3); fr = coeff (18); sa = coeff (25); sr = coeff (100);
        saturation.reset (r, 0.03); attack.reset (r, 0.03);
        resetDsp();
    }
    void resetDsp() override { fast = slow = 0; primed = false; }
    void begin (const Context& c) override
    {
        smoothTo (saturation, c.knobs[0] / 100, primed);
        smoothTo (attack, c.knobs[1] / 100, primed);
        primed = true;
    }
    void tick (float* x) override
    {
        const float sat = saturation.getNextValue(), att = attack.getNextValue();
        const float peak = std::max (std::abs (x[0]), std::abs (x[1]));
        const float f = peak > fast ? fa : fr, s = peak > slow ? sa : sr;
        fast = f * fast + (1 - f) * peak;
        slow = s * slow + (1 - s) * peak;
        const float transient = std::clamp ((fast - slow) / (slow + 0.025f), 0.f, 1.f);
        const float gain = 1 + 1.4f * att * transient;
        const float drive = 1 + 2.6f * sat * sat;
        const float compensation = 1 / std::sqrt (drive);
        for (int c = 0; c < 2; ++c)
        {
            const float shaped = x[c] * gain;
            const float wet = std::tanh (shaped * drive) * compensation;
            x[c] = shaped + (wet - shaped) * sat;
        }
    }
    float fast = 0, slow = 0, fa = 0, fr = 0, sa = 0, sr = 0;
    juce::SmoothedValue<float> saturation, attack;
    bool primed = false;
};

class GlueModule final : public OversampledModule
{
public:
    float wetAmount (const Context& c) const override { return c.knobs[0] <= 0 ? 0.0f : c.knobs[1] / 100; }
private:
    void prepareAt (double r) override
    {
        rate = r;
        dsp.prepare (r, profile);
        amount.reset (r, 0.03);
        primed = false;
    }
    void resetDsp() override { dsp.prepare (rate, profile); primed = false; }
    void begin (const Context& c) override
    {
        if (c.preset != profile) { profile = juce::jlimit (0, 5, c.preset); dsp.setProfile (profile); }
        smoothTo (amount, c.knobs[0] / 100, primed);
        targetGain = juce::Decibels::decibelsToGain (c.knobs[2]);
        primed = true;
    }
    void tick (float* frame) override { dsp.tick (frame, 2, amount.getNextValue()); }
    void end (float* left, float* right, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            gain += 0.002f * (targetGain - gain);
            left[i] *= gain; right[i] *= gain;
        }
    }
    GlueDSP dsp;
    juce::SmoothedValue<float> amount;
    double rate = 192000;
    int profile = 0;
    float gain = 1, targetGain = 1;
    bool primed = false;
};

//==============================================================================
class AdLibModule final : public Module
{
public:
    void prepare (double rate, int) override { engine.prepare (rate); }
    void reset() override { engine.discardTails(); }
    float wetAmount (const Context& c) const override { return c.knobs[4] / 100; }
    void process (float* left, float* right, int n, const Context& c) override
    {
        const auto& table = AdLibPresets::settingsTable();
        const auto& base = table[(size_t) juce::jlimit (0, (int) table.size() - 1, c.preset)];
        AdLib::Settings s = base;
        s.space = c.knobs[0] / 100;
        s.dirt = c.knobs[1] / 100;
        s.semitones = c.knobs[2];
        s.width = c.knobs[3] / 100;
        s.bpm = c.bpm;
        // Like the macros of the plugin: turning one up switches on the effect it drives.
        if (std::abs (s.dirt - base.dirt) > 0.005f && s.dirt > 0.005f) s.colorOn = true;
        if (std::abs (s.semitones - base.semitones) > 0.01f && std::abs (s.semitones) > 0.01f) s.pitchOn = true;
        if (! s.delayOn && ! s.reverbOn && std::abs (s.space - base.space) > 0.005f) s.reverbOn = true;
        engine.process (left, right, n, s);
    }

private:
    AdLib::Engine engine;
};

class SpaceModule final : public Module
{
public:
    void prepare (double rate, int) override { engine.prepare (rate); }
    void reset() override { engine.reset(); }
    void process (float* left, float* right, int n, const Context& c) override
    {
        auto v = space::presets[(size_t) juce::jlimit (0, 19, c.preset)].values;
        v[4] = c.knobs[0]; v[5] = c.knobs[1]; v[6] = c.knobs[2]; v[7] = c.knobs[3];
        v[9] = v[12] = c.knobs[4]; v[8] = c.knobs[5];
        engine.process (left, right, n, v, c.knobs[6] > 0.5f ? 1.0f : 0.0f, c.bpm);
    }

private:
    space::Engine engine;
};

inline std::unique_ptr<Module> create (Type t)
{
    switch (t)
    {
        case Type::match: return std::make_unique<MatchModule>();
        case Type::vox:   return std::make_unique<VoxModule>();
        case Type::lift:  return std::make_unique<LiftModule>();
        case Type::adlib: return std::make_unique<AdLibModule>();
        case Type::space: return std::make_unique<SpaceModule>();
        case Type::knock: return std::make_unique<KnockModule>();
        case Type::glue:  return std::make_unique<GlueModule>();
        default: return nullptr;
    }
}
} // namespace chain
