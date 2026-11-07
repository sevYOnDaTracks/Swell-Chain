// Engine checks for Swell Chain: neutrality, constant latency, stability of every module and chain preset.
#include <JuceHeader.h>
#include "../Source/ChainPresets.h"
#include <cstdio>
#include <random>

using namespace chain;

static int failures = 0;
static void check (bool ok, const char* what, double value = 0)
{
    std::printf ("%s  %s (%.3f)\n", ok ? "PASS" : "FAIL", what, value);
    if (! ok) ++failures;
}

/** A voice-like test signal: a pitched tone with harmonics, syllables and some noise. */
static std::vector<float> voice (double rate, double seconds, unsigned seed = 3)
{
    std::mt19937 gen (seed);
    std::normal_distribution<float> noise (0, 0.02f);
    std::vector<float> x ((size_t) (rate * seconds));
    double phase = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / rate;
        const double f0 = 180 + 30 * std::sin (2 * juce::MathConstants<double>::pi * 0.7 * t);
        phase += 2 * juce::MathConstants<double>::pi * f0 / rate;
        double s = 0;
        for (int h = 1; h <= 8; ++h) s += std::sin (phase * h) / h;
        const double syllable = std::max (0.0, std::sin (2 * juce::MathConstants<double>::pi * 2.5 * t));
        x[i] = (float) (0.12 * s * syllable) + noise (gen) * (float) syllable;
    }
    return x;
}

struct Out { std::vector<float> left, right; };

static Out run (Engine& e, const std::vector<float>& in, const ChainSettings& c, int block = 512)
{
    Out r { in, in };
    for (size_t start = 0; start < in.size(); start += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - start);
        e.process (r.left.data() + start, r.right.data() + start, n, c);
    }
    return r;
}

static double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    double e = 0;
    for (size_t i = from; i < to; ++i) e += (double) x[i] * x[i];
    return 10 * std::log10 (e / (double) std::max<size_t> (1, to - from) + 1e-30);
}

static bool finite (const Out& r)
{
    for (size_t i = 0; i < r.left.size(); ++i)
        if (! std::isfinite (r.left[i]) || ! std::isfinite (r.right[i])) return false;
    return true;
}

static float peak (const Out& r)
{
    float p = 0;
    for (size_t i = 0; i < r.left.size(); ++i) p = std::max ({ p, std::abs (r.left[i]), std::abs (r.right[i]) });
    return p;
}

int main()
{

    // 1. An empty chain is a pure delay of exactly its reported latency.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Engine e; e.prepare (rate, 512);
        ChainSettings c; c.autoGain = false;
        std::vector<float> impulse ((size_t) rate / 10, 0.0f); impulse[100] = 0.5f;
        auto r = run (e, impulse, c);
        size_t at = 0; for (size_t i = 0; i < r.left.size(); ++i) if (std::abs (r.left[i]) > 0.25f) at = i;
        char label[96]; std::snprintf (label, sizeof label, "chaîne vide = retard de %d échantillons à %.0f Hz", e.latency(), rate);
        check ((int) at - 100 == e.latency() && std::abs (r.left[at] - 0.5f) < 1e-6f, label, (double) at - 100);
    }

    // 2. Each module alone, at its defaults: finite, stable, not silent, and the latency does not move.
    for (int t = 1; t < typeCount; ++t)
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e; e.prepare (rate, 480);
            const int latency = e.latency();
            ChainSettings c; c.autoGain = false;
            c.slots[2].type = (Type) t;
            c.slots[2].knobs = presetKnobs ((Type) t, 0);
            auto in = voice (rate, 3.0);
            auto r = run (e, in, c, 480);
            const double level = rmsDb (r.left, in.size() / 2, in.size());
            const double growth = rmsDb (r.left, in.size() * 3 / 4, in.size()) - rmsDb (r.left, in.size() / 2, in.size() * 3 / 4);
            char label[128]; std::snprintf (label, sizeof label, "%s seul à %.0f Hz (niveau dB)", info ((Type) t).name, rate);
            check (finite (r) && level > -40 && std::abs (growth) < 3 && peak (r) < 4 && e.latency() == latency, label, level);
        }

    // 3. Every chain preset: finite, bounded, no runaway, Volume auto close to the input level.
    for (const auto& p : chainPresets())
    {
        Engine e; e.prepare (48000, 512);
        ChainSettings c; c.autoGain = true;
        c.slots = slotsFor (p);
        auto in = voice (48000, 8.0, 11);
        auto r = run (e, in, c);
        const double gap = rmsDb (r.left, in.size() * 5 / 8, in.size()) - rmsDb (in, in.size() * 5 / 8, in.size());
        char label[128]; std::snprintf (label, sizeof label, "preset %s : stable, écart de niveau (dB)", p.name);
        check (finite (r) && peak (r) < 4 && std::abs (gap) < 3, label, gap);
    }

    // 4. Silence stays silent through the fullest chains.
    for (const auto& p : chainPresets())
    {
        Engine e; e.prepare (48000, 512);
        ChainSettings c; c.slots = slotsFor (p);
        auto r = run (e, std::vector<float> (48000, 0.0f), c);
        if (peak (r) > 1e-5f) { check (false, p.name, peak (r)); }
    }
    check (true, "silence : tous les presets restent silencieux (sinon listé au-dessus)");

    // 5. Switching every slot off brings back the dry signal, delayed.
    {
        Engine e; e.prepare (48000, 512);
        ChainSettings c; c.autoGain = false;
        c.slots = slotsFor (chainPresets()[0]);
        auto in = voice (48000, 2.0);
        run (e, in, c);
        for (auto& s : c.slots) s.on = false;
        auto r = run (e, in, c);
        double err = 0;
        const size_t d = (size_t) e.latency();
        for (size_t i = 24000; i < in.size(); ++i) err = std::max (err, (double) std::abs (r.left[i] - in[i - d]));
        check (err < 1e-5, "tout coupé : signal d'origine retardé (écart max)", err);
    }

    // 6. Bypass returns the dry signal, delayed.
    {
        Engine e; e.prepare (48000, 512);
        ChainSettings c; c.slots = slotsFor (chainPresets()[6]); c.bypass = true;
        auto in = voice (48000, 1.0);
        auto r = run (e, in, c);
        double err = 0;
        const size_t d = (size_t) e.latency();
        for (size_t i = 4800; i < in.size(); ++i) err = std::max (err, (double) std::abs (r.left[i] - in[i - d]));
        check (err < 1e-5, "bypass : signal d'origine retardé (écart max)", err);
    }

    // 7. Changing presets and moving modules while audio runs stays finite.
    {
        Engine e; e.prepare (44100, 256);
        ChainSettings c;
        auto in = voice (44100, 0.5, 5);
        bool ok = true;
        for (size_t i = 0; i < chainPresets().size(); ++i)
        {
            c.slots = slotsFor (chainPresets()[i]);
            if (i % 3 == 0) std::rotate (c.slots.begin(), c.slots.begin() + 1, c.slots.end());
            ok = ok && finite (run (e, in, c, 256));
        }
        check (ok, "changements de presets et de place en cours de lecture");
    }

    std::printf (failures ? "\n%d test(s) en échec\n" : "\nTous les tests passent\n", failures);
    return failures ? 1 : 0;
}
