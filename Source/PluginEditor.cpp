#include "PluginEditor.h"

using namespace phat::params;

namespace colours
{
const juce::Colour bg (0xff15171c), panel (0xff1f232b), panelEdge (0xff2c313b), text (0xffe8eaed),
    dim (0xff8a909c), track (0xff353b47);
}

// ---------------------------------------------------------------------------
PhatLookAndFeel::PhatLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::track);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::TextButton::buttonColourId, colours::track);
    setColour (juce::ToggleButton::textColourId, colours::text);
}

void PhatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                        float end, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    const float lw = std::max (2.5f, radius * 0.14f);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, radius - lw, radius - lw, 0, start, end, true);
    value.addCentredArc (c.x, c.y, radius - lw, radius - lw, 0, start, angle, true);
    g.setColour (colours::track);
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.isEnabled() ? accent : accent.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = radius - lw * 2.2f;
    g.setColour (colours::panelEdge);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -inner, 3.0f, inner * 0.55f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void PhatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? accent.withAlpha (0.9f) : colours::track.brighter (hi ? 0.15f : 0.0f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::bg : colours::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);
    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, (double) s.getParameter (id)->getDefaultValue());
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (caption, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.removeFromTop (24));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption)
{
    button.setButtonText (caption);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

// ---------------------------------------------------------------------------
void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.getX() + 10, r.getY() + 9, 3, 14, 1.5f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (colours::text);
    g.drawText (title, (int) r.getX() + 18, (int) r.getY() + 6, 200, 20, juce::Justification::centredLeft);
}

void Section::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto top = r.removeFromTop (24);
    if (header != nullptr) header->setBounds (top.removeFromRight (std::min (top.getWidth() - 110, 150)).reduced (0, 1));
    r.removeFromTop (4);
    float total = 0;
    for (auto& it : items) total += it.second;
    float x = (float) r.getX();
    for (auto& it : items)
    {
        const float w = (float) r.getWidth() * it.second / std::max (1.0f, total);
        it.first->setBounds (juce::Rectangle<int> ((int) x, r.getY(), (int) w, r.getHeight()).reduced (3, 0));
        x += w;
    }
}

// ---------------------------------------------------------------------------
XYPad::XYPad (PhatFXProcessor& p) : proc (p) {}

juce::RangedAudioParameter* XYPad::target (const juce::String& choiceId) const
{
    const int idx = (int) proc.apvts.getRawParameterValue (choiceId)->load();
    return proc.apvts.getParameter (xyTargets()[juce::jlimit (0, xyTargets().size() - 1, idx)]);
}

void XYPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::track.darker (0.3f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::panelEdge);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4), r.getX(), r.getRight());
    }
    auto* tx = target (xyXTarget);
    auto* ty = target (xyYTarget);
    if (tx == nullptr || ty == nullptr) return;
    const float px = r.getX() + tx->getValue() * r.getWidth();
    const float py = r.getBottom() - ty->getValue() * r.getHeight();
    const juce::Colour a (0xffffb74d);
    g.setColour (a.withAlpha (0.25f));
    g.drawVerticalLine ((int) px, r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) py, r.getX(), r.getRight());
    g.setColour (a);
    g.fillEllipse (px - 7, py - 7, 14, 14);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("X: " + tx->getName (24), r.reduced (6).removeFromBottom (14), juce::Justification::centredRight);
    g.drawText ("Y: " + ty->getName (24), r.reduced (6).removeFromTop (14), juce::Justification::centredLeft);
}

void XYPad::setFromMouse (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat();
    const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
    const float y = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - r.getY()) / r.getHeight());
    if (dragX != nullptr) dragX->setValueNotifyingHost (x);
    if (dragY != nullptr && dragY != dragX) dragY->setValueNotifyingHost (y);
    repaint();
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    dragX = target (xyXTarget);
    dragY = target (xyYTarget);
    if (dragX) dragX->beginChangeGesture();
    if (dragY && dragY != dragX) dragY->beginChangeGesture();
    setFromMouse (e);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e); }

