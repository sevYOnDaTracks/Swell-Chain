#pragma once
// What the chain-wide analysis measures on the recorded voice, and the settings it suggests.
// Level and dynamics come from 20 ms frames with sound; sibilance compares the energy above 5 kHz
// with the whole signal on the loudest sibilant frames. The mapping is a starting point to validate by ear.
#include "dsp/DSP.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace chain
{
struct VoiceStats
{
    bool valid = false;
    float levelDb = -18;      // average level of the frames with sound, dBFS
    float dynamicsDb = 12;    // spread between loud and quiet frames (90th - 10th percentile)
    float sibilanceDb = -20;  // energy above 5 kHz relative to the whole, on sibilant frames
};

inline VoiceStats measureVoice (const std::vector<float>& x, double rate)
{
    VoiceStats s;
    const size_t frame = (size_t) std::max (64.0, rate * 0.02);
    if (x.size() < frame * 10) return s;
    vm::Biquad hp1, hp2;
    hp1.c = vm::Coefficients::highPass (rate, 5000);
    hp2.c = hp1.c;
    std::vector<float> full, high;
    for (size_t start = 0; start + frame <= x.size(); start += frame)
    {
        double e = 0, h = 0;
        for (size_t i = start; i < start + frame; ++i)
        {
            const float v = x[i];
            const float y = hp2.process (hp1.process (v, 0), 0);
            e += (double) v * v;
            h += (double) y * y;
        }
        full.push_back ((float) (10 * std::log10 (e / (double) frame + 1e-12)));
        high.push_back ((float) (10 * std::log10 (h / (double) frame + 1e-12)));
    }
    const float loudest = *std::max_element (full.begin(), full.end());
    std::vector<float> active, ratios;
    double power = 0;
    for (size_t i = 0; i < full.size(); ++i)
        if (full[i] > -55 && full[i] > loudest - 35)
        {
            active.push_back (full[i]);
            ratios.push_back (high[i] - full[i]);
            power += std::pow (10.0, full[i] / 10.0);
        }
    if (active.size() < 10) return s;
    auto percentile = [] (std::vector<float> v, double p)
    {
        std::sort (v.begin(), v.end());
        return v[(size_t) std::clamp (p * (double) (v.size() - 1), 0.0, (double) (v.size() - 1))];
    };
    s.levelDb = (float) (10 * std::log10 (power / (double) active.size()));
    s.dynamicsDb = percentile (active, 0.9) - percentile (active, 0.1);
    s.sibilanceDb = percentile (ratios, 0.95);
    s.valid = true;
    return s;
}

struct VoiceSettings
{
    float inputDb = 0, compress = 25, deEss = 25;   // input trim; VOX Compress and De-ess, Vocal Match De-esser (0..100)
};

inline VoiceSettings suggest (const VoiceStats& s)
{
    VoiceSettings v;
    if (! s.valid) return v;
    // Bring the voice near -18 dBFS before the chain, the level the modules are tuned for.
    v.inputDb = std::round (std::clamp (-18.0f - s.levelDb, -12.0f, 12.0f) * 2) / 2;
    // A voice with a wide spread between loud and quiet passages gets more compression.
    v.compress = std::round (std::clamp (20.0f + (s.dynamicsDb - 8.0f) * 4.0f, 15.0f, 70.0f));
    // Strong sibilants get more de-essing.
    v.deEss = std::round (std::clamp (20.0f + (s.sibilanceDb + 24.0f) * 3.5f, 10.0f, 70.0f));
    return v;
}

inline std::vector<std::string> describe (const VoiceStats& s, const VoiceSettings& v)
{
    std::vector<std::string> tags;
    if (! s.valid) return tags;
    char text[64];
    std::snprintf (text, sizeof text, "Entrée %+.1f dB", (double) v.inputDb);
    std::string t (text);
    std::replace (t.begin(), t.end(), '.', ',');
    tags.push_back (t);
    // Only what stands out, to keep the row short.
    if (s.dynamicsDb > 16) tags.push_back ("Dynamique forte");
    if (s.dynamicsDb < 9) tags.push_back ("Dynamique serrée");
    if (s.sibilanceDb > -16) tags.push_back ("Sifflantes marquées");
    if (s.sibilanceDb < -26) tags.push_back ("Sifflantes discrètes");
    return tags;
}
} // namespace chain
