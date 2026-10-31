#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"

namespace phat::params
{
// Parameter IDs. Order and ranges follow the original plug-in's parameter table
// (all continuous controls are unit 0..1 with the original display curves).
#define PHAT_PARAM_IDS(X)                                                                   \
    X (bpOn) X (bpLow) X (bpHigh) X (bpQLow) X (bpQHigh) X (bpBRMix) X (bpLink)             \
    X (distOn) X (distBit) X (distMech) X (distTube) X (distXcita)                          \
    X (mmOn) X (mmMix) X (mmType) X (mmAttack) X (mmDecay) X (mmEnv) X (mmCutoff) X (mmRes) \
    X (flOn) X (flAmount) X (flRate)                                                        \
    X (eqOn) X (eqAmount) X (eqTune)                                                        \
    X (compOn) X (compAmount) X (compRelease)                                               \
    X (masterMix) X (masterVol)                                                             \
    X (lfo1On) X (lfo1Target) X (lfo1Shape) X (lfo1Depth) X (lfo1Rate) X (lfo1Sync)         \
    X (lfo2On) X (lfo2Target) X (lfo2Shape) X (lfo2Depth) X (lfo2Rate) X (lfo2Sync)         \
    X (xyXTarget) X (xyYTarget) X (compMode) X (eqMode)

#define PHAT_ID_STRING(name) inline const juce::String name { #name };
PHAT_PARAM_IDS (PHAT_ID_STRING)
#undef PHAT_ID_STRING

// Parameters an LFO can modulate (original order).
inline const juce::StringArray& lfoTargets()
{
    static const juce::StringArray ids { bpLow, bpHigh, bpQLow, bpQHigh, bpBRMix, distBit, distMech, distTube,
                                         distXcita, mmMix, mmAttack, mmDecay, mmEnv, mmCutoff, mmRes,
                                         flAmount, flRate, eqAmount, eqTune, compAmount, compRelease,
                                         masterMix, masterVol };
    return ids;
}

// Parameters the XY pad can drive (LFO targets plus LFO rate/depth).
inline const juce::StringArray& xyTargets()
{
    static const juce::StringArray ids = [] {
        auto a = lfoTargets();
        a.addArray ({ lfo1Rate, lfo1Depth, lfo2Rate, lfo2Depth });
        return a;
    }();
    return ids;
}

inline juce::String displayName (const juce::String& id)
{
    static const std::map<juce::String, juce::String> names {
        { bpOn, "BP On" }, { bpLow, "BP Low" }, { bpHigh, "BP High" }, { bpQLow, "BP Q Low" },
        { bpQHigh, "BP Q High" }, { bpBRMix, "BR Mix" }, { bpLink, "BP Link" },
        { distOn, "Dist On" }, { distBit, "Bit Crush" }, { distMech, "Mech" }, { distTube, "Tube" },
        { distXcita, "Xcita" }, { mmOn, "Filter On" }, { mmMix, "Filter Mix" }, { mmType, "Filter Type" },
        { mmAttack, "Env Attack" }, { mmDecay, "Env Decay" }, { mmEnv, "Env Amount" },
        { mmCutoff, "Cutoff" }, { mmRes, "Resonance" }, { flOn, "Flanger On" }, { flAmount, "Flanger Amt" },
        { flRate, "Flanger Rate" }, { eqOn, "MagicEQ On" }, { eqAmount, "MagicEQ Amt" },
        { eqTune, "MagicEQ Tune" }, { compOn, "Comp On" }, { compAmount, "Compress" },
        { compRelease, "Release" }, { masterMix, "Master Mix" }, { masterVol, "Master Vol" },
        { lfo1On, "LFO1 On" }, { lfo1Target, "LFO1 Target" }, { lfo1Shape, "LFO1 Shape" },
        { lfo1Depth, "LFO1 Depth" }, { lfo1Rate, "LFO1 Rate" }, { lfo1Sync, "LFO1 Sync" },
        { lfo2On, "LFO2 On" }, { lfo2Target, "LFO2 Target" }, { lfo2Shape, "LFO2 Shape" },
        { lfo2Depth, "LFO2 Depth" }, { lfo2Rate, "LFO2 Rate" }, { lfo2Sync, "LFO2 Sync" },
        { xyXTarget, "XY X Target" }, { xyYTarget, "XY Y Target" }, { compMode, "Comp Mode" },
        { eqMode, "MagicEQ Mode" } };
    auto it = names.find (id);
    return it != names.end() ? it->second : id;
}

// --- engineering ranges for the non-unit "value" params -----------------------
inline float attackSec (float v) { return 0.01f + 0.99f * v; }   // 0.01 .. 1 s
inline float decaySec (float v) { return 0.2f + 2.8f * v; }      // 0.2 .. 3 s
inline float flangerHz (float v) { return 0.02f + 1.98f * v; }   // 0.02 .. 2 Hz
inline float tuneHz (float v) { return 60.0f + 60.0f * v; }      // 60 .. 120 Hz
inline float releaseMs (float v) { return 100.0f + 1900.0f * v; }// 100 .. 2000 ms
inline float compAmt (float v) { return 0.99f * v; }             // 0 .. 0.99

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace phat::params
