#include "Params.h"

namespace phat::params
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;

namespace
{
juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

juce::String dbString (float gain)
{
    if (gain < 1.0e-5f) return "-inf dB";
    return juce::String (20.0f * std::log10 (gain), 1) + " dB";
}

juce::String rateString (float v, bool synced)
{
    if (synced) return juce::String (phat::syncRateName (phat::syncIndex (v)));
    return juce::String (phat::freeRateHz (v), 2) + " Hz";
}

std::unique_ptr<APF> unit (const juce::String& id, float def,
                           std::function<juce::String (float, int)> toText = pct)
{
    return std::make_unique<APF> (juce::ParameterID { id, 1 }, displayName (id),
                                  juce::NormalisableRange<float> (0.0f, 1.0f), def,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText)));
}

std::unique_ptr<APB> toggle (const juce::String& id, bool def)
{
    return std::make_unique<APB> (juce::ParameterID { id, 1 }, displayName (id), def);
}

juce::StringArray namesFor (const juce::StringArray& ids)
{
    juce::StringArray out;
    for (auto& id : ids) out.add (displayName (id));
    return out;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto hz = [] (float hzValue) {
        return hzValue >= 1000.0f ? juce::String (hzValue / 1000.0f, 2) + " kHz"
                                  : juce::String (juce::roundToInt (hzValue)) + " Hz";
    };

    // Bandpass
    layout.add (toggle (bpOn, false));
    layout.add (unit (bpLow, 0.0f, [hz] (float v, int) { return hz (curve::bandHz (v)); }));
    layout.add (unit (bpHigh, 1.0f, [hz] (float v, int) { return hz (curve::bandHz (v)); }));
    layout.add (unit (bpQLow, 0.3f));
    layout.add (unit (bpQHigh, 0.3f));
    layout.add (unit (bpBRMix, 0.0f));
    layout.add (toggle (bpLink, false));

    // Distortion
    layout.add (toggle (distOn, false));
    layout.add (unit (distBit, 0.0f, [] (float v, int) { return juce::String (curve::bitDepth (v)) + " bit"; }));
    layout.add (unit (distMech, 0.0f));
    layout.add (unit (distTube, 0.0f));
    layout.add (unit (distXcita, 0.0f));

    // Multimode filter
    layout.add (toggle (mmOn, false));
    layout.add (unit (mmMix, 1.0f));
    {
        juce::StringArray types;
        for (int i = 0; i < NUM_FILTER_TYPES; ++i) types.add (filterTypeName (i));
        layout.add (std::make_unique<APC> (juce::ParameterID { mmType, 1 }, displayName (mmType), types, LP2_MG));
    }
    layout.add (unit (mmAttack, 0.02f, [] (float v, int) { return juce::String (attackSec (v), 2) + " s"; }));
    layout.add (unit (mmDecay, 0.1f, [] (float v, int) { return juce::String (decaySec (v), 2) + " s"; }));
    layout.add (unit (mmEnv, 0.0f));
    layout.add (unit (mmCutoff, 0.85f, [hz] (float v, int) { return hz (curve::cutoffHz (v)); }));
    layout.add (unit (mmRes, 0.2f));

    // Flanger
    layout.add (toggle (flOn, false));
    layout.add (unit (flAmount, 0.5f));
    layout.add (unit (flRate, 0.1f, [] (float v, int) { return juce::String (flangerHz (v), 2) + " Hz"; }));

    // MagicEQ
    layout.add (toggle (eqOn, false));
    layout.add (unit (eqAmount, 0.5f));
    layout.add (unit (eqTune, 0.33f, [] (float v, int) { return juce::String (juce::roundToInt (tuneHz (v))) + " Hz"; }));

    // Compressor
    layout.add (toggle (compOn, false));
    layout.add (unit (compAmount, 0.3f, [] (float v, int) { return juce::String (curve::compressDb (compAmt (v)), 1) + " dB"; }));
    layout.add (unit (compRelease, 0.2f, [] (float v, int) { return juce::String (juce::roundToInt (releaseMs (v))) + " ms"; }));

    // Master
    layout.add (unit (masterMix, 1.0f));
    layout.add (unit (masterVol, 1.0f, [] (float v, int) { return dbString (curve::masterGain (v)); }));

    // LFOs
    const auto targetNames = namesFor (lfoTargets());
    juce::StringArray shapes;
    for (int i = 0; i < NUM_LFO_SHAPES; ++i) shapes.add (lfoShapeName (i));

    auto addLfo = [&] (const juce::String& on, const juce::String& target, const juce::String& shape,
                       const juce::String& depth, const juce::String& rate, const juce::String& sync, int defTarget) {
        layout.add (toggle (on, false));
        layout.add (std::make_unique<APC> (juce::ParameterID { target, 1 }, displayName (target), targetNames, defTarget));
        layout.add (std::make_unique<APC> (juce::ParameterID { shape, 1 }, displayName (shape), shapes, 0));
        layout.add (unit (depth, 0.3f));
        // The rate text depends on the sync toggle; the editor refreshes it, the
        // host string assumes free-running unless synced.
        layout.add (unit (rate, 0.5f, [] (float v, int) { return rateString (v, false); }));
        layout.add (toggle (sync, false));
    };
    addLfo (lfo1On, lfo1Target, lfo1Shape, lfo1Depth, lfo1Rate, lfo1Sync, lfoTargets().indexOf (mmCutoff));
    addLfo (lfo2On, lfo2Target, lfo2Shape, lfo2Depth, lfo2Rate, lfo2Sync, lfoTargets().indexOf (flAmount));

    // XY pad targets and modes
    const auto xyNames = namesFor (xyTargets());
    layout.add (std::make_unique<APC> (juce::ParameterID { xyXTarget, 1 }, displayName (xyXTarget), xyNames,
                                       xyTargets().indexOf (mmCutoff)));
    layout.add (std::make_unique<APC> (juce::ParameterID { xyYTarget, 1 }, displayName (xyYTarget), xyNames,
                                       xyTargets().indexOf (mmRes)));
    layout.add (std::make_unique<APC> (juce::ParameterID { compMode, 1 }, displayName (compMode),
                                       juce::StringArray { "Peak", "Smooth" }, 0));
    layout.add (std::make_unique<APC> (juce::ParameterID { eqMode, 1 }, displayName (eqMode),
                                       juce::StringArray { "Limit", "Clip" }, 0));
    return layout;
}
} // namespace phat::params
