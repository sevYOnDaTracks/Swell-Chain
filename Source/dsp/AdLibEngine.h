#pragma once

#include <juce_dsp/juce_dsp.h>

// The Ad-lib FX signal chain: Clean -> Punch -> Pitch -> Color -> Mod -> Flange -> Delay -> Reverb -> Width.
// Every module works on a stereo block in place; the engine feeds them sub-blocks of at most maxBlock samples.
namespace AdLib
{
constexpr int maxBlock = 1024;

struct Settings
{
    bool cleanOn = true;   int cleanMode = 0;  float lowCut = 150.0f, highCut = 16000.0f, deess = 0.3f;
    bool punchOn = true;   int punchMode = 1;  float punchAmount = 0.5f, punchAttackMs = 5.0f, punchMakeupDb = 0.0f;
    bool pitchOn = false;  float semitones = 0.0f, pitchMix = 1.0f, fineCents = 0.0f;
    bool colorOn = false;  int colorMode = 0;  float dirt = 0.3f, colorTone = 0.6f, colorMix = 1.0f;
    bool modOn = false;    int modMode = 0;    float modDepth = 0.5f, modRate = 0.6f, modMix = 0.5f;
    bool flangeOn = false; int flangeMode = 0;
    float flangeDepth = 0.55f, flangeRate = 0.35f, flangeMix = 0.55f, flangeFeedback = 0.4f;
    bool delayOn = false;  int delayMode = 0;  float delayFeedback = 0.35f, delayTone = 0.6f, delayMix = 0.35f;
    bool throwOn = false;
    bool reverbOn = false; int reverbMode = 1; float reverbSize = 0.5f, preDelayMs = 20.0f, reverbMix = 0.35f;
    float space = 0.5f, width = 0.5f;
    float delayDuck = 0.0f, reverbDuck = 0.0f;
    double bpm = 120.0;
};

using SVF = juce::dsp::StateVariableTPTFilter<float>;

inline float onePole (double sampleRate, double seconds)
{
    return (float) std::exp (-1.0 / (juce::jmax (1.0e-5, seconds) * sampleRate));
}

inline float follow (float env, float in, float attack, float release)
{
    const float c = in > env ? attack : release;
    return c * env + (1.0f - c) * in;
}

inline void prepareFilter (SVF& f, double sr, SVF::Type type)
{
    f.prepare ({ sr, (juce::uint32) maxBlock, 2 });
    f.setType (type);
    f.reset();
}

inline void setCutoff (SVF& f, double sr, float hz)
{
    f.setCutoffFrequency (juce::jlimit (20.0f, (float) (sr * 0.45), hz));
}

// Reads a circular buffer `d` samples behind the write head, with linear interpolation
inline float readFrac (const std::vector<float>& buf, int writePos, float d)
{
    const int size = (int) buf.size();
    float pos = (float) writePos - d;
    while (pos < 0.0f) pos += (float) size;
    const int i0 = (int) pos;
    const float frac = pos - (float) i0;
    const float a = buf[(size_t) (i0 % size)];
    const float b = buf[(size_t) ((i0 + 1) % size)];
    return a + frac * (b - a);
}

//==============================================================================
// 01 CLEAN: low cut / high cut + de-esser
class Clean
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        prepareFilter (hp, sr, SVF::Type::highpass);
        prepareFilter (lp, sr, SVF::Type::lowpass);
        prepareFilter (split, sr, SVF::Type::highpass);
        setCutoff (split, sr, 5500.0f);
        att = onePole (sr, 0.002);
        rel = onePole (sr, 0.06);
        hiEnv = fullEnv = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        setCutoff (hp, sr, s.lowCut);
        setCutoff (lp, sr, s.highCut);
        const bool useHp = s.cleanMode != 1;  // 0 = High-pass, 1 = Low-pass, 2 = Band-pass
        const bool useLp = s.cleanMode != 0;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (useHp) { l = hp.processSample (0, l); r = hp.processSample (1, r); }
            if (useLp) { l = lp.processSample (0, l); r = lp.processSample (1, r); }