void XYPad::mouseUp (const juce::MouseEvent&)
{
    if (dragX) dragX->endChangeGesture();
    if (dragY && dragY != dragX) dragY->endChangeGesture();
    dragX = dragY = nullptr;
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::track);
    g.fillRoundedRectangle (r, 3.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float h = r.getHeight() * juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff66bb6a), 0, r.getBottom(), juce::Colour (0xffef5350),
                                             0, r.getY(), false));
    g.fillRoundedRectangle (r.withTop (r.getBottom() - h), 3.0f);
}

// ---------------------------------------------------------------------------
PhatFXEditor::PhatFXEditor (PhatFXProcessor& p)
    : AudioProcessorEditor (&p), proc (p), s (p.apvts),
      bpOn (s, phat::params::bpOn, "ON"), bpLink (s, phat::params::bpLink, "LINK"), distOn (s, phat::params::distOn, "ON"),
      mmOn (s, phat::params::mmOn, "ON"), flOn (s, phat::params::flOn, "ON"), eqOn (s, phat::params::eqOn, "ON"),
      compOn (s, phat::params::compOn, "ON"), lfo1On (s, phat::params::lfo1On, "ON"), lfo1Sync (s, phat::params::lfo1Sync, "SYNC"),
      lfo2On (s, phat::params::lfo2On, "ON"), lfo2Sync (s, phat::params::lfo2Sync, "SYNC"),
      bpLow (s, phat::params::bpLow, "Low"), bpHigh (s, phat::params::bpHigh, "High"), bpQLow (s, phat::params::bpQLow, "Q Low"),
      bpQHigh (s, phat::params::bpQHigh, "Q High"), bpBR (s, phat::params::bpBRMix, "BR Mix"),
      distBit (s, phat::params::distBit, "Bit"), distMech (s, phat::params::distMech, "Mech"),
      distTube (s, phat::params::distTube, "Tube"), distXcita (s, phat::params::distXcita, "Xcita"),
      mmCutoff (s, phat::params::mmCutoff, "Cutoff"), mmRes (s, phat::params::mmRes, "Res"), mmEnv (s, phat::params::mmEnv, "Env"),
      mmAttack (s, phat::params::mmAttack, "Attack"), mmDecay (s, phat::params::mmDecay, "Decay"), mmMix (s, phat::params::mmMix, "Mix"),
      flAmount (s, phat::params::flAmount, "Amount"), flRate (s, phat::params::flRate, "Rate"),
      eqAmount (s, phat::params::eqAmount, "Amount"), eqTune (s, phat::params::eqTune, "Tune"),
      compAmount (s, phat::params::compAmount, "Amount"), compRelease (s, phat::params::compRelease, "Release"),
      masterMix (s, phat::params::masterMix, "Mix"), masterVol (s, phat::params::masterVol, "Volume"),
      lfo1Depth (s, phat::params::lfo1Depth, "Depth"), lfo1Rate (s, phat::params::lfo1Rate, "Rate"),
      lfo2Depth (s, phat::params::lfo2Depth, "Depth"), lfo2Rate (s, phat::params::lfo2Rate, "Rate"),
      mmType (s, phat::params::mmType, "Type"), eqMode (s, phat::params::eqMode, "Mode"), compMode (s, phat::params::compMode, "Mode"),
      lfo1Target (s, phat::params::lfo1Target, "Target"), lfo1Shape (s, phat::params::lfo1Shape, "Shape"),
      lfo2Target (s, phat::params::lfo2Target, "Target"), lfo2Shape (s, phat::params::lfo2Shape, "Shape"),
      xyX (s, phat::params::xyXTarget, "X"), xyY (s, phat::params::xyYTarget, "Y"), pad (p)
{
    setLookAndFeel (&lnf);

    auto tint = [] (juce::Colour c, std::initializer_list<Knob*> knobs, std::initializer_list<Toggle*> toggles) {
        for (auto* k : knobs) k->slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        for (auto* t : toggles) t->button.setColour (juce::ToggleButton::tickColourId, c);
    };
    tint (juce::Colour (0xff4fc3f7), { &bpLow, &bpHigh, &bpQLow, &bpQHigh, &bpBR }, { &bpOn, &bpLink });
    tint (juce::Colour (0xffff8a65), { &distBit, &distMech, &distTube, &distXcita }, { &distOn });
    tint (juce::Colour (0xffba68c8), { &mmCutoff, &mmRes, &mmEnv, &mmAttack, &mmDecay, &mmMix }, { &mmOn });
    tint (juce::Colour (0xff81c784), { &flAmount, &flRate }, { &flOn });
    tint (juce::Colour (0xfffff176), { &eqAmount, &eqTune }, { &eqOn });
    tint (juce::Colour (0xff4db6ac), { &compAmount, &compRelease }, { &compOn });
    tint (juce::Colour (0xffe0e0e0), { &masterMix, &masterVol }, {});
    tint (juce::Colour (0xfff06292), { &lfo1Depth, &lfo1Rate }, { &lfo1On, &lfo1Sync });
    tint (juce::Colour (0xff9575cd), { &lfo2Depth, &lfo2Rate }, { &lfo2On, &lfo2Sync });

    bpSec.setHeader (bpOn);
    for (auto* c : std::initializer_list<juce::Component*> { &bpLow, &bpHigh, &bpQLow, &bpQHigh, &bpBR }) bpSec.add (*c);
    bpSec.add (bpLink, 0.7f);
    distSec.setHeader (distOn);
    for (auto* c : std::initializer_list<juce::Component*> { &distBit, &distMech, &distTube, &distXcita }) distSec.add (*c);
    filtSec.setHeader (mmOn);
    filtSec.add (mmType, 1.6f);
    for (auto* c : std::initializer_list<juce::Component*> { &mmCutoff, &mmRes, &mmEnv, &mmAttack, &mmDecay, &mmMix }) filtSec.add (*c);
    flSec.setHeader (flOn);
    flSec.add (flAmount); flSec.add (flRate);
    eqSec.setHeader (eqOn);
    eqSec.add (eqMode, 1.2f); eqSec.add (eqAmount); eqSec.add (eqTune);
    compSec.setHeader (compOn);
    compSec.add (compMode, 1.2f); compSec.add (compAmount); compSec.add (compRelease);
    masterSec.add (masterMix); masterSec.add (masterVol); masterSec.add (meterBox, 0.5f);
    meterBox.addAndMakeVisible (meterL);
    meterBox.addAndMakeVisible (meterR);

    auto lfoSec = [] (Section& sec, Toggle& on, Toggle& sync, Choice& tgt, Choice& shp, Knob& dep, Knob& rate) {
        sec.setHeader (on);
        sec.add (tgt, 1.9f); sec.add (shp, 1.5f); sec.add (dep, 1.0f); sec.add (rate, 1.15f); sec.add (sync, 0.9f);
    };
    lfoSec (lfo1Sec, lfo1On, lfo1Sync, lfo1Target, lfo1Shape, lfo1Depth, lfo1Rate);
    lfoSec (lfo2Sec, lfo2On, lfo2Sync, lfo2Target, lfo2Shape, lfo2Depth, lfo2Rate);

    xySec.add (xyX); xySec.add (xyY);

    for (auto* sec : { &bpSec, &distSec, &filtSec, &flSec, &eqSec, &compSec, &masterSec, &lfo1Sec, &lfo2Sec, &xySec })
        addAndMakeVisible (*sec);
    addAndMakeVisible (pad);

    // LFO rate text follows the sync toggle.
    auto rateText = [this] (const juce::String& syncId) {
        return [this, syncId] (double v) {
            const bool synced = s.getRawParameterValue (syncId)->load() > 0.5f;
            return synced ? juce::String (phat::syncRateName (phat::syncIndex ((float) v)))
                          : juce::String (phat::freeRateHz ((float) v), 2) + " Hz";
        };
    };
    lfo1Rate.slider.textFromValueFunction = rateText (phat::params::lfo1Sync);
    lfo2Rate.slider.textFromValueFunction = rateText (phat::params::lfo2Sync);
    lfo1Sync.button.onClick = [this] { lfo1Rate.slider.updateText(); };
    lfo2Sync.button.onClick = [this] { lfo2Rate.slider.updateText(); };
    lfo1Rate.slider.updateText();
    lfo2Rate.slider.updateText();

    randomButton.onClick = [this] { proc.randomize(); };
    addAndMakeVisible (randomButton);

    setupLink();
    setResizable (true, true);
    setResizeLimits (900, 560, 1800, 1120);
    setSize (1120, 620);
    startTimerHz (30);
}

