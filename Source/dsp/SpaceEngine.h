#pragma once
// The Swell Space and Time engine (synced delay, ping-pong, reverb, Focus ducking), taken out of
// SpaceProcessor::render so it can run inside a chain. Same algorithm, same parameter order as SpacePresets.h.
#include "SpacePresets.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace space
{
class Engine
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;
        for (auto& v : delayLine) v.assign ((size_t) (rate * 12.1) + 4, 0.0f);
        for (auto& v : preLine) v.assign ((size_t) (rate * 0.16) + 4, 0.0f);
        reverbEngine.setSampleRate (rate);
        for (auto& s : smooth) s.reset (rate, 0.04);
        pingBlend.reset (rate, 0.04);
        primed = false;
        reset();
    }

    void reset()
    {
        for (auto& v : delayLine) std::fill (v.begin(), v.end(), 0.0f);
        for (auto& v : preLine) std::fill (v.begin(), v.end(), 0.0f);
        position = prePosition = 0;
        env = 0; duck = 1;
        delayFilter = {}; reverbFilter = {};
        reverbEngine.reset();
        crossfade = 1;
        primed = false;
    }

    /** values: the 17 parameters in SpacePresets order; ping: 0 or 1; bpm: host tempo. Processes stereo in place. */
    void process (float* left, float* right, int numSamples, const std::array<float, parameterCount>& values, float ping, double bpm)
    {
        if (delayLine[0].empty()) return;
        if (! primed)
        {
            for (int i = 0; i < parameterCount; ++i) smooth[(size_t) i].setCurrentAndTargetValue (values[(size_t) i]);
            pingBlend.setCurrentAndTargetValue (ping);
            delaySamples = oldDelay = targetDelay (values, bpm);
            primed = true;
        }
        for (int i = 0; i < parameterCount; ++i) smooth[(size_t) i].setTargetValue (values[(size_t) i]);
        pingBlend.setTargetValue (ping);

        const float target = targetDelay (values, bpm);
        if (crossfade >= 1 && std::abs (target - delaySamples) > 0.5f) { oldDelay = delaySamples; delaySamples = target; crossfade = 0; }

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.12f + smooth[6].getCurrentValue() * 0.0082f;
        rp.damping = 0.45f; rp.wetLevel = 1; rp.dryLevel = 0; rp.width = 1; rp.freezeMode = 0;
        reverbEngine.setParameters (rp);

        const float attack = (float) std::exp (-1 / (sampleRate * 0.003)), release = (float) std::exp (-1 / (sampleRate * 0.22));
        const int len = (int) delayLine[0].size(), plen = (int) preLine[0].size();
        auto tap = [] (const std::vector<float>& line, int pos, float distance)
        {
            float read = (float) pos - distance;
            while (read < 0) read += (float) line.size();
            const int a = (int) read;
            const float f = read - (float) a;
            return line[(size_t) a] * (1 - f) + line[(size_t) ((a + 1) % (int) line.size())] * f;
        };
        auto widen = [] (float& l, float& r, float width)
        {
            const float m = (l + r) * 0.5f, s = (l - r) * 0.5f * width;
            const float norm = std::max (1.0f, width);
            l = (m + s) / norm; r = (m - s) / norm;
        };
        auto coefficient = [this] (float hz)
        { return (float) std::exp (-juce::MathConstants<double>::twoPi * std::min ((double) hz, sampleRate * 0.45) / sampleRate); };
        const float dh = coefficient (smooth[10].getCurrentValue()), dl = coefficient (smooth[11].getCurrentValue());
        const float rh = coefficient (smooth[13].getCurrentValue()), rl = coefficient (smooth[14].getCurrentValue());

        for (int n = 0; n < numSamples; ++n)
        {
            std::array<float, parameterCount> v;
            for (int k = 0; k < parameterCount; ++k) v[(size_t) k] = smooth[(size_t) k].getNextValue();
            const float dry[2] = { left[n], right[n] };
            const float peak = std::max (std::abs (dry[0]), std::abs (dry[1]));
            env = std::max (peak, env * release);
            const float strength = juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (env, -100.f) + 48.f) / 36.f);
            const float duckTarget = juce::Decibels::decibelsToGain (-24.f * v[8] / 100 * strength);
            const float coeff = duckTarget < duck ? attack : release;
            duck = duckTarget + (duck - duckTarget) * coeff;

            float echo[2];
            for (int c = 0; c < 2; ++c)
                echo[c] = delayFilter[(size_t) c].tick (tap (delayLine[(size_t) c], position, oldDelay) * (1 - crossfade)
                                                            + tap (delayLine[(size_t) c], position, delaySamples) * crossfade, dh, dl);
            // Read both taps before writing either channel. In ping-pong mode a mono send
            // starts on the left, then each repeat feeds the opposite delay line.
            const float pingNow = pingBlend.getNextValue(), mono = (dry[0] + dry[1]) * 0.5f;
            for (int c = 0; c < 2; ++c)
            {
                const float input = dry[c] * (1 - pingNow) + (c == 0 ? mono : 0.f) * pingNow;
                const float feedback = echo[c] * (1 - pingNow) + echo[1 - c] * pingNow;
                delayLine[(size_t) c][(size_t) position] = (input + feedback * v[4] / 100) * v[0];
            }
            crossfade = std::min (1.f, crossfade + (float) (1 / (sampleRate * 0.05)));
            position = (position + 1) % len;

            // Parallel dry signal; a modest delay send also enters the reverb.
            float rv[2];
            for (int c = 0; c < 2; ++c)
            {
                preLine[(size_t) c][(size_t) prePosition] = (dry[c] + echo[c] * (v[5] / 100) * 0.25f * v[0]) * v[1];
                rv[c] = tap (preLine[(size_t) c], prePosition, (float) (v[15] * sampleRate * 0.001));
            }
            prePosition = (prePosition + 1) % plen;
            reverbEngine.processStereo (&rv[0], &rv[1], 1);
            for (int c = 0; c < 2; ++c) rv[c] = reverbFilter[(size_t) c].tick (rv[c], rh, rl);
            widen (echo[0], echo[1], v[9] / 100);
            widen (rv[0], rv[1], v[12] / 100);
            left[n] = dry[0] + duck * (echo[0] * v[5] / 100 * v[0] + rv[0] * v[7] / 100 * v[1]);
            right[n] = dry[1] + duck * (echo[1] * v[5] / 100 * v[0] + rv[1] * v[7] / 100 * v[1]);
        }
    }

private:
    struct Filter
    {
        float low = 0, high = 0;
        float tick (float x, float hp, float lp) { low = (1 - hp) * x + hp * low; const float y = x - low; high = (1 - lp) * y + lp * high; return high; }
    };

    float targetDelay (const std::array<float, parameterCount>& v, double bpm) const
    {
        const float beats[] = { 4, 2, 1, 0.5f, 0.25f };
        const float feel[] = { 1, 1.5f, 2.f / 3.f };
        const double tempo = juce::jlimit (30.0, 300.0, bpm);
        return (float) (sampleRate * 60 / tempo) * beats[juce::jlimit (0, 4, (int) v[2])] * feel[juce::jlimit (0, 2, (int) v[3])];
    }

    double sampleRate = 44100;
    std::array<std::vector<float>, 2> delayLine, preLine;
    std::array<Filter, 2> delayFilter, reverbFilter;
    std::array<juce::SmoothedValue<float>, parameterCount> smooth;
    juce::SmoothedValue<float> pingBlend;
    juce::Reverb reverbEngine;
    int position = 0, prePosition = 0;
    float delaySamples = 22050, oldDelay = 22050, crossfade = 1, env = 0, duck = 1;
    bool primed = false;
};
} // namespace space