            if (s.deess > 0.001f)
            {
                const float hl = split.processSample (0, l), hr = split.processSample (1, r);
                hiEnv = follow (hiEnv, juce::jmax (std::abs (hl), std::abs (hr)), att, rel);
                fullEnv = follow (fullEnv, juce::jmax (std::abs (l), std::abs (r)), att, rel);
                const float ratio = hiEnv / (fullEnv + 1.0e-6f);
                const float g = ratio > 0.3f ? std::pow (0.3f / ratio, s.deess * 2.0f) : 1.0f;
                l -= hl * (1.0f - g);
                r -= hr * (1.0f - g);
            }
            L[i] = l; R[i] = r;
        }
    }

private:
    double sr = 44100.0;
    SVF hp, lp, split;
    float att = 0, rel = 0, hiEnv = 0, fullEnv = 0;
};

//==============================================================================
// 02 PUNCH: stereo-linked compressor (Smooth / Punchy / Smash)
class Punch
{
public:
    void prepare (double sampleRate) { sr = sampleRate; env = 0.0f; }

    void process (float* L, float* R, int n, const Settings& s)
    {
        static constexpr float ratios[]   = { 2.0f, 4.0f, 10.0f };
        static constexpr float releases[] = { 0.15f, 0.08f, 0.05f };
        const int mode = juce::jlimit (0, 2, s.punchMode);
        const float ratio = ratios[mode];
        const float thr = -30.0f * s.punchAmount;
        const float att = onePole (sr, s.punchAttackMs * 0.001);
        const float rel = onePole (sr, releases[mode]);
        const float makeup = juce::Decibels::decibelsToGain (s.punchMakeupDb - thr * (1.0f - 1.0f / ratio) * 0.5f);

        for (int i = 0; i < n; ++i)
        {
            env = follow (env, juce::jmax (std::abs (L[i]), std::abs (R[i])), att, rel);
            const float over = juce::Decibels::gainToDecibels (env, -100.0f) - thr;
            const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            const float g = juce::Decibels::decibelsToGain (-gr) * makeup;
            L[i] *= g; R[i] *= g;
        }
    }

private:
    double sr = 44100.0;
    float env = 0.0f;
};

//==============================================================================
// 03 PITCH: two-tap granular pitch shifter (octave down for demon ad-libs, up for chipmunk)
class Pitch
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.assign (65536, 0.0f);
        write = 0;
        phase = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        const float ratio = std::pow (2.0f, (s.semitones + s.fineCents / 100.0f) / 12.0f);
        const float grain = (float) (0.05 * sr);
        const float step = (1.0f - ratio) / grain;
        const int mask = (int) buf[0].size() - 1;
        const float mix = s.pitchMix;

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) write] = L[i];
            buf[1][(size_t) write] = R[i];

            float wl = 0.0f, wr = 0.0f;
            for (int tap = 0; tap < 2; ++tap)
            {
                float ph = phase + 0.5f * (float) tap;
                ph -= std::floor (ph);
                const float d = ph * grain + 2.0f;
                const float w = std::sin (juce::MathConstants<float>::pi * ph);
                wl += readFrac (buf[0], write, d) * w;
                wr += readFrac (buf[1], write, d) * w;
            }

            phase += step;
            phase -= std::floor (phase);
            write = (write + 1) & mask;

            L[i] = L[i] * (1.0f - mix) + wl * mix;
            R[i] = R[i] * (1.0f - mix) + wr * mix;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> buf[2];
    int write = 0;
    float phase = 0.0f;
};

