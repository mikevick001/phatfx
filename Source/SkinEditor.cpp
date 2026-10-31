#include "SkinEditor.h"
#include "Presets.h"

namespace phat
{
// ---------------------------------------------------------------------------
// Skin file parsing
// ---------------------------------------------------------------------------
juce::File findSkinFolder()
{
    auto data = findDataFolder();
    if (! data.isDirectory()) return {};
    auto def = data.getChildFile ("Skins/default");
    if (def.getChildFile ("SkinParameters.txt").existsAsFile()) return def;
    for (auto& d : data.getChildFile ("Skins").findChildFiles (juce::File::findDirectories, false))
        if (d.getChildFile ("SkinParameters.txt").existsAsFile()) return d;
    return {};
}

bool SkinData::load (const juce::File& dir)
{
    juce::StringArray lines;
    dir.getChildFile ("SkinParameters.txt").readLines (lines);
    if (lines.isEmpty()) return false;

    enum Section { NONE, BITMAPS, FONTS, PARAMETERS, PAGES, CONTROLS } section = NONE;
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto raw = lines[i].removeCharacters ("\r");
        const auto line = raw.trim();
        // Skip blanks, rules, column headings and section descriptions ("Fonts - ...").
        if (line.isEmpty() || line.startsWith ("-") || line.startsWith ("ControlName") || line.contains (" - ")) continue;
        if (line == "BITMAPS") { section = BITMAPS; continue; }
        if (line == "FONTS") { section = FONTS; continue; }
        if (line == "PARAMETERS") { section = PARAMETERS; continue; }
        if (line == "PAGES") { section = PAGES; continue; }
        if (line == "CONTROLS") { section = CONTROLS; continue; }

        auto tok = juce::StringArray::fromTokens (line, " \t", "");
        tok.removeEmptyStrings();
        switch (section)
        {
            case BITMAPS:
            {
                auto img = juce::ImageFileFormat::loadFrom (dir.getChildFile (line + ".png"));
                if (img.isValid()) images[line] = img;
                break;
            }
            case FONTS:
            {
                if (i + 3 >= lines.size() || ! lines[i + 1].trim().containsOnly ("0123456789")) break;
                BitmapFont f;
                f.image = juce::ImageFileFormat::loadFrom (dir.getChildFile (line + ".png"));
                const int count = lines[i + 1].trim().getIntValue();
                f.chars = lines[i + 2].removeCharacters ("\r").substring (0, count);
                auto w = juce::StringArray::fromTokens (lines[i + 3], " \t", "");
                w.removeEmptyStrings();
                int x = 0;
                for (auto& s : w) { f.offsets.push_back (x); f.widths.push_back (s.getIntValue()); x += s.getIntValue(); }
                if (f.image.isValid()) fonts[line] = f;
                i += 3;
                break;
            }
            case PARAMETERS:
                if (tok.size() >= 2) values[tok[0]] = tok[1].getIntValue();
                break;
            case PAGES:
            case CONTROLS:
            {
                if (tok.size() < 6) break;
                Control c;
                c.name = tok[0]; c.type = tok[1]; c.image = tok[2]; c.page = tok[3];
                c.x1 = tok[4].getIntValue(); c.y1 = tok[5].getIntValue();
                if (tok.size() >= 8) { c.x2 = tok[6].getIntValue(); c.y2 = tok[7].getIntValue(); }
                if (section == PAGES) pages[c.name] = c;
                else controls.push_back (c);
                break;
            }
            case NONE:
            default: break;
        }
    }
    return images.count ("Background") > 0 && ! controls.empty();
}

const SkinData::Control* SkinData::find (const juce::String& name) const
{
    for (auto& c : controls)
        if (c.name == name) return &c;
    return nullptr;
}

juce::Image SkinData::image (const juce::String& name) const
{
    auto it = images.find (name);
    return it != images.end() ? it->second : juce::Image();
}

int SkinData::value (const juce::String& name, int def) const
{
    auto it = values.find (name);
    return it != values.end() ? it->second : def;
}

