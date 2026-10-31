#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray& paramIdsInEngineOrder()
{
    using namespace phat::params;
    static const juce::StringArray ids {
#define PHAT_ID_ENTRY(name) name,
        PHAT_PARAM_IDS (PHAT_ID_ENTRY)
#undef PHAT_ID_ENTRY
    };
    return ids;
}
} // namespace

PhatFXProcessor::PhatFXProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PhatFX", phat::params::createLayout())
{
    const auto& ids = paramIdsInEngineOrder();
    jassert (ids.size() == phat::P_COUNT);
    for (int i = 0; i < phat::P_COUNT; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (ids[i]);
        jassert (raw[(size_t) i] != nullptr);
    }
}

bool PhatFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void PhatFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < phat::P_COUNT; ++i) engine.base[i] = raw[(size_t) i]->load();
    engine.prepare ((float) sampleRate);
    monoScratch.setSize (1, std::max (1, samplesPerBlock));
}

void PhatFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);
    if (n == 0) return;

    for (int i = 0; i < phat::P_COUNT; ++i) engine.base[i] = raw[(size_t) i]->load();

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }

    float* l = buffer.getWritePointer (0);
    float* r;
    if (buffer.getNumChannels() > 1) r = buffer.getWritePointer (1);
    else
    {
        if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, false, true);
        monoScratch.copyFrom (0, 0, buffer, 0, 0, n);
        r = monoScratch.getWritePointer (0);
    }

    engine.process (l, r, n, bpm, ppq, playing);

    if (buffer.getNumChannels() == 1)
        for (int i = 0; i < n; ++i) l[i] = 0.5f * (l[i] + r[i]);

    meterL = engine.meter[0];
    meterR = engine.meter[1];
    lfoOut1 = engine.lfoValue[0];
    lfoOut2 = engine.lfoValue[1];
}

void PhatFXProcessor::randomize()
{
    juce::Random rng;
    using namespace phat::params;
    const juce::StringArray continuous { bpLow, bpHigh, bpQLow, bpQHigh, distMech, distTube, distXcita,
                                         mmMix, mmAttack, mmDecay, mmEnv, mmCutoff, mmRes, flAmount, flRate,
                                         eqAmount, eqTune, compAmount, compRelease,
                                         lfo1Depth, lfo1Rate, lfo2Depth, lfo2Rate };
    for (auto& id : continuous)
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (rng.nextFloat());
    for (auto id : { mmType, lfo1Shape, lfo2Shape, lfo1Target, lfo2Target })
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (rng.nextFloat());
}

void PhatFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void PhatFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PhatFXProcessor::createEditor() { return new PhatFXEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PhatFXProcessor(); }