//==============================================================================
// 04 COLOR: Tape, Warm, Fuzz, Crush, Phone, Radio, Megaphone. Drive comes from the DIRT macro.
class Color
{
public:
    enum Mode { tape, warm, fuzz, crush, phone, radio, megaphone };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        prepareFilter (preHp, sr, SVF::Type::highpass);
        prepareFilter (preLp, sr, SVF::Type::lowpass);
        prepareFilter (tone, sr, SVF::Type::lowpass);
        holdCount = 0;
        held[0] = held[1] = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        const int mode = juce::jlimit (0, 6, s.colorMode);
        const bool band = mode >= phone;
        if (mode == phone)     { setCutoff (preHp, sr, 400.0f); setCutoff (preLp, sr, 3000.0f); }
        if (mode == radio)     { setCutoff (preHp, sr, 600.0f); setCutoff (preLp, sr, 4500.0f); }
        if (mode == megaphone) { setCutoff (preHp, sr, 700.0f); setCutoff (preLp, sr, 3500.0f); }
        setCutoff (tone, sr, 800.0f * std::pow (22.5f, s.colorTone));

        static constexpr float driveRange[] = { 6.0f, 3.0f, 40.0f, 0.0f, 8.0f, 5.0f, 20.0f };
        const float g = 1.0f + s.dirt * driveRange[mode];
        const float comp = 1.0f / std::pow (g, 0.6f);
        const float levels = std::pow (2.0f, 16.0f - s.dirt * 12.0f);
        const int hold = 1 + (int) (s.dirt * 15.0f);
        const float mix = s.colorMix;

        float* ch[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const bool sampleNow = (holdCount++ % hold) == 0;
            for (int c = 0; c < 2; ++c)
            {
                const float dry = ch[c][i];
                float x = dry;
                if (band) x = preLp.processSample (c, preHp.processSample (c, x));

                switch (mode)
                {
                    case tape:      x = std::tanh (g * x) * comp; break;
                    case warm:      x = (std::tanh (g * x + 0.1f) - std::tanh (0.1f)) * comp; break;
                    case fuzz:      x = juce::jlimit (-0.7f, 0.7f, std::tanh (g * x)); break;
                    case crush:
                        if (sampleNow) held[c] = std::round (x * levels) / levels;
                        x = held[c];
                        break;
                    case phone:
                    case radio:     x = std::tanh (g * x) * comp * 1.6f; break;
                    case megaphone: x = juce::jlimit (-0.6f, 0.6f, g * x) * 1.2f; break;
                    default: break;
                }

                x = tone.processSample (c, x);
                ch[c][i] = dry * (1.0f - mix) + x * mix;
            }
        }
    }

private:
    double sr = 44100.0;
    SVF preHp, preLp, tone;
    unsigned int holdCount = 0;
    float held[2] {};
};

//==============================================================================
// 05 MOD: Doubler / Chorus / Wide (modulated short delays)
class Mod
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.assign (16384, 0.0f);
        write = 0;
        lfo = 0.0f;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        struct Shape { float baseL, baseR, depthMs, phaseR; };
        static constexpr Shape shapes[] = {
            { 14.0f, 21.0f, 3.0f, 0.25f },  // Doubler
            {  9.0f,  9.0f, 4.0f, 0.5f },   // Chorus
            {  1.0f, 14.0f, 0.3f, 0.0f } }; // Wide (Haas)
        const auto& sh = shapes[juce::jlimit (0, 2, s.modMode)];
        const float msToS = (float) sr * 0.001f;
        const float depth = sh.depthMs * s.modDepth * msToS;
        const float inc = s.modRate / (float) sr;
        const int mask = (int) buf[0].size() - 1;
        const float wet = s.modMix * 0.7f;
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) write] = L[i];
            buf[1][(size_t) write] = R[i];

            const float dl = sh.baseL * msToS + depth * (0.5f + 0.5f * std::sin (twoPi * lfo));
            const float dr = sh.baseR * msToS + depth * (0.5f + 0.5f * std::sin (twoPi * (lfo + sh.phaseR)));
            const float yl = readFrac (buf[0], write, dl);
            const float yr = readFrac (buf[1], write, dr);

            lfo += inc;
            if (lfo >= 1.0f) lfo -= 1.0f;
            write = (write + 1) & mask;

            L[i] += yl * wet;
            R[i] += yr * wet;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> buf[2];
    int write = 0;
    float lfo = 0.0f;
};