int SkinData::textWidth (const juce::String& font, const juce::String& text) const
{
    auto it = fonts.find (font);
    if (it == fonts.end()) return 0;
    int w = 0;
    for (auto ch : text)
    {
        const int idx = it->second.chars.indexOfChar (ch);
        if (idx >= 0 && idx < (int) it->second.widths.size()) w += it->second.widths[(size_t) idx];
        else w += value (font + "Width", 6);
    }
    return w;
}

void SkinData::drawText (juce::Graphics& g, const juce::String& font, const juce::String& text,
                         juce::Rectangle<int> area, juce::Justification just) const
{
    auto it = fonts.find (font);
    if (it == fonts.end())
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions ((float) area.getHeight() * 0.8f));
        g.drawText (text, area, just);
        return;
    }
    const auto& f = it->second;
    const int h = f.image.getHeight();
    // Trim to fit.
    juce::String t = text;
    while (t.isNotEmpty() && textWidth (font, t) > area.getWidth()) t = t.dropLastCharacters (1);
    const int w = textWidth (font, t);
    int x = area.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred)) x = area.getCentreX() - w / 2;
    else if (just.testFlags (juce::Justification::right)) x = area.getRight() - w;
    const int y = area.getCentreY() - h / 2;
    for (auto ch : t)
    {
        const int idx = f.chars.indexOfChar (ch);
        if (idx < 0 || idx >= (int) f.widths.size()) { x += value (font + "Width", 6); continue; }
        const int cw = f.widths[(size_t) idx];
        g.drawImage (f.image, x, y, cw, h, f.offsets[(size_t) idx], 0, cw, h);
        x += cw;
    }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
static void drawFrame (juce::Graphics& g, const juce::Image& img, int frame, int numFrames, juce::Rectangle<int> dest)
{
    if (! img.isValid() || numFrames <= 0) return;
    const int fh = img.getHeight() / numFrames;
    frame = juce::jlimit (0, numFrames - 1, frame);
    g.drawImage (img, dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(), 0, frame * fh, img.getWidth(), fh);
}

void SkinLookAndFeel::drawRotarySlider (juce::Graphics& g, int, int, int, int, float pos, float, float, juce::Slider& s)
{
    auto img = skin.image (s.getProperties()["skinImage"].toString());
    if (! img.isValid()) return;
    const int frames = std::max (1, img.getHeight() / std::max (1, img.getWidth()));
    drawFrame (g, img, juce::roundToInt (pos * (float) (frames - 1)), frames, s.getLocalBounds());
}

