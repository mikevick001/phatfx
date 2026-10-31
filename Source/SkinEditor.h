// Editor that renders the original plug-in's skin from the user's own installed
// CamelPhatData folder (layout file + bitmaps), read at runtime. If no skin is
// found, PhatFX falls back to its built-in editor.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace phat
{
struct SkinData
{
    struct BitmapFont
    {
        juce::Image image;
        juce::String chars;
        std::vector<int> widths, offsets;
    };
    struct Control
    {
        juce::String name, type, image, page;
        int x1 = 0, y1 = 0, x2 = -1, y2 = -1;
        bool isRect() const { return x2 >= 0 && y2 >= 0; }
    };

    std::map<juce::String, juce::Image> images;
    std::map<juce::String, BitmapFont> fonts;
    std::map<juce::String, int> values;
    std::vector<Control> controls;
    std::map<juce::String, Control> pages;

    bool load (const juce::File& skinDir);
    const Control* find (const juce::String& name) const;
    juce::Image image (const juce::String& name) const;
    int value (const juce::String& name, int def) const;

    int textWidth (const juce::String& font, const juce::String& text) const;
    void drawText (juce::Graphics&, const juce::String& font, const juce::String& text,
                   juce::Rectangle<int> area, juce::Justification) const;
};

juce::File findSkinFolder();

class SkinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit SkinLookAndFeel (const SkinData& s) : skin (s) {}
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle,
                           juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 10; }

private:
    const SkinData& skin;
};

class SkinnedEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    SkinnedEditor (PhatFXProcessor&, std::unique_ptr<SkinData>);
    ~SkinnedEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

    class Canvas;

private:
    void timerCallback() override;
    std::unique_ptr<SkinData> skin;
    std::unique_ptr<SkinLookAndFeel> lnf;
    std::unique_ptr<Canvas> canvas;
};
} // namespace phat