//==============================================================================
// Flanger and four-voice Flangus stage, independent of the chorus.
class Flange
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& line : buffer) line.assign ((size_t) std::ceil (sr * 0.025) + 3, 0.0f);
        position = available = 0;
        phase = {0.0f, 0.29f, 0.54f, 0.82f};
        returnTone[0] = returnTone[1] = 0;
        damping = onePole (sr, 1.0 / (juce::MathConstants<double>::twoPi * 5400.0));
        for (auto* value : { &mix, &depth, &feedback, &stack, &rate })
            value->reset (sr, 0.015);
        mix.setCurrentAndTargetValue (0);
        depth.setCurrentAndTargetValue (0.5f);
        feedback.setCurrentAndTargetValue (0.35f);
        rate.setCurrentAndTargetValue (0.4f);
        stack.setCurrentAndTargetValue (0);
    }

    void deactivate()
    {
        available = 0;
        returnTone[0] = returnTone[1] = 0.0f;
        mix.setCurrentAndTargetValue (0.0f);
    }

    void process (float* left, float* right, int n, const Settings& settings)
    {
        mix.setTargetValue (settings.flangeOn ? settings.flangeMix : 0.0f);
        if (! settings.flangeOn && ! mix.isSmoothing() && mix.getCurrentValue() == 0.0f)
        {
            available = 0;
            returnTone[0] = returnTone[1] = 0;
            return;
        }

        depth.setTargetValue (settings.flangeDepth);
        feedback.setTargetValue (settings.flangeFeedback * 0.80f);
        stack.setTargetValue (settings.flangeMode == 1 ? 1.0f : 0.0f);
        rate.setTargetValue (settings.flangeRate);
        const float samplesPerMs = (float) sr * 0.001f;
        constexpr float speeds[] { 1.0f, 0.81f, 1.19f, 0.67f };
        for (int i = 0; i < n; ++i)
        {
            const float amount = mix.getNextValue(), width = depth.getNextValue();
            const float resonance = feedback.getNextValue(), layers = stack.getNextValue();
            const float step = rate.getNextValue() / (float) sr;
            float dry[] {left[i], right[i]};
            float wet[2] {};
            for (int ch = 0; ch < 2; ++ch)
            {
                const float stereo = ch == 0 ? 0.0f : 0.23f;
                float sweep = 0;
                if (layers < 1.0f)
                    sweep = tap (ch, samplesPerMs * (0.48f + width * 4.5f * wave (phase[0] + stereo)));
                float cloud = 0;
                if (layers > 0.0f)
                    for (size_t voice = 0; voice < phase.size(); ++voice)
                    {
                        const float centre = 2.1f + (float) voice * 1.8f;
                        const float excursion = width * (2.6f + (float) voice * 0.8f);
                        cloud += tap (ch, samplesPerMs * (centre + excursion * wave (phase[voice] + stereo))) * 0.25f;
                    }
                wet[ch] = sweep + layers * (cloud - sweep);
                returnTone[ch] = damping * returnTone[ch] + (1.0f - damping) * wet[ch];
                // Damped, bounded feedback keeps the resonance below self-oscillation.
                buffer[ch][(size_t) position] = dry[ch] + resonance * std::tanh (returnTone[ch]);
            }
            const float blend = 0.5f * amount;
            left[i]  = dry[0] + blend * (wet[0] - dry[0]);
            right[i] = dry[1] + blend * (wet[1] - dry[1]);
            for (size_t v = 0; v < phase.size(); ++v)
            {
                phase[v] += step * speeds[v];
                if (phase[v] >= 1.0f) phase[v] -= 1.0f;
            }
            if (++position == (int) buffer[0].size()) position = 0;
            available = std::min (available + 1, (int) buffer[0].size());
        }
    }