void SkinLookAndFeel::drawLinearSlider (juce::Graphics& g, int, int, int, int, float sliderPos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider& s)
{
    auto img = skin.image (s.getProperties()["skinImage"].toString());
    if (! img.isValid()) return;
    const int x = (s.getWidth() - img.getWidth()) / 2;
    g.drawImageAt (img, x, juce::roundToInt (sliderPos) - img.getHeight() / 2);
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------
namespace
{
juce::String paramFor (const juce::String& skinName)
{
    using namespace params;
    static const std::map<juce::String, juce::String> m {
        { "BpFilterOn", bpOn }, { "BpFilterLow", bpLow }, { "BpFilterHigh", bpHigh }, { "BpFilterQLow", bpQLow },
        { "BpFilterQHigh", bpQHigh }, { "BpFilterBRMix", bpBRMix }, { "BpFilterLink", bpLink },
        { "DistOn", distOn }, { "DistBitCrusher", distBit }, { "DistMech", distMech }, { "DistTube", distTube },
        { "DistXcita", distXcita }, { "MmFilterOn", mmOn }, { "MmFilterMix", mmMix }, { "MmFilterType", mmType },
        { "MmFilterAttack", mmAttack }, { "MmFilterDecay", mmDecay }, { "MmFilterEnv", mmEnv },
        { "MmFilterCutoff", mmCutoff }, { "MmFilterRes", mmRes }, { "FlangerOn", flOn },
        { "FlangerAmount", flAmount }, { "FlangerRate", flRate }, { "MagicEQOn", eqOn },
        { "MagicEQAmount", eqAmount }, { "MagicEQTune", eqTune }, { "MagicEQMode", eqMode },
        { "CompressOn", compOn }, { "CompressAmount", compAmount }, { "CompressRelease", compRelease },
        { "CompressMode", compMode }, { "MasterMix", masterMix }, { "MasterVolume", masterVol },
        { "Lfo1On", lfo1On }, { "Lfo1Target", lfo1Target }, { "Lfo1Shape", lfo1Shape }, { "Lfo1Depth", lfo1Depth },
        { "Lfo1Rate", lfo1Rate }, { "Lfo1Sync", lfo1Sync }, { "Lfo2On", lfo2On }, { "Lfo2Target", lfo2Target },
        { "Lfo2Shape", lfo2Shape }, { "Lfo2Depth", lfo2Depth }, { "Lfo2Rate", lfo2Rate }, { "Lfo2Sync", lfo2Sync },
        { "MorphXParam", xyXTarget }, { "MorphYParam", xyYTarget } };
    auto it = m.find (skinName);
    return it != m.end() ? it->second : juce::String();
}

juce::Rectangle<int> centredOn (int cx, int cy, int w, int h) { return { cx - w / 2, cy - h / 2, w, h }; }

// Two-frame image button bound to a bool or two-choice parameter.
class SkinToggle : public juce::Component
{
public:
    SkinToggle (const SkinData& s, juce::RangedAudioParameter& p, const juce::String& img)
        : skin (s), param (p), image (img),
          attach (p, [this] (float) { repaint(); })
    {
        attach.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override { drawFrame (g, skin.image (image), param.getValue() > 0.5f ? 1 : 0, 2, getLocalBounds()); }
    void mouseDown (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.0f : 1.0f); }

    const SkinData& skin;
    juce::RangedAudioParameter& param;
    juce::String image;
    juce::ParameterAttachment attach;
};

// Arrow selector (left/right steps, centre opens a menu) for a choice parameter.
class SkinSelector : public juce::Component
{
public:
    SkinSelector (const SkinData& s, juce::AudioParameterChoice& p, const juce::String& img, int buttonWidth)
        : skin (s), param (p), image (img), buttonW (buttonWidth),
          attach (p, [this] (float) { repaint(); })
    {
        attach.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override
    {
        drawFrame (g, skin.image (image), pressed, 3, getLocalBounds());
        skin.drawText (g, "FontSelector", param.getCurrentChoiceName(),
                       getLocalBounds().reduced (buttonW + 2, 0), juce::Justification::centred);
    }
    void step (int d)
    {
        const int n = param.choices.size();
        attach.setValueAsCompleteGesture ((float) ((param.getIndex() + d + n) % n));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < buttonW) { pressed = 2; step (-1); }
        else if (e.x > getWidth() - buttonW) { pressed = 1; step (1); }
        else
        {
            juce::PopupMenu m;
            for (int i = 0; i < param.choices.size(); ++i)
                m.addItem (i + 1, param.choices[i], true, i == param.getIndex());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) {
                if (r > 0) attach.setValueAsCompleteGesture ((float) (r - 1));
            });
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { pressed = 0; repaint(); }

    const SkinData& skin;
    juce::AudioParameterChoice& param;
    juce::String image;
    int buttonW, pressed = 0;
    juce::ParameterAttachment attach;
};

class SkinMeter : public juce::Component
{
public:
    explicit SkinMeter (const SkinData& s) : skin (s) {}
    void paint (juce::Graphics& g) override
    {
        auto off = skin.image ("LevelMeterOff"), on = skin.image ("LevelMeterOn");
        const auto r = getLocalBounds();
        if (off.isValid()) g.drawImage (off, r.toFloat());
        const float db = juce::Decibels::gainToDecibels (level, -48.0f);
        const int lit = juce::roundToInt ((float) r.getHeight() * juce::jmap (db, -48.0f, 0.0f, 0.0f, 1.0f));
        if (on.isValid() && lit > 0)
        {
            const float sy = (float) on.getHeight() / (float) r.getHeight();
            g.drawImage (on, r.getX(), r.getBottom() - lit, r.getWidth(), lit, 0,
                         juce::roundToInt ((float) (r.getHeight() - lit) * sy), on.getWidth(), juce::roundToInt ((float) lit * sy));
        }
    }
    const SkinData& skin;
    float level = 0;
};

class SkinXY : public juce::Component
{
public:
    SkinXY (const SkinData& s, PhatFXProcessor& p) : skin (s), proc (p) {}
    juce::RangedAudioParameter* target (const juce::String& id) const
    {
        const int idx = (int) proc.apvts.getRawParameterValue (id)->load();
        return proc.apvts.getParameter (params::xyTargets()[juce::jlimit (0, params::xyTargets().size() - 1, idx)]);
    }
    void paint (juce::Graphics& g) override
    {
        g.drawImage (skin.image ("XYScreen"), getLocalBounds().toFloat());
        auto *tx = target (params::xyXTarget), *ty = target (params::xyYTarget);
        if (tx == nullptr || ty == nullptr) return;
        const auto r = getLocalBounds().toFloat().reduced (4.0f);
        const float px = r.getX() + tx->getValue() * r.getWidth();
        const float py = r.getBottom() - ty->getValue() * r.getHeight();
        g.setColour (juce::Colour ((juce::uint8) skin.value ("XYLineR", 172), (juce::uint8) skin.value ("XYLineG", 184),
                                   (juce::uint8) skin.value ("XYLineB", 255), (juce::uint8) skin.value ("XYLineA", 64)));
        g.drawVerticalLine (juce::roundToInt (px), r.getY(), r.getBottom());
        g.drawHorizontalLine (juce::roundToInt (py), r.getX(), r.getRight());
        auto cur = skin.image ("XYCursor");
        if (cur.isValid()) g.drawImageAt (cur, juce::roundToInt (px) - cur.getWidth() / 2, juce::roundToInt (py) - cur.getHeight() / 2);
    }
    void set (const juce::MouseEvent& e)
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f);
        if (dx) dx->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth()));
        if (dy && dy != dx) dy->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()));
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dx = target (params::xyXTarget); dy = target (params::xyYTarget);
        if (dx) dx->beginChangeGesture();
        if (dy && dy != dx) dy->beginChangeGesture();
        set (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dx) dx->endChangeGesture();
        if (dy && dy != dx) dy->endChangeGesture();
        dx = dy = nullptr;
    }
    const SkinData& skin;
    PhatFXProcessor& proc;
    juce::RangedAudioParameter *dx = nullptr, *dy = nullptr;
};