PhatFXEditor::~PhatFXEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PhatFXEditor::setupLink()
{
    // With LINK on, Low and High move together, keeping the band width.
    lastLow = (float) bpLow.slider.getValue();
    lastHigh = (float) bpHigh.slider.getValue();
    auto follow = [this] (Knob& moved, Knob& other, float& lastMoved, float& lastOther) {
        return [this, &moved, &other, &lastMoved, &lastOther] {
            const float v = (float) moved.slider.getValue();
            if (! linking && bpLink.button.getToggleState())
            {
                linking = true;
                other.slider.setValue (juce::jlimit (0.0, 1.0, other.slider.getValue() + (v - lastMoved)),
                                       juce::sendNotificationSync);
                linking = false;
            }
            lastMoved = v;
            lastOther = (float) other.slider.getValue();
        };
    };
    bpLow.slider.onValueChange = follow (bpLow, bpHigh, lastLow, lastHigh);
    bpHigh.slider.onValueChange = follow (bpHigh, bpLow, lastHigh, lastLow);
}

void PhatFXEditor::timerCallback()
{
    meterL.level = proc.meterL.load();
    meterR.level = proc.meterR.load();
    meterL.repaint();
    meterR.repaint();
    pad.repaint();
}

void PhatFXEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("PhatFX", 16, 8, 200, 30, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("multi-effect", 104, 14, 200, 22, juce::Justification::centredLeft);
}

void PhatFXEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (36);
    randomButton.setBounds (header.removeFromRight (110).reduced (0, 4));

    auto right = r.removeFromRight (juce::roundToInt (r.getWidth() * 0.37f));
    const int rowH = r.getHeight() / 4;

    auto row1 = r.removeFromTop (rowH);
    bpSec.setBounds (row1.removeFromLeft (juce::roundToInt (row1.getWidth() * 0.58f)));
    distSec.setBounds (row1);

    filtSec.setBounds (r.removeFromTop (rowH));

    auto row3 = r.removeFromTop (rowH);
    eqSec.setBounds (row3.removeFromLeft (row3.getWidth() / 2));
    compSec.setBounds (row3);

    auto row4 = r;
    flSec.setBounds (row4.removeFromLeft (juce::roundToInt (row4.getWidth() * 0.4f)));
    masterSec.setBounds (row4);

    lfo1Sec.setBounds (right.removeFromTop (rowH));
    lfo2Sec.setBounds (right.removeFromTop (rowH));
    auto xy = right;
    xySec.setBounds (xy.removeFromTop (88));
    pad.setBounds (xy.reduced (6));

    auto mb = meterBox.getLocalBounds().reduced (4, 6);
    meterL.setBounds (mb.removeFromLeft (mb.getWidth() / 2).reduced (2, 0));
    meterR.setBounds (mb.reduced (2, 0));
}