private:
    static float wave (float p) { return 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * p); }
    float tap (int ch, float delay) const
    {
        return delay + 1.0f <= (float) available ? readFrac (buffer[ch], position, delay) : 0.0f;
    }

    double sr = 44100.0;
    std::vector<float> buffer[2];
    int position = 0, available = 0;
    std::array<float, 4> phase {};
    float damping = 0.0f, returnTone[2] {};
    juce::SmoothedValue<float> mix, depth, feedback, stack, rate;
};

//==============================================================================
// Stereo-linked attenuation of the wet return, without changing the echo timing or feedback.
class ReturnDucker
{
public:
    void prepare (double sampleRate, double releaseSeconds)
    {
        attack = onePole (sampleRate, 0.006);
        release = onePole (sampleRate, releaseSeconds);
        envelope = 0;
    }

    float gain (float left, float right, float amount)
    {
        envelope = follow (envelope, std::max (std::abs (left), std::abs (right)), attack, release);
        return 1.0f / (1.0f + 14.0f * amount * envelope);
    }

private:
    float attack = 0, release = 0, envelope = 0;
};

//==============================================================================
// 06 DELAY: tempo-synced echo. THROW only feeds the delay at the end of words.
class Delay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& b : buf) b.assign ((size_t) (sr * 4.0) + 1, 0.0f);
        write = 0;
        prepareFilter (tone, sr, SVF::Type::lowpass);
        for (auto& h : history) h.assign ((size_t) std::ceil (sr * 0.100) + 1, 0.0f);
        historyPos = valid = 0;
        ducker.prepare (sr, 0.18);
        fastAtt = onePole (sr, 0.001); fastRel = onePole (sr, 0.015);
        slowAtt = onePole (sr, 0.005); slowRel = onePole (sr, 0.3);
        gateAtt = onePole (sr, 0.005); gateRel = onePole (sr, 0.12);
        fast = slow = gate = 0.0f;
    }

    void deactivate()
    {
        if (valid == 0) return;
        valid = 0;
        fast = slow = gate = 0.0f;
        tone.reset();
        ducker.prepare (sr, 0.18);
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        // 1/4, 1/8, 1/16, 1/4 dotted, Ping-pong (1/4)
        static constexpr double beats[] = { 1.0, 0.5, 0.25, 1.5, 1.0 };
        const int mode = juce::jlimit (0, 4, s.delayMode);
        const double beat = 60.0 / juce::jlimit (40.0, 300.0, s.bpm);
        const int size = (int) buf[0].size();
        const int d = juce::jlimit (1, size - 1, (int) (beat * beats[mode] * sr));
        const bool ping = mode == 4;
        const int historySize = (int) history[0].size();
        const int lead = std::min (d - 1, historySize - 1);
        const float fb = s.delayFeedback * 0.9f;
        const float wet = s.delayMix * s.space;
        setCutoff (tone, sr, 1000.0f * std::pow (12.0f, s.delayTone));

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];

            history[0][(size_t) historyPos] = inL;
            history[1][(size_t) historyPos] = inR;
            const float lvl = juce::jmax (std::abs (inL), std::abs (inR));
            fast = follow (fast, lvl, fastAtt, fastRel);
            slow = follow (slow, lvl, slowAtt, slowRel);
            const float target = (slow > 0.002f && fast < slow * 0.55f) ? 1.0f : 0.0f;
            gate = follow (gate, target, gateAtt, gateRel);

            int r = write - d;
            if (r < 0) r += size;
            const float rl = valid >= d ? buf[0][(size_t) r] : 0.0f;
            const float rr = valid >= d ? buf[1][(size_t) r] : 0.0f;
            const float fl = tone.processSample (0, rl), fr = tone.processSample (1, rr);

            buf[0][(size_t) write] = (ping ? fr : fl) * fb;
            buf[1][(size_t) write] = (ping ? fl : fr) * fb;
            // A falling envelope arrives after the word. Feed the preceding syllable
            // into its original slot in the delay line so its repeat stays on the beat.
            int at = write;
            float sendL = inL, sendR = inR;
            if (s.throwOn)
            {
                const int read = (historyPos + historySize - lead) % historySize;
                sendL = valid >= lead ? history[0][(size_t) read] * gate : 0.0f;
                sendR = valid >= lead ? history[1][(size_t) read] * gate : 0.0f;
                at = (write + size - lead) % size;
            }
            if (ping)
                buf[0][(size_t) at] += 0.5f * (sendL + sendR);
            else
            {
                buf[0][(size_t) at] += sendL;
                buf[1][(size_t) at] += sendR;
            }
            valid = std::min (valid + 1, size);
            if (++historyPos == historySize) historyPos = 0;
            if (++write >= size) write = 0;
            const float returnGain = wet * ducker.gain (inL, inR, s.delayDuck);
            L[i] = inL + rl * returnGain;
            R[i] = inR + rr * returnGain;
        }
    }