// Generic clickable image with a pressed frame.
class SkinKick : public juce::Component
{
public:
    SkinKick (const SkinData& s, const juce::String& img, int frames) : skin (s), image (img), numFrames (frames) {}
    void paint (juce::Graphics& g) override { drawFrame (g, skin.image (image), frame, numFrames, getLocalBounds()); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        frame = pressFrame ? pressFrame (e) : 1;
        repaint();
        if (onPress) onPress (e);
    }
    void mouseUp (const juce::MouseEvent&) override { frame = 0; repaint(); }
    const SkinData& skin;
    juce::String image;
    int numFrames, frame = 0;
    std::function<int (const juce::MouseEvent&)> pressFrame;
    std::function<void (const juce::MouseEvent&)> onPress;
};

class SkinTextArea : public juce::Component
{
public:
    explicit SkinTextArea (const SkinData& s) : skin (s) {}
    void paint (juce::Graphics& g) override
    {
        skin.drawText (g, "FontSelector", text, getLocalBounds().reduced (3, 0), juce::Justification::centredLeft);
    }
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    const SkinData& skin;
    juce::String text;
    std::function<void()> onClick;
};

class SkinBandBar : public juce::Component
{
public:
    SkinBandBar (const SkinData& s, juce::RangedAudioParameter& lo, juce::RangedAudioParameter& hi) : skin (s), low (lo), high (hi) {}
    void paint (juce::Graphics& g) override
    {
        auto img = skin.image ("VSliderBar");
        if (! img.isValid()) return;
        const float a = std::min (low.getValue(), high.getValue()), b = std::max (low.getValue(), high.getValue());
        const int h = getHeight();
        const int top = juce::roundToInt ((1.0f - b) * (float) h), bottom = juce::roundToInt ((1.0f - a) * (float) h);
        if (bottom <= top) return;
        const float sy = (float) img.getHeight() / (float) h;
        g.drawImage (img, 0, top, getWidth(), bottom - top, 0, juce::roundToInt ((float) top * sy), img.getWidth(),
                     juce::roundToInt ((float) (bottom - top) * sy));
    }
    const SkinData& skin;
    juce::RangedAudioParameter &low, &high;
};
} // namespace

