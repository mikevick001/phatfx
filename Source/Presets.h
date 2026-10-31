// Loads the original plug-in's preset banks (.fxb) from the user's installed
// CamelPhatData folder and maps them onto PhatFX parameters. No preset data is
// bundled with PhatFX.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

namespace phat
{
// Locations the original installer uses for its data folder.
inline juce::File findDataFolder()
{
    const juce::String rel = "Application Support/Camel Audio/CamelPhatData";
    for (auto base : { juce::File ("/Library"),
                       juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library") })
    {
        auto d = base.getChildFile (rel);
        if (d.isDirectory()) return d;
    }
    return {};
}

struct Preset
{
    juce::String name;
    std::vector<float> values; // original parameter order
};

struct PresetBank
{
    juce::String name;
    std::vector<Preset> presets;
};

// Parses an opaque-chunk VST bank written by the original plug-in.
inline bool parseBank (const juce::File& f, PresetBank& out)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() < 164) return false;
    auto* b = static_cast<const uint8_t*> (mb.getData());
    if (std::memcmp (b, "CcnK", 4) != 0 || std::memcmp (b + 8, "FBCh", 4) != 0) return false;

    const size_t size = mb.getSize();
    size_t p = 160; // VST bank header (156 bytes) + chunk size
    out.name = f.getFileNameWithoutExtension().trimCharactersAtStart ("-");
    out.presets.clear();
    auto readF = [&] (size_t at) { float v; std::memcpy (&v, b + at, 4); return juce::ByteOrder::swapIfBigEndian (v); };
    auto readI = [&] (size_t at) { int32_t v; std::memcpy (&v, b + at, 4); return (int) juce::ByteOrder::swapIfBigEndian (v); };

    while (p + 8 <= size)
    {
        const float version = readF (p);
        if (version < 0.5f || version > 10.0f) break;
        size_t e = p + 4;
        while (e < size && b[e] != 0) ++e;
        if (e >= size) break;
        Preset pr;
        pr.name = juce::String::fromUTF8 (reinterpret_cast<const char*> (b + p + 4), (int) (e - p - 4));
        size_t q = e + 1;
        if (q + 4 > size) break;
        const int n = readI (q);
        q += 4;
        if (n <= 0 || n > 512 || q + (size_t) n * 4 > size) break;
        for (int i = 0; i < n; ++i) pr.values.push_back (readF (q + (size_t) i * 4));
        out.presets.push_back (std::move (pr));
        p = q + (size_t) n * 4;
    }
    return ! out.presets.empty();
}

inline std::vector<PresetBank> loadAllBanks()
{
    std::vector<PresetBank> banks;
    auto dir = findDataFolder();
    if (! dir.isDirectory()) return banks;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.fxb");
    files.sort();
    for (auto& f : files)
    {
        PresetBank bank;
        if (parseBank (f, bank)) banks.push_back (std::move (bank));
    }
    return banks;
}

// Applies one original preset to the PhatFX parameter tree.
inline void applyPreset (juce::AudioProcessorValueTreeState& s, const Preset& pr)
{
    using namespace params;
    // Original parameter order (index 29 "MasterOn" has no PhatFX equivalent).
    static const juce::String order[] = { bpOn, bpLow, bpHigh, bpQLow, bpQHigh, bpBRMix, bpLink,
                                          distOn, distBit, distMech, distTube, distXcita,
                                          mmOn, mmMix, mmType, mmAttack, mmDecay, mmEnv, mmCutoff, mmRes,
                                          flOn, flAmount, flRate, eqOn, eqAmount, eqTune,
                                          compOn, compAmount, compRelease, {}, masterMix, masterVol,
                                          lfo1On, lfo1Target, lfo1Shape, lfo1Depth, lfo1Rate, lfo1Sync,
                                          lfo2On, lfo2Target, lfo2Shape, lfo2Depth, lfo2Rate, lfo2Sync,
                                          xyXTarget, xyYTarget, compMode, eqMode };
    // Filter menu position (as stored) -> filter type.
    static const int filterMap[] = { 0, 2, 4, 1, 3, 5, 7, 8, 9, 11, 10, 11, 12, 13, 14, 15, 16,
                                     17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 6 };

    const int n = std::min ((int) pr.values.size(), (int) std::size (order));
    for (int i = 0; i < n; ++i)
    {
        if (order[i].isEmpty()) continue;
        auto* p = s.getParameter (order[i]);
        if (p == nullptr) continue;
        float v = juce::jlimit (0.0f, 1.0f, pr.values[(size_t) i]);
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p))
        {
            int idx = juce::roundToInt (v * 100.0f);
            if (order[i] == mmType) idx = filterMap[juce::jlimit (0, 33, idx)];
            if (order[i] == compMode || order[i] == eqMode) idx = v > 0.5f ? 1 : 0;
            idx = juce::jlimit (0, c->choices.size() - 1, idx);
            v = p->convertTo0to1 ((float) idx);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
            v = v > 0.5f ? 1.0f : 0.0f;
        p->setValueNotifyingHost (v);
    }
}
} // namespace phat
