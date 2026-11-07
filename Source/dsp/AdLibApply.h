#pragma once
// Applies an Ad-lib FX factory preset value to the engine settings.
// Copied from Swell Ad-lib FX (tools/RenderDemo.cpp): AdLib::Settings defaults are the parameter defaults.
#include "AdLibEngine.h"
#include "AdLibPresets.h"
#include <cstring>

namespace AdLibPresets
{
inline void apply (AdLib::Settings& s, const juce::String& id, float v)
{
    const bool on = v > 0.5f;
    const int mode = juce::roundToInt (v);
    if      (id == "space") s.space = v;
    else if (id == "dirt") s.dirt = v;
    else if (id == "pitch") s.semitones = v;
    else if (id == "width") s.width = v;
    else if (id == "clean_on") s.cleanOn = on;
    else if (id == "clean_mode") s.cleanMode = mode;
    else if (id == "clean_low") s.lowCut = v;
    else if (id == "clean_high") s.highCut = v;
    else if (id == "clean_deess") s.deess = v;
    else if (id == "punch_on") s.punchOn = on;
    else if (id == "punch_mode") s.punchMode = mode;
    else if (id == "punch_amount") s.punchAmount = v;
    else if (id == "punch_attack") s.punchAttackMs = v;
    else if (id == "punch_makeup") s.punchMakeupDb = v;
    else if (id == "pitch_on") s.pitchOn = on;
    else if (id == "pitch_mix") s.pitchMix = v;
    else if (id == "pitch_fine") s.fineCents = v;
    else if (id == "color_on") s.colorOn = on;
    else if (id == "color_mode") s.colorMode = mode;
    else if (id == "color_tone") s.colorTone = v;
    else if (id == "color_mix") s.colorMix = v;
    else if (id == "mod_on") s.modOn = on;
    else if (id == "mod_mode") s.modMode = mode;
    else if (id == "mod_depth") s.modDepth = v;
    else if (id == "mod_rate") s.modRate = v;
    else if (id == "mod_mix") s.modMix = v;
    else if (id == "delay_duck")     s.delayDuck = v;
    else if (id == "reverb_duck")    s.reverbDuck = v;
    else if (id == "flange_on")      s.flangeOn = on;
    else if (id == "flange_mode")    s.flangeMode = mode;
    else if (id == "flange_mix")     s.flangeMix = v;
    else if (id == "flange_depth")   s.flangeDepth = v;
    else if (id == "flange_rate")    s.flangeRate = v;
    else if (id == "flange_feedback") s.flangeFeedback = v;
    else if (id == "delay_on") s.delayOn = on;
    else if (id == "delay_mode") s.delayMode = mode;
    else if (id == "delay_feedback") s.delayFeedback = v;
    else if (id == "delay_tone") s.delayTone = v;
    else if (id == "delay_mix") s.delayMix = v;
    else if (id == "throw") s.throwOn = on;
    else if (id == "reverb_on") s.reverbOn = on;
    else if (id == "reverb_mode") s.reverbMode = mode;
    else if (id == "reverb_size") s.reverbSize = v;
    else if (id == "reverb_predelay") s.preDelayMs = v;
    else if (id == "reverb_mix") s.reverbMix = v;
}

/** Engine settings of every factory preset, built once (never on the audio thread). */
inline const std::vector<AdLib::Settings>& settingsTable()
{
    static const std::vector<AdLib::Settings> table = []
    {
        std::vector<AdLib::Settings> t;
        for (const auto& p : all())
        {
            AdLib::Settings s;
            for (const auto& v : p.values) apply (s, juce::String (v.id), v.value);
            t.push_back (s);
        }
        return t;
    }();
    return table;
}
} // namespace AdLibPresets