// ---------------------------------------------------------------------------
// Canvas: the 1:1 skin surface, scaled by the editor.
// ---------------------------------------------------------------------------
class SkinnedEditor::Canvas : public juce::Component
{
public:
    Canvas (PhatFXProcessor& p, const SkinData& s, SkinLookAndFeel& l) : proc (p), skin (s), lnf (l), display (s)
    {
        auto bg = skin.image ("Background");
        setSize (bg.getWidth(), bg.getHeight());
        lfoPage = skin.pages.count ("LfoPg1") ? skin.pages.at ("LfoPg1") : SkinData::Control {};

        for (auto& c : skin.controls) build (c);

        meterL = addMeter ("LevelMeterL");
        meterR = addMeter ("LevelMeterR");

        if (auto* c = skin.find ("ParamDisplay"))
        {
            addAndMakeVisible (display);
            display.setBounds (c->x1, c->y1, c->x2 - c->x1, c->y2 - c->y1);
            display.onClick = [this] { showPresetMenu(); };
        }
        if (auto* lo = proc.apvts.getParameter (params::bpLow))
            if (auto* hi = proc.apvts.getParameter (params::bpHigh))
                if (auto* c = skin.find ("BpFilterBar"))
                {
                    auto img = skin.image ("VSliderBar");
                    bar = std::make_unique<SkinBandBar> (skin, *lo, *hi);
                    addAndMakeVisible (*bar);
                    bar->setBounds (centredOn (c->x1, c->y1, img.getWidth(), img.getHeight()));
                    bar->setInterceptsMouseClicks (false, false);
                }
        showLfo (0);
    }

    void paint (juce::Graphics& g) override
    {
        g.drawImageAt (skin.image ("Background"), 0, 0);
        if (lfoPage.isRect())
            g.drawImageAt (skin.image (lfoPage.image), lfoPage.x1, lfoPage.y1);
    }

