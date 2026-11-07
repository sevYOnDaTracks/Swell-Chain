#pragma once
// Chain presets: which modules, in which order, with which module preset, plus a few knob changes.
// Starting points to validate by ear in real sessions.
#include "Chain.h"

namespace chain
{
struct Step
{
    Type type;
    int preset = 0;
    std::vector<std::pair<int, float>> knobs {};   // knob index, value in real units
};

struct ChainPreset
{
    const char* name;
    const char* style;
    std::vector<Step> steps;
};

// Module preset indices, for readability.
namespace mp
{
enum Match { lateNight = 0, neonAuto, divaPower, sharpFlow };
enum Vox { natural = 0, upfront, softWarm };
enum AdLib { astro = 0, demonLow, throwItBack, toronto, silkStack, queenChoir, rageScream, blownOut, bullhorn, phoneCall,
             radioDays, ghostReverse, chipmunk, basementSlap, velvetFifth, razorAnswer, orbitThrow, velvetFlangus,
             jetSweep, lastWord, softFocus, prismStack, lowOrbit };
enum Space { vocalClean = 0, slap, rapThrow, wideChorus, dreamy, spaceDrift = 18, infiniteClouds = 19 };
enum Glue { gentle = 0, warm = 3 };
}

inline Step M (int profile) { return { Type::match, profile }; }
inline Step V (int preset) { return { Type::vox, preset }; }
inline Step L (float lift) { return { Type::lift, 0, { { 0, lift } } }; }
inline Step A (int preset, float mix) { return { Type::adlib, preset, { { 4, mix } } }; }
inline Step S (int preset) { return { Type::space, preset }; }
inline Step K (float saturation, float attack) { return { Type::knock, 0, { { 0, saturation }, { 1, attack } } }; }
inline Step G (int style, float glue) { return { Type::glue, style, { { 0, glue } } }; }

inline const std::vector<ChainPreset>& chainPresets()
{
    using namespace mp;
    static const std::vector<ChainPreset> list {
        { "Trap Lead", "Trap", { M (sharpFlow), V (upfront), L (40), S (rapThrow) } },
        { "Trap Melodic", "Trap", { M (neonAuto), V (natural), A (velvetFlangus, 50), S (dreamy) } },
        { "Trap Ad-libs", "Trap", { V (natural), A (throwItBack, 100), S (slap) } },
        { "Trap Dark", "Trap", { M (lateNight), V (upfront), K (35, 15), S (rapThrow) } },
        { "Auto-Tune Lead", "Trap", { M (neonAuto), V (upfront), L (35), S (vocalClean) } },
        { "Trap Hook", "Trap", { M (sharpFlow), V (upfront), G (gentle, 40), S (wideChorus) } },
        { "R&B Silk", "R&B", { M (divaPower), V (softWarm), A (velvetFlangus, 60), S (dreamy) } },
        { "R&B Night", "R&B", { M (lateNight), V (softWarm), S (dreamy) } },
        { "R&B Airy", "R&B", { M (divaPower), L (45), V (natural), S (wideChorus) } },
        { "Velvet Doubles", "R&B", { V (softWarm), A (velvetFlangus, 70), S (vocalClean) } },
        { "Slow Jam", "R&B", { M (divaPower), V (softWarm), G (warm, 35), S (dreamy) } },
        { "Rap Clair", "Rap", { M (lateNight), V (natural), L (35), S (vocalClean) } },
        { "Rap Sec", "Rap", { M (sharpFlow), V (upfront) } },
        { "Boom Bap", "Rap", { M (lateNight), V (upfront), K (30, 25), S (slap) } },
        { "Storytelling", "Rap", { M (lateNight), V (natural), S (vocalClean) } },
        { "Club Verse", "Rap", { M (sharpFlow), V (upfront), L (40), G (gentle, 45), S (slap) } },
        { "Afro Bounce", "Afro", { M (neonAuto), V (natural), A (silkStack, 50), S (slap) } },
        { "Afrobeats Lead", "Afro", { M (divaPower), V (natural), L (35), S (wideChorus) } },
        { "Amapiano Float", "Afro", { M (neonAuto), V (softWarm), A (softFocus, 60), S (dreamy) } },
        { "Afro Doubles", "Afro", { V (natural), A (silkStack, 80), S (slap) } },
        { "Drill Dark", "Drill", { M (lateNight), V (upfront), K (30, 20), S (rapThrow) } },
        { "UK Drill", "Drill", { M (sharpFlow), V (upfront), K (25, 20), A (basementSlap, 40), S (slap) } },
        { "Drill Ad-libs", "Drill", { V (upfront), K (40, 20), A (razorAnswer, 100), S (slap) } },
        { "Rage Scream", "Rage", { V (upfront), K (60, 20), A (rageScream, 100), S (spaceDrift) } },
        { "Hyperpop", "Rage", { M (neonAuto), V (upfront), A (chipmunk, 50), S (wideChorus) } },
        { "Distorted Hook", "Rage", { V (upfront), K (70, 30), S (rapThrow) } },
        { "Ghost Intro", "FX", { V (natural), A (ghostReverse, 100), S (infiniteClouds) } },
        { "Phone Intro", "FX", { V (natural), A (phoneCall, 100), S (slap) } },
        { "Radio Verse", "FX", { V (natural), A (radioDays, 100), S (vocalClean) } },
        { "Reverse Rise", "FX", { A (ghostReverse, 100), S (infiniteClouds) } },
        { "Topline Démo", "Topline", { M (divaPower), V (natural), S (wideChorus) } },
        { "Topline Clean", "Topline", { M (divaPower), L (35), S (vocalClean) } },
    };
    return list;
}

/** The slots a chain preset sets: its modules in order, then empty slots. */
inline std::array<SlotState, slotCount> slotsFor (const ChainPreset& p)
{
    std::array<SlotState, slotCount> slots;
    for (size_t i = 0; i < slots.size() && i < p.steps.size(); ++i)
    {
        const auto& step = p.steps[i];
        slots[i].type = step.type;
        slots[i].on = true;
        slots[i].preset = step.preset;
        slots[i].knobs = presetKnobs (step.type, step.preset);
        for (auto [k, v] : step.knobs) slots[i].knobs[(size_t) k] = v;
    }
    return slots;
}
} // namespace chain
