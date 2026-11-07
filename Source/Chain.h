#pragma once
// The chain: six slots in series. Every slot delays its signal by the same fixed latency, so the
// total latency never changes when modules are added, removed, switched off or moved.
#include "Modules.h"

namespace chain
{
constexpr int slotCount = 6;

struct SlotState
{
    Type type = Type::empty;
    bool on = true;
    int preset = 0;
    std::array<float, maxKnobs> knobs {};   // real units
};

struct ChainSettings
{
    std::array<SlotState, slotCount> slots;
    int solo = -1;            // slot heard alone, -1: none
    float mix = 1;            // 0..1
    float outputDb = 0;
    float inputDb = 0;        // trim before the chain, set by the analysis
    bool autoGain = true;
    bool bypass = false;
    int voiceType = 1;
    double bpm = 120;
    vm::Settings analysis;    // analysis fields only
};

/** A fixed delay of a few samples, per channel. */
class Delay
{
public:
    void prepare (int samples) { length = samples; for (auto& b : buffer) b.assign ((size_t) std::max (1, samples), 0.0f); pos = 0; }
    void reset() { for (auto& b : buffer) std::fill (b.begin(), b.end(), 0.0f); pos = 0; }
    void process (float* left, float* right, int n)
    {
        if (length <= 0) return;
        for (int i = 0; i < n; ++i)
        {
            std::swap (left[i], buffer[0][(size_t) pos]);
            std::swap (right[i], buffer[1][(size_t) pos]);
            pos = (pos + 1) % length;
        }
    }
private:
    std::array<std::vector<float>, 2> buffer;
    int length = 0, pos = 0;
};

class Slot
{
public:
    Slot()
    {
        for (int t = 1; t < typeCount; ++t) modules[(size_t) t] = create ((Type) t);
    }

    int maxLatency() const
    {
        int l = 0;
        for (auto& m : modules) if (m) l = std::max (l, m->latency());
        return l;
    }

    void prepare (double rate, int maxBlock)
    {
        for (auto& m : modules) if (m) m->prepare (rate, maxBlock);
        block = maxBlock;
        dryL.assign ((size_t) maxBlock, 0.0f); dryR.assign ((size_t) maxBlock, 0.0f);
        wet.reset (rate, 0.02);
        wet.setCurrentAndTargetValue (0);
        current = Type::empty;
    }

    void setLatency (int total)
    {
        slotLatency = total;
        dryDelay.prepare (total);
        for (int t = 1; t < typeCount; ++t)
            padding[(size_t) t].prepare (total - modules[(size_t) t]->latency());
    }

    void reset()
    {
        for (auto& m : modules) if (m) m->reset();
        for (auto& p : padding) p.reset();
        dryDelay.reset();
        wet.setCurrentAndTargetValue (0);
        current = Type::empty;
    }

    /** Processes up to maxBlock samples in place. active: the slot is on and not muted by another slot's solo. */
    void process (float* left, float* right, int n, const SlotState& s, bool active, const Context& context)
    {
        if (s.type != current)
        {
            // A new module starts from silence; the previous one stops at once.
            current = s.type;
            if (current != Type::empty) { modules[(size_t) current]->reset(); padding[(size_t) current].reset(); }
            wet.setCurrentAndTargetValue (0);
        }
        auto* module = current == Type::empty ? nullptr : modules[(size_t) current].get();
        const float target = module != nullptr && active ? module->wetAmount (context) : 0.0f;
        wet.setTargetValue (target);

        std::copy (left, left + n, dryL.begin());
        std::copy (right, right + n, dryR.begin());
        dryDelay.process (dryL.data(), dryR.data(), n);

        if (module == nullptr || (! wet.isSmoothing() && wet.getTargetValue() <= 0))
        {
            // Off: the delayed dry signal keeps the latency constant. The module restarts clean when switched back on.
            std::copy (dryL.begin(), dryL.begin() + n, left);
            std::copy (dryR.begin(), dryR.begin() + n, right);
            if (module != nullptr && running) { module->reset(); padding[(size_t) current].reset(); }
            running = false;
            return;
        }
        running = true;
        module->process (left, right, n, context);
        padding[(size_t) current].process (left, right, n);
        for (int i = 0; i < n; ++i)
        {
            const float w = wet.getNextValue();
            left[i] = dryL[(size_t) i] + (left[i] - dryL[(size_t) i]) * w;
            right[i] = dryR[(size_t) i] + (right[i] - dryR[(size_t) i]) * w;
        }
    }

private:
    std::array<std::unique_ptr<Module>, typeCount> modules;
    std::array<Delay, typeCount> padding;
    Delay dryDelay;
    std::vector<float> dryL, dryR;
    juce::SmoothedValue<float> wet;
    Type current = Type::empty;
    int slotLatency = 0, block = 512;
    bool running = false;
};

class Engine
{
public:
    void prepare (double rate, int maxBlock)
    {
        sampleRate = rate;
        block = std::max (32, maxBlock);
        perSlot = 0;
        for (auto& s : slots) { s.prepare (rate, block); perSlot = std::max (perSlot, s.maxLatency()); }
        for (auto& s : slots) s.setLatency (perSlot);
        inputDelay.prepare (perSlot * slotCount);
        dryL.assign ((size_t) block, 0.0f); dryR.assign ((size_t) block, 0.0f);
        mix.reset (rate, 0.03); gain.reset (rate, 0.05); bypassFade.reset (rate, 0.02); input.reset (rate, 0.05);
        primed = false;
        inEnergy = outEnergy = 0; autoDb = 0;
        energyCoeff = (float) std::exp (-1.0 / (1.5 * rate));
    }