private:
    double sr = 44100.0;
    std::vector<float> buf[2];
    int write = 0;
    SVF tone;
    std::vector<float> history[2];
    int historyPos = 0, valid = 0;
    ReturnDucker ducker;
    float fastAtt = 0, fastRel = 0, slowAtt = 0, slowRel = 0, gateAtt = 0, gateRel = 0;
    float fast = 0, slow = 0, gate = 0;
};

//==============================================================================
// 07 REVERB: Room, Plate, Hall, Dark, Reverse (+ pre-delay)
class Space
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ducker.prepare (sr, 0.28);
        reverb.setSampleRate (sr);
        reverb.reset();
        for (auto& b : pre) b.assign ((size_t) (sr * 0.2) + 1, 0.0f);
        preWrite = validPre = validReverse = 0;
        active = false;
        prepareFilter (dark, sr, SVF::Type::lowpass);
        setCutoff (dark, sr, 2500.0f);
        const auto maxLen = (size_t) (sr * 1.6);
        for (auto& b : rec) b.assign (maxLen, 0.0f);
        for (auto& b : play) b.assign (maxLen, 0.0f);
        revPos = 0;
        revLen = (int) (sr * 0.8);
    }

    void deactivate()
    {
        if (! active) return;
        active = false;
        reverb.reset();
        dark.reset();
        ducker.prepare (sr, 0.28);
        validPre = validReverse = revPos = 0;
    }

    void process (float* L, float* R, int n, const Settings& s)
    {
        static constexpr float rooms[] = { 0.45f, 0.7f, 0.88f, 0.85f, 0.88f };
        static constexpr float damps[] = { 0.5f, 0.25f, 0.4f, 0.8f, 0.4f };
        const int mode = juce::jlimit (0, 4, s.reverbMode);
        active = true;

        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, rooms[mode] + (s.reverbSize - 0.5f) * 0.24f);
        p.damping = damps[mode];
        p.wetLevel = 0.5f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        reverb.setParameters (p);

        const int preSize = (int) pre[0].size();
        const int pd = juce::jlimit (0, preSize - 1, (int) (s.preDelayMs * 0.001f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            pre[0][(size_t) preWrite] = L[i];
            pre[1][(size_t) preWrite] = R[i];
            int r = preWrite - pd;
            if (r < 0) r += preSize;
            wetL[i] = validPre >= pd ? pre[0][(size_t) r] : 0.0f;
            wetR[i] = validPre >= pd ? pre[1][(size_t) r] : 0.0f;
            validPre = std::min (preSize, validPre + 1);
            if (++preWrite >= preSize) preWrite = 0;
        }

        reverb.processStereo (wetL, wetR, n);

        if (mode == 3)
            for (int i = 0; i < n; ++i) { wetL[i] = dark.processSample (0, wetL[i]); wetR[i] = dark.processSample (1, wetR[i]); }

        if (mode == 4)
            reverse (n, s.reverbSize);
        else
            revPos = validReverse = 0;

        const float wet = s.reverbMix * s.space;
        for (int i = 0; i < n; ++i)
        {
            const float returnGain = wet * ducker.gain (L[i], R[i], s.reverbDuck);
            L[i] += wetL[i] * returnGain;
            R[i] += wetR[i] * returnGain;
        }
    }