    void tick()
    {
        if (meterL) { meterL->level = proc.meterL.load(); meterL->repaint(); }
        if (meterR) { meterR->level = proc.meterR.load(); meterR->repaint(); }
        if (xy) xy->repaint();
        if (bar) bar->repaint();

        // Value readout: parameter under the mouse/drag, otherwise the preset name.
        juce::String text;
        for (auto& [comp, id] : paramComponents)
            if (comp->isVisible() && (comp->isMouseOverOrDragging (true)))
                if (auto* p = proc.apvts.getParameter (id))
                    text = p->getName (32) + ": " + p->getCurrentValueAsText();
        if (text.isEmpty())
        {
            const auto& banks = proc.getBanks();
            if (proc.getCurrentPresetIndex() >= 0 && ! banks.empty())
                text = banks[(size_t) proc.getCurrentBank()].name + ": " + proc.getCurrentPresetName();
            else
                text = banks.empty() ? "PhatFX" : "Click to load a preset";
        }
        display.setText (text);
    }

private:
    SkinMeter* addMeter (const juce::String& name)
    {
        auto* c = skin.find (name);
        if (c == nullptr || ! c->isRect()) return nullptr;
        auto m = std::make_unique<SkinMeter> (skin);
        m->setBounds (c->x1, c->y1, c->x2 - c->x1, c->y2 - c->y1);
        m->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*m);
        auto* raw = m.get();
        owned.push_back (std::move (m));
        return raw;
    }

    void place (std::unique_ptr<juce::Component> comp, const SkinData::Control& c, const juce::String& paramId)
    {
        addAndMakeVisible (*comp);
        if (c.page == "LfoPg1") lfoComps[0].push_back (comp.get());
        if (c.page == "LfoPg2") lfoComps[1].push_back (comp.get());
        if (paramId.isNotEmpty()) paramComponents.push_back ({ comp.get(), paramId });
        owned.push_back (std::move (comp));
    }

    void build (const SkinData::Control& c)
    {
        const auto img = skin.image (c.image);
        const auto id = paramFor (c.name);
        auto* param = id.isNotEmpty() ? proc.apvts.getParameter (id) : nullptr;

        if (c.type == "Knob" && param != nullptr && img.isValid())
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
            s->getProperties().set ("skinImage", c.image);
            s->setLookAndFeel (&lnf);
            s->setMouseDragSensitivity (180);
            s->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getWidth()));
            s->setDoubleClickReturnValue (true, (double) param->getDefaultValue());
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
            place (std::move (s), c, id);
        }
        else if (c.type == "VSlider" && param != nullptr && img.isValid())
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            s->getProperties().set ("skinImage", c.image);
            s->setLookAndFeel (&lnf);
            const int cx = (c.x1 + std::max (c.x1, c.x2)) / 2;
            s->setBounds (cx - img.getWidth() / 2, c.y1 - 10, img.getWidth(), std::max (c.y2, c.y1 + 20) - c.y1 + 20);
            s->setDoubleClickReturnValue (true, (double) param->getDefaultValue());
            if (id == params::bpLow || id == params::bpHigh) bandSliders.push_back (s.get());
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
            place (std::move (s), c, id);
            if (bandSliders.size() == 2) setupLink();
        }
        else if (c.type == "OnButton" && param != nullptr && img.isValid())
        {
            auto t = std::make_unique<SkinToggle> (skin, *param, c.image);
            t->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight() / 2));
            place (std::move (t), c, id);
        }
        else if (c.type == "OnButton" && param == nullptr && img.isValid())
        {
            // e.g. MasterOn (no equivalent): show as permanently on.
            auto k = std::make_unique<SkinKick> (skin, c.image, 2);
            k->frame = 1;
            k->pressFrame = [] (const juce::MouseEvent&) { return 1; };
            k->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight() / 2));
            place (std::move (k), c, {});
        }
        else if (c.type == "Selector" && img.isValid())
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            {
                const bool small = c.image.startsWith ("SelectorScreen");
                auto s = std::make_unique<SkinSelector> (skin, *choice, c.image,
                                                         small ? skin.value ("SelectorButtonWidthSmall", 15)
                                                               : skin.value ("SelectorButtonWidth", 24));
                s->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight() / 3));
                place (std::move (s), c, id);
            }
        }
        else if (c.type == "MorphSquare" && img.isValid())
        {
            auto x = std::make_unique<SkinXY> (skin, proc);
            x->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight()));
            xy = x.get();
            place (std::move (x), c, {});
        }
        else if (c.type == "KickButton" && img.isValid())
        {
            auto k = std::make_unique<SkinKick> (skin, c.image, 2);
            k->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight() / 2));
            k->onPress = [this] (const juce::MouseEvent&) { proc.randomize(); };
            place (std::move (k), c, {});
        }
        else if (c.type == "UpDownPreset" && img.isValid())
        {
            auto k = std::make_unique<SkinKick> (skin, c.image, 3);
            k->setBounds (centredOn (c.x1, c.y1, img.getWidth(), img.getHeight() / 3));
            k->pressFrame = [h = img.getHeight() / 3] (const juce::MouseEvent& e) { return e.y < h / 2 ? 1 : 2; };
            k->onPress = [this, h = img.getHeight() / 3] (const juce::MouseEvent& e) { proc.stepPreset (e.y < h / 2 ? -1 : 1); };
            place (std::move (k), c, {});
        }
        else if (c.type == "RadioButton" && c.name == "LfoSelect")
        {
            const int space = skin.value ("LfoButtonSpace", 28);
            for (int i = 0; i < 2; ++i)
            {
                const auto name = "LfoSelect" + juce::String (i + 1);
                auto im = skin.image (name);
                if (! im.isValid()) continue;
                auto k = std::make_unique<SkinKick> (skin, name, 2);
                k->setBounds (c.x1 + i * space, c.y1, im.getWidth(), im.getHeight() / 2);
                k->onPress = [this, i] (const juce::MouseEvent&) { showLfo (i); };
                k->pressFrame = [] (const juce::MouseEvent&) { return 1; };
                lfoButtons[i] = k.get();
                place (std::move (k), c, {});
            }
        }
    }

    void showLfo (int which)
    {
        for (int p = 0; p < 2; ++p)
            for (auto* comp : lfoComps[p]) comp->setVisible (p == which);
        for (int i = 0; i < 2; ++i)
            if (lfoButtons[i] != nullptr)
            {
                lfoButtons[i]->setVisible (true);
                lfoButtons[i]->frame = i == which ? 1 : 0;
                lfoButtons[i]->repaint();
            }
        // The selector buttons must stay clickable regardless of page.
        for (int i = 0; i < 2; ++i)
            if (lfoButtons[i] != nullptr)
            {
                lfoButtons[i]->onPress = [this, i] (const juce::MouseEvent&) { showLfo (i); };
                lfoButtons[i]->pressFrame = [this, i] (const juce::MouseEvent&) { return 1; };
            }
        currentLfo = which;
    }

    void setupLink()
    {
        auto* lo = bandSliders[0];
        auto* hi = bandSliders[1];
        lastLo = (float) lo->getValue();
        lastHi = (float) hi->getValue();
        auto follow = [this] (juce::Slider* moved, juce::Slider* other, float* lastMoved, float* lastOther) {
            return [this, moved, other, lastMoved, lastOther] {
                const float v = (float) moved->getValue();
                if (! linking && proc.apvts.getRawParameterValue (params::bpLink)->load() > 0.5f)
                {
                    linking = true;
                    other->setValue (juce::jlimit (0.0, 1.0, other->getValue() + (v - *lastMoved)), juce::sendNotificationSync);
                    linking = false;
                }
                *lastMoved = v;
                *lastOther = (float) other->getValue();
            };
        };
        lo->onValueChange = follow (lo, hi, &lastLo, &lastHi);
        hi->onValueChange = follow (hi, lo, &lastHi, &lastLo);
    }

    void showPresetMenu()
    {
        const auto& banks = proc.getBanks();
        juce::PopupMenu menu;
        for (int b = 0; b < (int) banks.size(); ++b)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) banks[(size_t) b].presets.size(); ++i)
                sub.addItem (b * 1000 + i + 1, banks[(size_t) b].presets[(size_t) i].name, true,
                             b == proc.getCurrentBank() && i == proc.getCurrentPresetIndex());
            menu.addSubMenu (banks[(size_t) b].name, sub, true, nullptr, b == proc.getCurrentBank());
        }
        if (banks.empty()) menu.addItem (-1, "No preset banks found", false);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&display), [this] (int r) {
            if (r > 0) proc.loadPreset ((r - 1) / 1000, (r - 1) % 1000);
        });
    }

    PhatFXProcessor& proc;
    const SkinData& skin;
    SkinLookAndFeel& lnf;
    SkinData::Control lfoPage;
    SkinTextArea display;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::pair<juce::Component*, juce::String>> paramComponents;
    std::vector<juce::Component*> lfoComps[2];
    SkinKick* lfoButtons[2] = { nullptr, nullptr };
    std::vector<juce::Slider*> bandSliders;
    std::unique_ptr<SkinBandBar> bar;
    SkinMeter *meterL = nullptr, *meterR = nullptr;
    SkinXY* xy = nullptr;
    int currentLfo = 0;
    float lastLo = 0, lastHi = 1;
    bool linking = false;

public:
    ~Canvas() override
    {
        attachments.clear();
        for (auto& c : owned)
            if (auto* s = dynamic_cast<juce::Slider*> (c.get())) s->setLookAndFeel (nullptr);
    }
};

// ---------------------------------------------------------------------------
SkinnedEditor::SkinnedEditor (PhatFXProcessor& p, std::unique_ptr<SkinData> s)
    : AudioProcessorEditor (&p), skin (std::move (s))
{
    lnf = std::make_unique<SkinLookAndFeel> (*skin);
    canvas = std::make_unique<Canvas> (p, *skin, *lnf);
    addAndMakeVisible (*canvas);
    const int w = canvas->getWidth(), h = canvas->getHeight();
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) w / (double) h);
        c->setSizeLimits (w / 2, h / 2, w * 3, h * 3);
    }
    setSize (w, h);
    startTimerHz (30);
}

SkinnedEditor::~SkinnedEditor() { stopTimer(); }

void SkinnedEditor::resized()
{
    const float scale = (float) getWidth() / (float) std::max (1, canvas->getWidth());
    canvas->setTransform (juce::AffineTransform::scale (scale));
}

void SkinnedEditor::timerCallback() { canvas->tick(); }
} // namespace phat