    int latency() const { return perSlot * slotCount; }

    void reset()
    {
        for (auto& s : slots) s.reset();
        inputDelay.reset();
        inEnergy = outEnergy = 0; autoDb = 0;
    }

    void process (float* left, float* right, int numSamples, const ChainSettings& c)
    {
        if (! primed)
        {
            mix.setCurrentAndTargetValue (c.mix);
            gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (c.outputDb));
            bypassFade.setCurrentAndTargetValue (c.bypass ? 1.0f : 0.0f);
            input.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (c.inputDb));
            primed = true;
        }
        mix.setTargetValue (c.mix);
        bypassFade.setTargetValue (c.bypass ? 1.0f : 0.0f);
        input.setTargetValue (juce::Decibels::decibelsToGain (c.inputDb));
        for (int start = 0; start < numSamples; start += block)
        {
            const int n = std::min (block, numSamples - start);
            float* l = left + start;
            float* r = right + start;
            float inPeak = 0, outPeak = 0;
            for (int i = 0; i < n; ++i) inPeak = std::max ({ inPeak, std::abs (l[i]), std::abs (r[i]) });
            std::copy (l, l + n, dryL.begin());
            std::copy (r, r + n, dryR.begin());
            inputDelay.process (dryL.data(), dryR.data(), n);
            for (int i = 0; i < n; ++i) { const float g = input.getNextValue(); l[i] *= g; r[i] *= g; }

            for (int s = 0; s < slotCount; ++s)
            {
                const auto& slot = c.slots[(size_t) s];
                Context context;
                context.preset = slot.preset;
                context.knobs = slot.knobs;
                context.bpm = c.bpm;
                context.voiceType = c.voiceType;
                context.analysis = &c.analysis;
                const bool active = slot.on && (c.solo < 0 || c.solo == s);
                slots[(size_t) s].process (l, r, n, slot, active, context);
            }

            // Volume auto: the processed level follows the input level, slowly, while there is sound.
            for (int i = 0; i < n; ++i)
            {
                const float m = mix.getNextValue();
                l[i] = dryL[(size_t) i] + (l[i] - dryL[(size_t) i]) * m;
                r[i] = dryR[(size_t) i] + (r[i] - dryR[(size_t) i]) * m;
                const float xin = 0.5f * (dryL[(size_t) i] * dryL[(size_t) i] + dryR[(size_t) i] * dryR[(size_t) i]);
                const float xout = 0.5f * (l[i] * l[i] + r[i] * r[i]);
                inEnergy = energyCoeff * inEnergy + (1 - energyCoeff) * xin;
                outEnergy = energyCoeff * outEnergy + (1 - energyCoeff) * xout;
            }
            if (c.autoGain && inEnergy > 1.0e-7f && outEnergy > 1.0e-9f)
            {
                const float wanted = juce::jlimit (-24.0f, 12.0f, 10.0f * std::log10 (inEnergy / outEnergy));
                autoDb += 0.02f * (wanted - autoDb);
            }
            else if (! c.autoGain)
                autoDb += 0.05f * (0.0f - autoDb);
            gain.setTargetValue (juce::Decibels::decibelsToGain (c.outputDb + autoDb));

            for (int i = 0; i < n; ++i)
            {
                const float g = gain.getNextValue(), b = bypassFade.getNextValue();
                l[i] = protect (l[i] * g) * (1 - b) + dryL[(size_t) i] * b;
                r[i] = protect (r[i] * g) * (1 - b) + dryR[(size_t) i] * b;
                outPeak = std::max ({ outPeak, std::abs (l[i]), std::abs (r[i]) });
            }
            inputPeak = std::max (inputPeak.load(), inPeak);
            outputPeak = std::max (outputPeak.load(), outPeak);
        }
    }

    /** Output protection: untouched below -1 dBFS, then a soft curve that never goes past 0 dBFS. */
    static float protect (float x)
    {
        constexpr float t = 0.891f;   // -1 dBFS
        const float a = std::abs (x);
        if (a <= t) return x;
        return std::copysign (t + (1 - t) * std::tanh ((a - t) / (1 - t)), x);
    }

    float autoGainDb() const { return autoDb; }
    std::atomic<float> inputPeak { 0 }, outputPeak { 0 };

private:
    std::array<Slot, slotCount> slots;
    Delay inputDelay;
    std::vector<float> dryL, dryR;
    juce::SmoothedValue<float> mix, gain, bypassFade, input;
    double sampleRate = 48000;
    int block = 512, perSlot = 0;
    float inEnergy = 0, outEnergy = 0, autoDb = 0, energyCoeff = 0.9999f;
    bool primed = false;
};
} // namespace chain