private:
    // Records the reverb tail in chunks and plays each chunk backwards, fading in: the classic swell
    void reverse (int n, float size)
    {
        const int fade = (int) (sr * 0.01);
        for (int i = 0; i < n; ++i)
        {
            rec[0][(size_t) revPos] = wetL[i];
            rec[1][(size_t) revPos] = wetR[i];
            const auto back = (size_t) (revLen - 1 - revPos);
            const float env = ((float) revPos / (float) revLen) * juce::jmin (1.0f, (float) (revLen - revPos) / (float) fade);
            wetL[i] = back < (size_t) validReverse ? play[0][back] * env * 1.5f : 0.0f;
            wetR[i] = back < (size_t) validReverse ? play[1][back] * env * 1.5f : 0.0f;

            if (++revPos >= revLen)
            {
                std::swap (rec[0], play[0]);
                std::swap (rec[1], play[1]);
                validReverse = revLen;
                revPos = 0;
                revLen = juce::jlimit (1, (int) rec[0].size(), (int) (sr * (0.3 + size * 1.2)));
            }
        }
    }

    double sr = 44100.0;
    juce::Reverb reverb;
    ReturnDucker ducker;
    std::vector<float> pre[2], rec[2], play[2];
    int preWrite = 0, revPos = 0, revLen = 1, validPre = 0, validReverse = 0;
    bool active = false;
    SVF dark;
    float wetL[maxBlock] {}, wetR[maxBlock] {};
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        clean.prepare (sampleRate);
        punch.prepare (sampleRate);
        pitch.prepare (sampleRate);
        color.prepare (sampleRate);
        mod.prepare (sampleRate);
        flange.prepare (sampleRate);
        delay.prepare (sampleRate);
        space.prepare (sampleRate);
    }

    void discardTails()
    {
        flange.deactivate();
        delay.deactivate();
        space.deactivate();
    }

    void process (float* L, float* R, int numSamples, const Settings& s)
    {
        for (int start = 0; start < numSamples; start += maxBlock)
        {
            const int n = juce::jmin (maxBlock, numSamples - start);
            float* l = L + start;
            float* r = R + start;

            if (s.cleanOn)  clean.process (l, r, n, s);
            if (s.punchOn)  punch.process (l, r, n, s);
            if (s.pitchOn)  pitch.process (l, r, n, s);
            if (s.colorOn)  color.process (l, r, n, s);
            if (s.modOn)    mod.process (l, r, n, s);
            flange.process (l, r, n, s);
            if (s.delayOn)  delay.process (l, r, n, s); else delay.deactivate();
            if (s.reverbOn) space.process (l, r, n, s); else space.deactivate();

            // WIDTH macro: 0 = mono, 0.5 = untouched, 1 = twice as wide
            const float side = s.width * 2.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = (l[i] + r[i]) * 0.5f;
                const float sd = (l[i] - r[i]) * 0.5f * side;
                l[i] = m + sd;
                r[i] = m - sd;
            }
        }
    }

private:
    Clean clean;
    Punch punch;
    Pitch pitch;
    Color color;
    Mod mod;
    Flange flange;
    Delay delay;
    Space space;
};
} // namespace AdLib
