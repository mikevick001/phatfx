#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PhatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PhatLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Knob with a caption above and value text below.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// A titled panel that lays its children out in one row.
class Section : public juce::Component
{
public:
    Section (const juce::String& t, juce::Colour a) : title (t), accent (a) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    void add (juce::Component& c, float weight = 1.0f) { items.push_back ({ &c, weight }); addAndMakeVisible (c); }
    void setHeader (juce::Component& c) { header = &c; addAndMakeVisible (c); }

private:
    juce::String title;
    juce::Colour accent;
    juce::Component* header = nullptr;
    std::vector<std::pair<juce::Component*, float>> items;
};

class XYPad : public juce::Component
{
public:
    explicit XYPad (PhatFXProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* target (const juce::String& choiceId) const;
    void setFromMouse (const juce::MouseEvent&);
    PhatFXProcessor& proc;
    juce::RangedAudioParameter *dragX = nullptr, *dragY = nullptr;
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float level = 0;
};

class PhatFXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PhatFXEditor (PhatFXProcessor&);
    ~PhatFXEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupLink();

    PhatFXProcessor& proc;
    PhatLookAndFeel lnf;
    juce::AudioProcessorValueTreeState& s;

    Section bpSec { "BANDPASS", juce::Colour (0xff4fc3f7) }, distSec { "DISTORTION", juce::Colour (0xffff8a65) },
        filtSec { "FILTER", juce::Colour (0xffba68c8) }, flSec { "FLANGER", juce::Colour (0xff81c784) },
        eqSec { "MAGIC EQ", juce::Colour (0xfffff176) }, compSec { "COMPRESSOR", juce::Colour (0xff4db6ac) },
        masterSec { "MASTER", juce::Colour (0xffe0e0e0) }, lfo1Sec { "LFO 1", juce::Colour (0xfff06292) },
        lfo2Sec { "LFO 2", juce::Colour (0xff9575cd) }, xySec { "XY MORPH", juce::Colour (0xffffb74d) };

    Toggle bpOn, bpLink, distOn, mmOn, flOn, eqOn, compOn, lfo1On, lfo1Sync, lfo2On, lfo2Sync;
    Knob bpLow, bpHigh, bpQLow, bpQHigh, bpBR, distBit, distMech, distTube, distXcita, mmCutoff, mmRes, mmEnv,
        mmAttack, mmDecay, mmMix, flAmount, flRate, eqAmount, eqTune, compAmount, compRelease, masterMix,
        masterVol, lfo1Depth, lfo1Rate, lfo2Depth, lfo2Rate;
    Choice mmType, eqMode, compMode, lfo1Target, lfo1Shape, lfo2Target, lfo2Shape, xyX, xyY;

    XYPad pad;
    Meter meterL, meterR;
    juce::Component meterBox;
    juce::TextButton randomButton { "Randomize" };
    juce::Label lfo1Light, lfo2Light;

    float lastLow = 0, lastHigh = 1;
    bool linking = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhatFXEditor)
};
