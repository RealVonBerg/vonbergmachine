#include "PluginEditor.h"

using namespace juce;

// =====================================================================================  palette
namespace C
{
const Colour bg       (0xff09090b);
const Colour panel    (0xff101014);
const Colour panelHi  (0xff19191f);
const Colour line     (0xff25252d);
const Colour lineHi   (0xff383843);
const Colour text     (0xffdedee6);
const Colour dim      (0xff74747f);
const Colour faint    (0xff44444e);
const Colour accent   (0xffe5163f);
const Colour accentHi (0xffff4467);
const Colour steel    (0xff6f8fd8);
const Colour amber    (0xffe89a2c);
const Colour violet   (0xff9064f2);
const Colour mute     (0xffd88b1f);
}

static Font F (float h, bool bold = true) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static Font FK (float h, float kern, bool bold = true) { return F (h, bold).withExtraKerningFactor (kern); }

static float textWidth (const Font& f, const String& t)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

static void panelBox (Graphics& g, Rectangle<float> r, float corner = 6.0f)
{
    ColourGradient gr (C::panelHi, r.getX(), r.getY(), C::panel, r.getX(), r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.expanded (0.6f), corner + 0.6f, 1.2f);
    g.setColour (C::line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

static void screw (Graphics& g, float cx, float cy)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - 4.5f, cy - 4.0f, 9.0f, 9.0f);
    ColourGradient gr (Colour (0xff4a4a54), cx - 3, cy - 3, Colour (0xff17171b), cx + 3, cy + 3, false);
    g.setGradientFill (gr);
    g.fillEllipse (cx - 3.6f, cy - 3.6f, 7.2f, 7.2f);
    g.setColour (Colour (0xff0b0b0d));
    g.drawLine (cx - 2.4f, cy - 1.0f, cx + 2.4f, cy + 1.0f, 1.1f);
}

static Colour sectionColour (int type)
{
    switch (type)
    {
        case vbm::SEC_INTRO:   return Colour (0xff6c7385);
        case vbm::SEC_DROP:    return C::accent;
        case vbm::SEC_STOP:    return Colour (0xff3a3a44);
        case vbm::SEC_HPBREAK: return C::steel;
        case vbm::SEC_LPBREAK: return C::violet;
        case vbm::SEC_BUILD:   return C::amber;
        default:               return Colour (0xff5d5d68);
    }
}

static Colour laneColour (int lane)
{
    switch (lane)
    {
        case vbm::KICK:  return C::accent;
        case vbm::BASS:  return Colour (0xffff2b55);
        case vbm::SNARE: return Colour (0xffeeeef4);
        case vbm::CLAP:  return Colour (0xffd2d2dc);
        case vbm::HAT:   return Colour (0xff9fb3e6);
        case vbm::OHAT:  return Colour (0xff7f97d6);
        case vbm::PERC:  return C::amber;
        case vbm::STOMP: return Colour (0xffff6a3d);
        default:         return C::violet;
    }
}

// =====================================================================================  look & feel
VbmLookAndFeel::VbmLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, C::bg);
    setColour (ComboBox::backgroundColourId, C::panelHi);
    setColour (ComboBox::textColourId, C::text);
    setColour (ComboBox::outlineColourId, C::line);
    setColour (ComboBox::arrowColourId, C::accent);
    setColour (PopupMenu::backgroundColourId, Colour (0xff131317));
    setColour (PopupMenu::textColourId, C::text);
    setColour (PopupMenu::highlightedBackgroundColourId, C::accent.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextButton::buttonColourId, C::panelHi);
    setColour (TextButton::buttonOnColourId, C::accent);
    setColour (TextButton::textColourOffId, C::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (Slider::textBoxTextColourId, C::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, C::text);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff17171c));
    setColour (TooltipWindow::textColourId, C::text);
    setColour (TooltipWindow::outlineColourId, C::lineHi);
    setColour (BubbleComponent::backgroundColourId, Colour (0xff17171c));
    setColour (BubbleComponent::outlineColourId, C::accent);
}

void VbmLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const float size = (float) jmin (w, h);
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float r = size * 0.5f - 2.5f;
    const float kr = r * 0.70f;
    const float thick = jmax (1.8f, size * 0.055f);
    const float ang = a0 + pos * (a1 - a0);
    const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float start = bip ? (a0 + a1) * 0.5f : a0;

    Path track;
    track.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, true);
    g.setColour (Colour (0xff1e1e24));
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    if (std::abs (ang - start) > 0.001f)
    {
        Path val;
        val.addCentredArc (cx, cy, r, r, 0.0f, jmin (start, ang), jmax (start, ang), true);
        g.setColour (C::accent.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (thick * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (C::accent);
        g.strokePath (val, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // body
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - kr - 1.0f, cy - kr + 1.5f, kr * 2.0f + 2.0f, kr * 2.0f + 2.0f);
    ColourGradient body (Colour (0xff34343c), cx, cy - kr, Colour (0xff101013), cx, cy + kr, false);
    g.setGradientFill (body);
    g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    ColourGradient cap (Colour (0xff26262d), cx - kr * 0.4f, cy - kr * 0.6f, Colour (0xff141418), cx + kr * 0.5f, cy + kr * 0.7f, true);
    g.setGradientFill (cap);
    g.fillEllipse (cx - kr * 0.80f, cy - kr * 0.80f, kr * 1.6f, kr * 1.6f);
    g.setColour (Colour (0xff3d3d47));
    g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 0.9f);

    // pointer
    Path p;
    p.addRoundedRectangle (-thick * 0.55f, -kr + 1.5f, thick * 1.1f, kr * 0.5f, thick * 0.5f);
    g.setColour (s.isMouseOverOrDragging() ? Colours::white : C::text);
    g.fillPath (p, AffineTransform::rotation (ang).translated (cx, cy));
}

void VbmLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& bgCol, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool accentButton = bgCol.getARGB() == C::accent.getARGB();
    const bool on = b.getToggleState() || accentButton;
    const Colour onCol = accentButton ? (over ? C::accentHi : C::accent) : b.findColour (TextButton::buttonOnColourId);
    ColourGradient gr (on ? onCol.brighter (0.15f) : Colour (0xff22222a), r.getX(), r.getY(),
                       on ? onCol.darker (0.35f) : Colour (0xff131317), r.getX(), r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, 4.0f);
    if (on)
    {
        g.setColour (onCol.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.0f), 5.0f, 2.0f);
    }
    g.setColour (on ? onCol.brighter (0.4f) : (over ? C::lineHi.brighter (0.3f) : C::lineHi));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (down) { g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, 4.0f); }
}

void VbmLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const bool accentButton = b.findColour (TextButton::buttonColourId).getARGB() == C::accent.getARGB();
    g.setColour ((b.getToggleState() || accentButton) ? Colours::white : C::text.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), Justification::centred, 1);
}

Font VbmLookAndFeel::getTextButtonFont (TextButton&, int h)
{
    return FK (jmin (12.5f, (float) h * 0.52f), 0.08f);
}

void VbmLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    ColourGradient gr (Colour (0xff1d1d23), 0, 0, Colour (0xff111114), 0, (float) h, false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, 3.5f);
    g.setColour (box.isMouseOver (true) ? C::lineHi.brighter (0.2f) : C::lineHi);
    g.drawRoundedRectangle (r, 3.5f, 1.0f);
    g.setColour (C::dim);
    g.setFont (FK (8.5f, 0.12f));
    g.drawText (box.getName(), Rectangle<float> (7.0f, 0.0f, 50.0f, (float) h), Justification::centredLeft);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (C::accent);
    g.fillPath (arrow);
}

Font VbmLookAndFeel::getComboBoxFont (ComboBox&) { return FK (11.5f, 0.06f); }

void VbmLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    const int left = box.getName().isNotEmpty() ? 50 : 8;
    label.setBounds (left, 1, box.getWidth() - left - 18, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (Justification::centredLeft);
}

Font VbmLookAndFeel::getPopupMenuFont() { return FK (13.0f, 0.04f); }

void VbmLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (Colour (0xff121216));
    g.setColour (C::lineHi);
    g.drawRect (0, 0, w, h, 1);
}

Label* VbmLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (F (11.0f));
    return l;
}

void VbmLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf0141418));
    g.fillRoundedRectangle (body, 3.0f);
    g.setColour (C::accent);
    g.drawRoundedRectangle (body.reduced (0.5f), 3.0f, 1.0f);
}

Font VbmLookAndFeel::getSliderPopupFont (Slider&) { return F (12.0f); }

// =====================================================================================  shared UI state
struct UiState
{
    int selBar = 0;
    bool follow = true;
    int playingBar() const { return playBar; }
    int playBar = -1;
    double playTick = -1.0;
};

// =====================================================================================  logo
class Logo : public Component
{
public:
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        // emblem: machined hexagon with a cut "V"
        const float h = r.getHeight() - 8.0f, cx = r.getX() + h * 0.5f + 4.0f, cy = r.getCentreY();
        Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = MathConstants<float>::pi / 3.0f * (float) i + MathConstants<float>::pi / 6.0f;
            const float px = cx + std::cos (a) * h * 0.5f, py = cy + std::sin (a) * h * 0.5f;
            if (i == 0) hex.startNewSubPath (px, py); else hex.lineTo (px, py);
        }
        hex.closeSubPath();
        ColourGradient gr (Colour (0xff2e2e36), cx, cy - h * 0.5f, Colour (0xff0d0d10), cx, cy + h * 0.5f, false);
        g.setGradientFill (gr);
        g.fillPath (hex);
        g.setColour (C::lineHi);
        g.strokePath (hex, PathStrokeType (1.2f));
        Path v;
        v.startNewSubPath (cx - h * 0.24f, cy - h * 0.20f);
        v.lineTo (cx, cy + h * 0.24f);
        v.lineTo (cx + h * 0.24f, cy - h * 0.20f);
        g.setColour (C::accent.withAlpha (0.35f));
        g.strokePath (v, PathStrokeType (6.0f, PathStrokeType::mitered, PathStrokeType::square));
        g.setColour (C::accent);
        g.strokePath (v, PathStrokeType (2.6f, PathStrokeType::mitered, PathStrokeType::square));

        // wordmark
        const float tx = cx + h * 0.5f + 12.0f;
        auto big = FK (25.0f, 0.10f);
        g.setFont (big);
        const String a ("VONBERG"), b ("MACHINE");
        const float wa = textWidth (big, a);
        g.setColour (C::text);
        g.drawText (a, Rectangle<float> (tx, cy - 19.0f, wa + 4.0f, 28.0f), Justification::centredLeft);
        g.setColour (C::accent);
        g.drawText (b, Rectangle<float> (tx + wa + 1.0f, cy - 19.0f, 200.0f, 28.0f), Justification::centredLeft);
        g.setColour (C::dim);
        g.setFont (FK (8.6f, 0.42f));
        g.drawText ("DRUM ARRANGEMENT ENGINE", Rectangle<float> (tx + 1.0f, cy + 9.0f, 260.0f, 12.0f), Justification::centredLeft);
    }
};

// =====================================================================================  pattern LCD
class PatternDisplay : public Component
{
public:
    explicit PatternDisplay (VbmProcessor& p) : proc (p) {}
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff050506));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (C::lineHi);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (r.reduced (1.5f), 3.5f, 1.0f);
        // scanlines
        g.setColour (Colours::white.withAlpha (0.018f));
        for (float y = r.getY() + 2.0f; y < r.getBottom() - 2.0f; y += 2.0f) g.drawHorizontalLine ((int) y, r.getX() + 3.0f, r.getRight() - 3.0f);

        auto song = proc.getSong();
        const bool rnd = proc.isRandomPattern();
        String big = rnd ? "RND" : String (proc.getBankIndex() + 1).paddedLeft ('0', 3);
        String sub = rnd ? ("SEED " + String::toHexString ((int) (song ? song->seed & 0xFFFFF : 0)).toUpperCase())
                         : String ("/ ") + String (vbm::BANK_SIZE);
        auto bigF = FK (30.0f, 0.06f);
        auto area = r.reduced (10.0f, 4.0f);
        auto left = area.removeFromLeft (rnd ? 70.0f : 66.0f);
        g.setFont (bigF);
        g.setColour (C::accent.withAlpha (0.25f));
        g.drawText (big, left.translated (0.0f, 1.0f).expanded (1.0f), Justification::centredLeft);
        g.setColour (C::accentHi);
        g.drawText (big, left, Justification::centredLeft);

        g.setFont (FK (9.0f, 0.15f));
        g.setColour (C::dim);
        g.drawText (sub, area.removeFromTop (area.getHeight() * 0.5f).withTrimmedLeft (4.0f), Justification::bottomLeft);
        g.setFont (FK (11.5f, 0.10f));
        g.setColour (C::text);
        g.drawText (song ? String (vbm::styleName (song->style)) : String(), area.withTrimmedLeft (4.0f), Justification::topLeft);
    }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        if (std::abs (w.deltaY) > 0.0f) proc.nextPattern (w.deltaY > 0 ? -1 : 1);
    }
private:
    VbmProcessor& proc;
};

// =====================================================================================  BPM + play
class BpmDisplay : public Component
{
public:
    explicit BpmDisplay (VbmProcessor& p) : proc (p) {}
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff050506));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (C::lineHi);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        const double bpm = proc.currentBpm.load();
        auto song = proc.getSong();
        g.setFont (FK (21.0f, 0.04f));
        g.setColour (C::text);
        g.drawText (String (bpm, 1), r.withTrimmedBottom (14.0f).withTrimmedTop (2.0f), Justification::centred);
        g.setFont (FK (8.0f, 0.2f));
        g.setColour (C::dim);
        const String src = proc.hostHasTempo.load() ? "HOST BPM" : "REF BPM";
        const String ref = song ? (" / REF " + String (roundToInt (song->refBpm))) : String();
        g.drawText (src + ref, r.removeFromBottom (16.0f), Justification::centred);
    }
private:
    VbmProcessor& proc;
};

class PlayButton : public Component, public SettableTooltipClient
{
public:
    PlayButton (VbmProcessor& p, UiState& s) : proc (p), st (s) { setTooltip ("Preview the arrangement without the host transport"); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool playing = proc.previewPlaying.load() || proc.hostIsPlaying.load();
        ColourGradient gr (playing ? C::accent.brighter (0.1f) : Colour (0xff24242b), r.getX(), r.getY(),
                           playing ? C::accent.darker (0.5f) : Colour (0xff121215), r.getX(), r.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (playing ? C::accentHi : C::lineHi);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        const float cx = r.getCentreX(), cy = r.getCentreY() - 5.0f;
        g.setColour (Colours::white);
        if (proc.previewPlaying.load())
            g.fillRoundedRectangle (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f, 2.0f);
        else
        {
            Path tri; tri.addTriangle (cx - 6.0f, cy - 8.0f, cx - 6.0f, cy + 8.0f, cx + 9.0f, cy);
            g.fillPath (tri);
        }
        g.setFont (FK (8.0f, 0.2f));
        g.setColour (playing ? Colours::white : C::dim);
        g.drawText (proc.hostIsPlaying.load() ? "HOST" : "PREVIEW", r.removeFromBottom (15.0f), Justification::centred);
    }
    void mouseDown (const MouseEvent&) override
    {
        if (proc.previewPlaying.load()) proc.previewPlaying.store (false);
        else
        {
            proc.setPreviewPosition (st.selBar);
            proc.previewPlaying.store (true);
        }
        repaint();
    }
private:
    VbmProcessor& proc;
    UiState& st;
};

// =====================================================================================  arrangement timeline
class Timeline : public Component, public SettableTooltipClient
{
public:
    Timeline (VbmProcessor& p, UiState& s) : proc (p), st (s)
    {
        setTooltip ("Arrangement: click a bar to edit it (and to jump there in preview)");
    }

    Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (12.0f, 6.0f).withTrimmedTop (18.0f).withTrimmedBottom (12.0f); }

    void paint (Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        panelBox (g, full);
        auto song = proc.getSong();
        g.setFont (FK (9.0f, 0.3f));
        g.setColour (C::dim);
        g.drawText ("ARRANGEMENT", Rectangle<float> (14.0f, 4.0f, 200.0f, 16.0f), Justification::centredLeft);
        if (song == nullptr) return;
        g.setColour (C::text.withAlpha (0.8f));
        g.drawText (String (song->name).toUpperCase() + "   |   " + String (song->numBars) + " BARS",
                    Rectangle<float> (full.getRight() - 420.0f, 4.0f, 406.0f, 16.0f), Justification::centredRight);

        const auto a = area();
        const int nb = song->numBars;
        const float bw = a.getWidth() / (float) nb;
        auto xOf = [&] (double tick) { return a.getX() + (float) (tick / vbm::TPBAR) * bw; };

        g.setColour (Colour (0xff070709));
        g.fillRect (a);

        // section blocks
        for (auto& sec : song->sections)
        {
            Rectangle<float> r (a.getX() + (float) sec.startBar * bw, a.getY(), (float) sec.numBars * bw, a.getHeight());
            const Colour c = sectionColour (sec.type);
            ColourGradient gr (c.withAlpha (sec.type == vbm::SEC_DROP ? 0.20f : 0.26f), r.getX(), r.getY(),
                               c.withAlpha (0.04f), r.getX(), r.getBottom(), false);
            g.setGradientFill (gr);
            g.fillRect (r.reduced (0.6f, 0.0f));
            g.setColour (c);
            g.fillRect (r.getX() + 0.6f, r.getY(), r.getWidth() - 1.2f, 2.5f);
            if (sec.type == vbm::SEC_STOP)
            {
                g.setColour (Colours::white.withAlpha (0.05f));
                for (float x = r.getX() - a.getHeight(); x < r.getRight(); x += 5.0f)
                {
                    Graphics::ScopedSaveState ss (g);
                    g.reduceClipRegion (r.toNearestInt());
                    g.drawLine (x, r.getBottom(), x + a.getHeight(), r.getY(), 1.0f);
                }
            }
            if (r.getWidth() > 26.0f)
            {
                g.setFont (FK (8.5f, 0.12f));
                g.setColour (c.brighter (0.4f));
                g.drawText (vbm::sectionName (sec.type), r.reduced (4.0f, 4.0f).withHeight (12.0f), Justification::topLeft, true);
            }
        }

        // bar grid
        for (int b = 0; b <= nb; ++b)
        {
            const float x = a.getX() + (float) b * bw;
            g.setColour (Colours::white.withAlpha (b % 8 == 0 ? 0.10f : (b % 4 == 0 ? 0.05f : 0.022f)));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            if (b % 8 == 0 && b < nb)
            {
                g.setFont (F (8.5f));
                g.setColour (C::dim);
                g.drawText (String (b + 1), Rectangle<float> (x + 2.0f, a.getBottom() + 1.0f, 30.0f, 11.0f), Justification::centredLeft);
            }
        }

        // event activity (3 rows at the bottom)
        const float rowH = a.getHeight() * 0.13f;
        const float base = a.getBottom() - 3.0f;
        for (int b = 0; b < nb; ++b)
            for (auto& e : song->bars[(size_t) b])
            {
                int row = 1;
                if (e.lane == vbm::KICK || e.lane == vbm::BASS) row = 0;
                else if (e.lane == vbm::HAT || e.lane == vbm::OHAT) row = 2;
                else if (e.lane == vbm::FX) continue;
                const float x = xOf ((double) (b * vbm::TPBAR + e.tick));
                const float y1 = base - (float) row * (rowH + 2.0f);
                g.setColour (laneColour (e.lane).withAlpha (0.25f + 0.55f * e.vel));
                if (e.lane == vbm::BASS)
                    g.fillRect (x, y1 - rowH * 0.45f, jmax (1.0f, (float) e.len / vbm::TPBAR * bw), rowH * 0.45f);
                else
                    g.drawVerticalLine ((int) x, y1 - rowH * e.vel, y1);
            }

        // automation lanes (upper part)
        const float top = a.getY() + 16.0f, bot = a.getBottom() - 3.0f * (rowH + 2.0f) - 4.0f;
        const int ns = (int) song->lp.size();
        auto sx = [&] (int step) { return a.getX() + (float) step / (float) vbm::STEPS * bw; };

        // reverb throws
        for (int i = 0; i < ns; ++i)
        {
            const float v = song->verb[(size_t) i];
            if (v > 1.05f)
            {
                g.setColour (Colours::white.withAlpha (jmin (0.12f, 0.03f * v)));
                g.fillRect (sx (i), top, bw / vbm::STEPS + 0.5f, bot - top);
            }
        }
        // riser
        {
            Path rp; bool open = false;
            for (int i = 0; i <= ns; ++i)
            {
                const float v = i < ns ? song->riser[(size_t) i] : 0.0f;
                const float y = bot - v * (bot - top) * 0.8f;
                if (v > 0.001f && ! open) { rp.startNewSubPath (sx (i), bot); open = true; }
                if (open) rp.lineTo (sx (i), y);
                if (open && v <= 0.001f) { rp.lineTo (sx (i), bot); rp.closeSubPath(); open = false; }
            }
            g.setColour (C::amber.withAlpha (0.35f));
            g.fillPath (rp);
        }
        auto curve = [&] (const std::vector<float>& v, bool isLp, Colour c)
        {
            Path p;
            bool any = false;
            for (int i = 0; i < ns; ++i)
            {
                const float val = v[(size_t) i];
                const float y = isLp ? top + (1.0f - val) * (bot - top) : bot - val * (bot - top);
                if (i == 0) p.startNewSubPath (sx (i), y); else p.lineTo (sx (i), y);
                any |= isLp ? val < 0.99f : val > 0.01f;
            }
            if (! any) return;
            g.setColour (c.withAlpha (0.22f));
            g.strokePath (p, PathStrokeType (3.5f));
            g.setColour (c);
            g.strokePath (p, PathStrokeType (1.3f));
        };
        curve (song->lp, true, C::accentHi);
        curve (song->hp, false, C::steel);
        // gain cuts & reverb kills
        for (int i = 0; i < ns; ++i)
        {
            if (song->kill[(size_t) i])
            {
                Path t; const float x = sx (i);
                t.addTriangle (x - 3.5f, a.getY() + 3.0f, x + 3.5f, a.getY() + 3.0f, x, a.getY() + 9.0f);
                g.setColour (Colours::white.withAlpha (0.85f));
                g.fillPath (t);
            }
            if (song->gain[(size_t) i] < 0.05f)
            {
                g.setColour (Colours::black.withAlpha (0.8f));
                g.fillRect (sx (i), a.getY() + 2.5f, bw / vbm::STEPS + 0.5f, a.getHeight() - 2.5f);
            }
        }

        // selected bar
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawRect (Rectangle<float> (a.getX() + (float) st.selBar * bw, a.getY(), bw, a.getHeight()), 1.0f);

        // playhead
        if (st.playTick >= 0.0)
        {
            const float x = xOf (st.playTick);
            g.setColour (C::accent.withAlpha (0.25f));
            g.fillRect (x - 3.0f, a.getY(), 6.0f, a.getHeight());
            g.setColour (Colours::white);
            g.fillRect (x - 0.75f, a.getY(), 1.5f, a.getHeight());
        }

        // legend
        g.setFont (FK (8.0f, 0.15f));
        float lx = 120.0f;
        auto legend = [&] (const String& t, Colour c)
        {
            g.setColour (c); g.fillRect (lx, 10.0f, 8.0f, 3.0f);
            g.setColour (C::dim); g.drawText (t, Rectangle<float> (lx + 11.0f, 4.0f, 70.0f, 15.0f), Justification::centredLeft);
            lx += 18.0f + textWidth (g.getCurrentFont(), t);
        };
        legend ("LOWPASS", C::accentHi); legend ("HIGHPASS", C::steel); legend ("RISER", C::amber);
        legend ("REVERB THROW", Colours::white.withAlpha (0.4f)); legend ("TAIL CUT", Colours::white);
    }

    void mouseDown (const MouseEvent& e) override { pick (e); }
    void mouseDrag (const MouseEvent& e) override { pick (e); }

private:
    void pick (const MouseEvent& e)
    {
        auto song = proc.getSong();
        if (song == nullptr) return;
        const auto a = area();
        const int bar = jlimit (0, song->numBars - 1, (int) ((e.position.x - a.getX()) / a.getWidth() * (float) song->numBars));
        st.selBar = bar;
        if (proc.previewPlaying.load()) proc.setPreviewPosition (bar);
        if (st.playTick >= 0.0) st.follow = false;
        if (auto* p = getParentComponent()) p->repaint();
    }

    VbmProcessor& proc;
    UiState& st;
};

// =====================================================================================  step grid
class StepGrid : public Component
{
public:
    static constexpr int headerH = 20, rowH = 31;

    StepGrid (VbmProcessor& p, UiState& s) : proc (p), st (s) {}

    Rectangle<float> cell (int lane, int step) const
    {
        const float cw = (float) getWidth() / (float) vbm::STEPS;
        return { (float) step * cw + 1.5f, (float) (headerH + lane * rowH) + 3.0f, cw - 3.0f, (float) rowH - 6.0f };
    }

    void paint (Graphics& g) override
    {
        auto song = proc.getSong();
        const float cw = (float) getWidth() / (float) vbm::STEPS;

        // header
        g.setFont (F (9.0f));
        for (int s = 0; s < vbm::STEPS; ++s)
        {
            g.setColour (s % 4 == 0 ? C::text.withAlpha (0.8f) : C::faint);
            g.drawText (String (s + 1), Rectangle<float> ((float) s * cw, 2.0f, cw, 14.0f), Justification::centred);
        }

        const int bar = jlimit (0, song ? song->numBars - 1 : 0, st.selBar);
        int playStep = -1;
        if (st.playTick >= 0.0 && st.playBar == bar) playStep = (int) (std::fmod (st.playTick, (double) vbm::TPBAR) / vbm::TPS);

        // background cells
        for (int l = 0; l < vbm::NUM_LANES; ++l)
            for (int s = 0; s < vbm::STEPS; ++s)
            {
                auto c = cell (l, s);
                const bool beat = (s / 4) % 2 == 0;
                g.setColour (beat ? Colour (0xff17171c) : Colour (0xff121216));
                g.fillRoundedRectangle (c, 2.5f);
                g.setColour (Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (c, 2.5f, 1.0f);
            }

        if (playStep >= 0)
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.fillRect (Rectangle<float> ((float) playStep * cw, (float) headerH, cw, (float) (rowH * vbm::NUM_LANES)));
            g.setColour (C::accent);
            g.fillRect (Rectangle<float> ((float) playStep * cw + 2.0f, 15.0f, cw - 4.0f, 2.0f));
        }

        if (song == nullptr) return;
        const auto& evs = song->bars[(size_t) bar];

        // 808 notes as bars with length
        for (auto& e : evs)
        {
            if (e.lane != vbm::BASS) continue;
            const int s0 = e.tick / vbm::TPS;
            auto c0 = cell (vbm::BASS, s0);
            const float len = jmax (cw * 0.5f, (float) e.len / vbm::TPS * cw - 3.0f);
            Rectangle<float> r (c0.getX() + (float) (e.tick % vbm::TPS) / vbm::TPS * cw, c0.getY() + 3.0f,
                                jmin (len, (float) getWidth() - c0.getX() - 2.0f), c0.getHeight() - 6.0f);
            const Colour lc = laneColour (vbm::BASS);
            ColourGradient gr (lc.withAlpha (0.95f), r.getX(), r.getY(), lc.darker (0.7f).withAlpha (0.9f), r.getRight(), r.getY(), false);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (r, 2.0f);
            if (std::abs (e.pitch2 - e.pitch) > 0.01f || (e.flags & vbm::EV_GLIDE))
            {
                g.setColour (Colours::white.withAlpha (0.8f));
                g.drawLine (r.getX() + 2.0f, r.getBottom() - 2.0f, r.getRight() - 2.0f, r.getY() + 2.0f, 1.0f);
            }
            if (std::abs (e.pitch) > 0.01f)
            {
                g.setColour (Colours::white);
                g.setFont (F (8.5f));
                g.drawText ((e.pitch > 0 ? "+" : "") + String (roundToInt (e.pitch)), r.reduced (3.0f, 0.0f), Justification::centredLeft);
            }
        }

        // drum hits: velocity brightness, rolls as dots
        for (int l = 0; l < vbm::NUM_LANES; ++l)
        {
            if (l == vbm::BASS) continue;
            float vmax[vbm::STEPS] = {};
            int cnt[vbm::STEPS] = {};
            for (auto& e : evs)
                if (e.lane == l)
                {
                    const int s = jlimit (0, vbm::STEPS - 1, e.tick / vbm::TPS);
                    vmax[s] = jmax (vmax[s], e.vel);
                    ++cnt[s];
                }
            const Colour lc = laneColour (l);
            for (int s = 0; s < vbm::STEPS; ++s)
            {
                if (cnt[s] == 0) continue;
                auto c = cell (l, s).reduced (1.5f);
                g.setColour (lc.withAlpha (0.18f + 0.30f * vmax[s]));
                g.fillRoundedRectangle (c.expanded (1.5f), 3.0f);
                ColourGradient gr (lc.withAlpha (0.35f + 0.65f * vmax[s]), c.getX(), c.getY(),
                                   lc.darker (0.6f).withAlpha (0.35f + 0.6f * vmax[s]), c.getX(), c.getBottom(), false);
                g.setGradientFill (gr);
                g.fillRoundedRectangle (c, 2.0f);
                if (cnt[s] > 1)
                {
                    g.setColour (Colours::black.withAlpha (0.75f));
                    const int dots = jmin (cnt[s], 6);
                    const float dw = (c.getWidth() - 6.0f) / (float) dots;
                    for (int d = 0; d < dots; ++d)
                        g.fillEllipse (c.getX() + 3.0f + dw * (float) d + dw * 0.5f - 1.6f, c.getBottom() - 6.0f, 3.2f, 3.2f);
                }
            }
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        const float cw = (float) getWidth() / (float) vbm::STEPS;
        const int s = (int) (e.position.x / cw);
        const int l = (int) ((e.position.y - headerH) / rowH);
        if (e.position.y < headerH || s < 0 || s >= vbm::STEPS || l < 0 || l >= vbm::NUM_LANES) return;
        st.follow = false;
        proc.toggleStep (st.selBar, s, l);
        repaint();
    }

private:
    VbmProcessor& proc;
    UiState& st;
};

// =====================================================================================  lane strip (drop target)
class LaneStrip : public Component, public FileDragAndDropTarget, public SettableTooltipClient
{
public:
    LaneStrip (VbmProcessor& p, int laneIndex) : proc (p), lane (laneIndex)
    {
        muteBtn.setClickingTogglesState (true);
        soloBtn.setClickingTogglesState (true);
        muteBtn.setColour (TextButton::buttonOnColourId, C::mute);
        soloBtn.setColour (TextButton::buttonOnColourId, C::steel);
        muteBtn.setTooltip ("Mute");
        soloBtn.setTooltip ("Solo");
        addAndMakeVisible (muteBtn);
        addAndMakeVisible (soloBtn);
        muteAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, VbmProcessor::laneParamId ("mute", lane), muteBtn);
        soloAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, VbmProcessor::laneParamId ("solo", lane), soloBtn);
        setTooltip ("Drag a sample here from the FL Studio browser (or click to load). Right-click: reset to built-in sound");
    }

    Rectangle<float> sampleBox() const { return { 84.0f, 3.0f, (float) getWidth() - 84.0f - 58.0f, (float) getHeight() - 6.0f }; }

    void resized() override
    {
        const int h = getHeight() - 8;
        soloBtn.setBounds (getWidth() - 26, 4, 24, h);
        muteBtn.setBounds (getWidth() - 53, 4, 24, h);
    }

    void tick()
    {
        const uint32_t hits = proc.engine.uiHits[(size_t) lane].load();
        bool changed = false;
        if (hits != lastHits) { lastHits = hits; flash = 1.0f; changed = true; }
        else if (flash > 0.01f) { flash *= 0.80f; changed = true; }
        auto smp = proc.getSampleData (lane);
        if (smp.get() != lastSample)
        {
            lastSample = smp.get();
            computePeaks (smp);
            changed = true;
        }
        if (changed) repaint();
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const Colour lc = laneColour (lane);
        // LED
        const float ly = r.getCentreY();
        g.setColour (lc.withAlpha (0.15f + 0.5f * flash));
        g.fillEllipse (6.0f - 4.0f * flash, ly - 4.0f - 4.0f * flash, 8.0f + 8.0f * flash, 8.0f + 8.0f * flash);
        g.setColour (flash > 0.05f ? lc.brighter (0.5f * flash) : lc.withAlpha (0.35f));
        g.fillEllipse (6.0f, ly - 4.0f, 8.0f, 8.0f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (6.0f, ly - 4.0f, 8.0f, 8.0f, 0.8f);

        g.setFont (FK (11.0f, 0.12f));
        g.setColour (C::text);
        g.drawText (vbm::laneName (lane), Rectangle<float> (20.0f, 0.0f, 64.0f, r.getHeight()), Justification::centredLeft);

        // sample box
        auto sb = sampleBox();
        g.setColour (Colour (0xff060608));
        g.fillRoundedRectangle (sb, 3.0f);
        g.setColour (hover ? C::accent : C::line);
        g.drawRoundedRectangle (sb.reduced (0.5f), 3.0f, hover ? 1.6f : 1.0f);

        // waveform
        if (! peaks.empty())
        {
            auto wf = sb.reduced (3.0f, 3.0f);
            const float mid = wf.getCentreY();
            const float step = wf.getWidth() / (float) peaks.size();
            g.setColour (lc.withAlpha (0.28f));
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float hh = jmax (0.5f, peaks[i] * wf.getHeight() * 0.5f);
                g.fillRect (wf.getX() + (float) i * step, mid - hh, jmax (1.0f, step - 0.5f), hh * 2.0f);
            }
        }
        const bool user = proc.isUserSample (lane);
        g.setFont (F (10.0f, user));
        g.setColour (user ? Colours::white : C::text.withAlpha (0.75f));
        g.drawText (proc.getSampleName (lane), sb.reduced (6.0f, 0.0f).withTrimmedRight (user ? 4.0f : 44.0f), Justification::centredLeft, true);
        if (! user)
        {
            g.setFont (FK (7.5f, 0.2f));
            g.setColour (C::dim);
            g.drawText ("BUILT-IN", sb.reduced (6.0f, 0.0f), Justification::centredRight);
        }
        if (hover)
        {
            g.setColour (C::accent.withAlpha (0.18f));
            g.fillRoundedRectangle (sb, 3.0f);
            g.setFont (FK (9.5f, 0.25f));
            g.setColour (Colours::white);
            g.drawText ("DROP TO LOAD", sb, Justification::centred);
        }
    }

    // ---- drag & drop from the FL Studio browser / explorer
    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& f : files) if (VbmProcessor::isAudioFile (f)) return true;
        return false;
    }
    void fileDragEnter (const StringArray&, int, int) override { hover = true; repaint(); }
    void fileDragExit (const StringArray&) override { hover = false; repaint(); }
    void filesDropped (const StringArray& files, int, int) override
    {
        hover = false;
        for (auto& f : files)
            if (VbmProcessor::isAudioFile (f) && proc.loadSampleFile (lane, File (f))) break;
        repaint();
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! sampleBox().contains (e.position) && ! e.mods.isPopupMenu()) return;
        if (e.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addSectionHeader (vbm::laneName (lane));
            m.addItem (1, "Load sample...");
            m.addItem (2, "Reset to built-in sound", proc.isUserSample (lane));
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r)
            {
                if (r == 1) openChooser();
                if (r == 2) { proc.clearSample (lane); repaint(); }
            });
            return;
        }
        openChooser();
    }

private:
    void openChooser()
    {
        chooser = std::make_unique<FileChooser> ("Load " + String (vbm::laneName (lane)) + " sample", File(),
                                                 "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile()) proc.loadSampleFile (lane, f);
            repaint();
        });
    }

    void computePeaks (const vbm::SamplePtr& s)
    {
        peaks.assign (60, 0.0f);
        if (s == nullptr || s->length() == 0) return;
        const int n = s->length();
        const int per = jmax (1, n / 60);
        float mx = 0.0001f;
        for (int i = 0; i < 60; ++i)
        {
            float p = 0.0f;
            for (int k = i * per; k < jmin (n, (i + 1) * per); k += 4) p = jmax (p, std::abs (s->L[(size_t) k]));
            peaks[(size_t) i] = p; mx = jmax (mx, p);
        }
        for (auto& p : peaks) p /= mx;
    }

    VbmProcessor& proc;
    const int lane;
    TextButton muteBtn { "M" }, soloBtn { "S" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> muteAtt, soloAtt;
    std::unique_ptr<FileChooser> chooser;
    std::vector<float> peaks;
    const void* lastSample = nullptr;
    uint32_t lastHits = 0;
    float flash = 0.0f;
    bool hover = false;
};

// =====================================================================================  knobs
static void setupKnob (Slider& s, Component* popupParent)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setPopupDisplayEnabled (true, true, popupParent, 1200);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (180);
}

class LaneKnobs : public Component
{
public:
    LaneKnobs (VbmProcessor& p, int lane, Component* popupParent)
    {
        const char* ids[4] = { "vol", "tune", "decay", "send" };
        for (int i = 0; i < 4; ++i)
        {
            auto& s = sl[(size_t) i];
            setupKnob (s, popupParent);
            const String id = VbmProcessor::laneParamId (ids[i], lane);
            att[(size_t) i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, s);
            if (auto* prm = p.apvts.getParameter (id))
                s.setDoubleClickReturnValue (true, p.apvts.getParameterRange (id).convertFrom0to1 (prm->getDefaultValue()));
            addAndMakeVisible (s);
        }
        sl[0].setTextValueSuffix (" dB");
        sl[1].setTextValueSuffix (" st");
    }
    void resized() override
    {
        const int w = getWidth() / 4;
        for (int i = 0; i < 4; ++i) sl[(size_t) i].setBounds (i * w + (w - 28) / 2, (getHeight() - 28) / 2, 28, 28);
    }
private:
    std::array<Slider, 4> sl;
    std::array<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>, 4> att;
};

class KnobBlock : public Component
{
public:
    KnobBlock (VbmProcessor& p, const String& paramId, const String& label, Component* popupParent, const String& suffix = {})
        : name (label)
    {
        setupKnob (slider, popupParent);
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
        if (auto* prm = p.apvts.getParameter (paramId))
            slider.setDoubleClickReturnValue (true, p.apvts.getParameterRange (paramId).convertFrom0to1 (prm->getDefaultValue()));
        slider.setTextValueSuffix (suffix);
        addAndMakeVisible (slider);
    }
    void resized() override
    {
        const int k = jmin (getWidth() - 8, getHeight() - 22);
        slider.setBounds ((getWidth() - k) / 2, 2, k, k);
    }
    void paint (Graphics& g) override
    {
        g.setFont (FK (9.0f, 0.18f));
        g.setColour (C::text.withAlpha (0.85f));
        g.drawText (name, getLocalBounds().removeFromBottom (16), Justification::centred);
    }
    Slider slider;
private:
    String name;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
};

// =====================================================================================  sidechain view + meter
class DuckView : public Component
{
public:
    explicit DuckView (VbmProcessor& p) : proc (p) {}
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff060608));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (C::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        const float depth = proc.apvts.getRawParameterValue ("scDepth")->load();
        const float rel = proc.apvts.getRawParameterValue ("scRelease")->load();
        const double bpm = jmax (40.0, proc.currentBpm.load());
        const float beat = (float) (60.0 / bpm);
        auto a = r.reduced (5.0f, 6.0f);
        Path p;
        for (int i = 0; i <= 64; ++i)
        {
            const float t = (float) i / 64.0f * 2.0f * beat;   // two beats
            const float tb = std::fmod (t, beat);
            const float e = std::exp (-tb / jmax (0.02f, rel));
            const float env = e * e * (3.0f - 2.0f * e);
            const float gval = 1.0f - depth * env;
            const float x = a.getX() + (float) i / 64.0f * a.getWidth();
            const float y = a.getBottom() - gval * a.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        Path fillP (p);
        fillP.lineTo (a.getRight(), a.getBottom()); fillP.lineTo (a.getX(), a.getBottom()); fillP.closeSubPath();
        g.setColour (C::accent.withAlpha (0.18f));
        g.fillPath (fillP);
        g.setColour (C::accent);
        g.strokePath (p, PathStrokeType (1.5f));
        // live duck amount
        const float d = jlimit (0.0f, 1.0f, proc.engine.uiDuck.load());
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRect (a.getRight() + 1.0f, a.getBottom() - d * a.getHeight(), 3.0f, d * a.getHeight());
        g.setFont (FK (7.5f, 0.2f));
        g.setColour (C::dim);
        g.drawText ("808 PUMP", r.reduced (5.0f, 2.0f), Justification::topLeft);
    }
private:
    VbmProcessor& proc;
};

class Meter : public Component
{
public:
    explicit Meter (VbmProcessor& p) : proc (p) {}
    void tick()
    {
        const float pk = proc.engine.uiPeak.load();
        level = jmax (pk, level * 0.88f);
        hold = pk >= hold ? pk : hold * 0.995f;
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff060608));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (C::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
        auto a = r.reduced (3.0f);
        auto toY = [&] (float v) { const float db = Decibels::gainToDecibels (v, -48.0f); return a.getBottom() - (db + 48.0f) / 48.0f * a.getHeight(); };
        const int segs = 24;
        for (int i = 0; i < segs; ++i)
        {
            const float y0 = a.getBottom() - (float) (i + 1) / segs * a.getHeight();
            const float db = -48.0f + 48.0f * (float) (i + 1) / segs;
            const bool lit = toY (level) <= y0 + 1.0f;
            Colour c = db > -3.0f ? C::accentHi : (db > -12.0f ? C::amber : Colour (0xff9aa3b5));
            g.setColour (lit ? c : c.withAlpha (0.08f));
            g.fillRect (a.getX(), y0 + 0.8f, a.getWidth(), a.getHeight() / segs - 1.6f);
        }
        g.setColour (Colours::white);
        g.fillRect (a.getX(), toY (hold), a.getWidth(), 1.5f);
    }
private:
    VbmProcessor& proc;
    float level = 0.0f, hold = 0.0f;
};

// =====================================================================================  drag export
class DragExport : public Component, public SettableTooltipClient
{
public:
    DragExport (VbmProcessor& p, bool audioMode) : proc (p), audio (audioMode)
    {
        setTooltip (audio ? "Drag the rendered arrangement (WAV, filters + reverb included) into the FL playlist. Click to show the file."
                          : "Drag the arrangement as MIDI (drums + 808 notes) into FL. Click to show the file.");
        setMouseCursor (MouseCursor::DraggingHandCursor);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        ColourGradient gr (over ? Colour (0xff2a1016) : Colour (0xff1d1d23), r.getX(), r.getY(), Colour (0xff111114), r.getX(), r.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (over ? C::accent : C::lineHi);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        // icon
        auto ic = r.removeFromLeft (r.getHeight()).reduced (9.0f);
        g.setColour (C::accent);
        if (audio)
        {
            Path w;
            for (int i = 0; i <= 20; ++i)
            {
                const float x = ic.getX() + ic.getWidth() * (float) i / 20.0f;
                const float amp = std::sin ((float) i * 1.7f) * std::exp (-std::abs ((float) i - 8.0f) * 0.12f);
                const float y = ic.getCentreY() - amp * ic.getHeight() * 0.45f;
                if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
            }
            g.strokePath (w, PathStrokeType (1.6f));
        }
        else
        {
            g.drawEllipse (ic, 1.6f);
            for (int i = 0; i < 5; ++i)
            {
                const float a = MathConstants<float>::pi * (1.0f + (float) i / 4.0f);
                g.fillEllipse (ic.getCentreX() + std::cos (a) * ic.getWidth() * 0.28f - 1.4f,
                               ic.getCentreY() + std::sin (a) * ic.getHeight() * 0.28f - 1.4f + 2.0f, 2.8f, 2.8f);
            }
        }
        g.setFont (FK (10.5f, 0.15f));
        g.setColour (C::text);
        g.drawText (busy ? "RENDERING..." : (audio ? "DRAG AUDIO" : "DRAG MIDI"), r.removeFromTop (r.getHeight() * 0.58f), Justification::bottomLeft);
        g.setFont (FK (7.5f, 0.15f));
        g.setColour (C::dim);
        g.drawText (audio ? "FULL MIX  .WAV" : "DRUMS + 808  .MID", r, Justification::topLeft);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const MouseEvent&) override { dragged = false; }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragged || e.getDistanceFromDragStart() < 6) return;
        dragged = true;
        auto f = makeFile();
        if (f.existsAsFile())
            DragAndDropContainer::performExternalDragDropOfFiles (StringArray (f.getFullPathName()), false, this);
    }
    void mouseUp (const MouseEvent&) override
    {
        if (! dragged)
        {
            auto f = makeFile();
            if (f.existsAsFile()) f.revealToUser();
        }
        dragged = false;
    }
private:
    File makeFile()
    {
        busy = true; repaint();
        auto f = audio ? proc.renderAudioFile() : proc.writeMidiFile();
        busy = false; repaint();
        return f;
    }
    VbmProcessor& proc;
    bool audio, over = false, dragged = false, busy = false;
};

// =====================================================================================  main content
class VbmContent : public Component, private Timer
{
public:
    explicit VbmContent (VbmProcessor& p)
        : proc (p), patDisp (p), bpm (p), play (p, st), timeline (p, st), grid (p, st),
          duck (p), meter (p), dragMidi (p, false), dragAudio (p, true)
    {
        buildBackground();

        addAndMakeVisible (logo);
        addAndMakeVisible (patDisp);
        addAndMakeVisible (prevBtn);
        addAndMakeVisible (nextBtn);
        addAndMakeVisible (randomBtn);
        prevBtn.onClick = [this] { proc.nextPattern (-1); refreshAll(); };
        nextBtn.onClick = [this] { proc.nextPattern (1); refreshAll(); };
        randomBtn.onClick = [this] { proc.randomPattern(); refreshAll(); };
        randomBtn.setColour (TextButton::buttonColourId, C::accent);
        randomBtn.setTooltip ("Generate a brand-new arrangement in the selected style");
        prevBtn.setTooltip ("Previous factory pattern (500)");
        nextBtn.setTooltip ("Next factory pattern (500)");

        auto setupBox = [this] (ComboBox& b, const String& name, const String& id, std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment>& att)
        {
            b.setName (name);
            if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id)))
                b.addItemList (c->choices, 1);
            att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, b);
            addAndMakeVisible (b);
        };
        setupBox (styleBox, "STYLE", "style", styleAtt);
        setupBox (barsBox, "LENGTH", "bars", barsAtt);
        setupBox (keyBox, "808 KEY", "key", keyAtt);

        freeRunBtn.setClickingTogglesState (true);
        freeRunBtn.setTooltip ("Ignore the host song position and run the arrangement from the moment playback starts (use in FL pattern mode)");
        freeRunAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "freeRun", freeRunBtn);
        addAndMakeVisible (freeRunBtn);

        addAndMakeVisible (bpm);
        addAndMakeVisible (play);
        addAndMakeVisible (timeline);
        addAndMakeVisible (grid);

        followBtn.setClickingTogglesState (true);
        followBtn.setToggleState (true, dontSendNotification);
        followBtn.onClick = [this] { st.follow = followBtn.getToggleState(); };
        followBtn.setTooltip ("Step editor follows the playhead");
        addAndMakeVisible (followBtn);

        for (int l = 0; l < vbm::NUM_LANES; ++l)
        {
            addAndMakeVisible (strips.add (new LaneStrip (proc, l)));
            addAndMakeVisible (knobs.add (new LaneKnobs (proc, l, this)));
        }

        auto addK = [this] (const String& id, const String& label, const String& suffix = {})
        {
            auto* k = fx.add (new KnobBlock (proc, id, label, this, suffix));
            addAndMakeVisible (k);
            return k;
        };
        addK ("swing", "SWING"); addK ("humanize", "HUMANIZE"); addK ("glide", "808 GLIDE", " s");
        addK ("scDepth", "DEPTH"); addK ("scRelease", "RELEASE", " s");
        addK ("verbSize", "SIZE"); addK ("verbMix", "MIX");
        addK ("filterMoves", "FILTER MOVES"); addK ("riser", "RISER");
        addK ("drive", "DRIVE"); addK ("master", "OUTPUT", " dB");

        transBtn.setClickingTogglesState (true);
        transBtn.setTooltip ("Automatic transitions: LP/HP filter moves, reverb throws + tail cuts, risers, volume cuts");
        transAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "transitions", transBtn);
        addAndMakeVisible (transBtn);

        addAndMakeVisible (duck);
        addAndMakeVisible (meter);
        addAndMakeVisible (dragMidi);
        addAndMakeVisible (dragAudio);

        startTimerHz (30);
    }

    ~VbmContent() override { stopTimer(); }

    void paint (Graphics& g) override
    {
        g.drawImageAt (bgImage, 0, 0);

        // header plate
        ColourGradient hg (Colour (0xff17171c), 0, 0, Colour (0xff0c0c0f), 0, 72, false);
        g.setGradientFill (hg);
        g.fillRect (0, 0, getWidth(), 72);
        ColourGradient al (C::accent.withAlpha (0.0f), 0.0f, 72.0f, C::accent.withAlpha (0.0f), (float) getWidth(), 72.0f, false);
        al.addColour (0.5, C::accent);
        g.setGradientFill (al);
        g.fillRect (0, 71, getWidth(), 1);
        g.setColour (Colours::black);
        g.fillRect (0, 72, getWidth(), 1);
        screw (g, 9, 9); screw (g, (float) getWidth() - 9, 9);

        // lane block
        panelBox (g, laneArea.toFloat());
        g.setFont (FK (9.0f, 0.3f));
        g.setColour (C::dim);
        g.drawText ("KIT  /  DROP SAMPLES FROM FL BROWSER", laneArea.getX() + 14, laneArea.getY() + 5, 320, 14, Justification::centredLeft);
        auto song = proc.getSong();
        if (song != nullptr)
        {
            const int bar = jlimit (0, song->numBars - 1, st.selBar);
            const int si = song->sectionAt (bar);
            const String secName = si >= 0 ? vbm::sectionName (song->sections[(size_t) si].type) : "";
            g.setColour (C::text);
            g.drawText ("BAR " + String (bar + 1) + " / " + String (song->numBars) + "   " + secName,
                        grid.getX(), laneArea.getY() + 5, 300, 14, Justification::centredLeft);
        }
        const char* kn[4] = { "VOL", "TUNE", "DECAY", "REVERB" };
        const int kw = knobsArea.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            g.drawText (kn[i], knobsArea.getX() + i * kw, laneArea.getY() + 5 + 18, kw, 12, Justification::centred);
        // row separators
        for (int l = 1; l < vbm::NUM_LANES; ++l)
        {
            const int y = grid.getY() + StepGrid::headerH + l * StepGrid::rowH;
            g.setColour (Colours::black.withAlpha (0.5f));
            g.drawHorizontalLine (y, (float) laneArea.getX() + 8, (float) laneArea.getRight() - 8);
            g.setColour (Colours::white.withAlpha (0.025f));
            g.drawHorizontalLine (y + 1, (float) laneArea.getX() + 8, (float) laneArea.getRight() - 8);
        }

        // bottom groups
        for (auto& gp : groups)
        {
            panelBox (g, gp.r.toFloat());
            g.setFont (FK (9.0f, 0.3f));
            g.setColour (C::dim);
            g.drawText (gp.title, gp.r.getX() + 12, gp.r.getY() + 6, gp.r.getWidth() - 20, 14, Justification::centredLeft);
            g.setColour (C::accent);
            g.fillRect (gp.r.getX() + 12, gp.r.getY() + 21, 14, 2);
        }
        g.setFont (FK (8.0f, 0.25f));
        g.setColour (C::faint);
        g.drawText ("VONBERGMACHINE  v1.0", getWidth() - 220, getHeight() - 14, 206, 12, Justification::centredRight);
    }

    void resized() override
    {
        logo.setBounds (16, 9, 340, 54);
        prevBtn.setBounds (372, 20, 28, 34);
        patDisp.setBounds (404, 13, 196, 48);
        nextBtn.setBounds (604, 20, 28, 34);
        randomBtn.setBounds (642, 15, 118, 44);
        styleBox.setBounds (776, 13, 150, 22);
        barsBox.setBounds (932, 13, 120, 22);
        keyBox.setBounds (776, 39, 150, 22);
        freeRunBtn.setBounds (932, 39, 120, 22);
        bpm.setBounds (1060, 13, 72, 48);
        play.setBounds (1138, 13, 48, 48);

        timeline.setBounds (14, 82, 1172, 142);

        laneArea = { 14, 232, 1172, 318 };
        const int top = laneArea.getY() + 24;          // grid header starts here
        const int rows0 = top + StepGrid::headerH;
        grid.setBounds (360, top, 528, StepGrid::headerH + StepGrid::rowH * vbm::NUM_LANES);
        followBtn.setBounds (820, laneArea.getY() + 4, 64, 17);
        knobsArea = { 900, rows0, 278, StepGrid::rowH * vbm::NUM_LANES };
        for (int l = 0; l < vbm::NUM_LANES; ++l)
        {
            strips[l]->setBounds (22, rows0 + l * StepGrid::rowH, 330, StepGrid::rowH);
            knobs[l]->setBounds (knobsArea.getX(), rows0 + l * StepGrid::rowH, knobsArea.getWidth(), StepGrid::rowH);
        }

        // bottom groups
        groups.clear();
        const int gy = 560, gh = 206;
        auto group = [this, gy, gh] (int x, int w, const String& t) { groups.push_back ({ Rectangle<int> (x, gy, w, gh), t }); return Rectangle<int> (x, gy, w, gh); };
        auto kb = [] (Rectangle<int> g, int i, int n, int w = 66) { const int x0 = g.getX() + 10 + i * w; (void) n; return Rectangle<int> (x0, g.getY() + 46, w, 90); };

        auto gGroove = group (14, 222, "GROOVE");
        fx[0]->setBounds (kb (gGroove, 0, 3, 68)); fx[1]->setBounds (kb (gGroove, 1, 3, 68)); fx[2]->setBounds (kb (gGroove, 2, 3, 68));
        auto gSc = group (244, 238, "SIDECHAIN");
        fx[3]->setBounds (kb (gSc, 0, 2)); fx[4]->setBounds (kb (gSc, 1, 2));
        duck.setBounds (gSc.getX() + 146, gSc.getY() + 50, 82, 70);
        auto gSp = group (490, 156, "SPACE");
        fx[5]->setBounds (kb (gSp, 0, 2)); fx[6]->setBounds (kb (gSp, 1, 2));
        auto gTr = group (654, 228, "TRANSITIONS");
        fx[7]->setBounds (kb (gTr, 0, 2, 74)); fx[8]->setBounds (kb (gTr, 1, 2, 74));
        transBtn.setBounds (gTr.getX() + 160, gTr.getY() + 64, 58, 40);
        auto gMa = group (890, 162, "MASTER");
        fx[9]->setBounds (kb (gMa, 0, 2)); fx[10]->setBounds (kb (gMa, 1, 2));
        meter.setBounds (gMa.getRight() - 22, gMa.getY() + 30, 12, gh - 44);
        auto gEx = group (1060, 126, "EXPORT");
        dragMidi.setBounds (gEx.getX() + 10, gEx.getY() + 36, gEx.getWidth() - 20, 50);
        dragAudio.setBounds (gEx.getX() + 10, gEx.getY() + 94, gEx.getWidth() - 20, 50);

        transBtn.setButtonText ("ON");
    }

private:
    void refreshAll()
    {
        st.selBar = 0;
        repaint();
    }

    void timerCallback() override
    {
        const double tick = proc.engine.uiTick.load();
        auto song = proc.getSong();
        st.playTick = (song != nullptr && tick >= 0.0) ? tick : -1.0;
        st.playBar = st.playTick >= 0.0 ? (int) (st.playTick / vbm::TPBAR) : -1;
        if (st.playTick < 0.0 && ! st.follow) { /* keep user selection */ }
        if (st.follow && st.playBar >= 0) st.selBar = st.playBar;
        if (st.playTick < 0.0 && ! followBtn.getToggleState()) st.follow = false;
        if (followBtn.getToggleState() != st.follow) followBtn.setToggleState (st.follow, dontSendNotification);
        if (song != nullptr) st.selBar = jlimit (0, song->numBars - 1, st.selBar);

        const int ver = proc.songVersion.load();
        if (ver != lastVersion || st.selBar != lastSel)
        {
            lastVersion = ver; lastSel = st.selBar;
            patDisp.repaint();
            repaint (laneArea.withHeight (24));
        }
        timeline.repaint();
        grid.repaint();
        for (auto* s : strips) s->tick();
        meter.tick();
        duck.repaint();
        bpm.repaint();
        play.repaint();
        const String tt = transBtn.getToggleState() ? "ON" : "OFF";
        if (transBtn.getButtonText() != tt) transBtn.setButtonText (tt);
    }

    void buildBackground()
    {
        bgImage = Image (Image::ARGB, VbmEditor::baseW, VbmEditor::baseH, true);
        Graphics g (bgImage);
        ColourGradient grad (Colour (0xff131317), (float) VbmEditor::baseW * 0.5f, 0.0f, Colour (0xff060607), (float) VbmEditor::baseW * 0.5f, (float) VbmEditor::baseH, false);
        g.setGradientFill (grad);
        g.fillAll();
        Random r (7);
        for (int y = 0; y < VbmEditor::baseH; ++y)
        {
            g.setColour (Colours::white.withAlpha (0.006f + r.nextFloat() * 0.016f));
            g.drawHorizontalLine (y, 0.0f, (float) VbmEditor::baseW);
        }
        for (int i = 0; i < 14000; ++i)
        {
            g.setColour ((r.nextBool() ? Colours::white : Colours::black).withAlpha (r.nextFloat() * 0.05f));
            g.fillRect ((float) r.nextInt (VbmEditor::baseW), (float) r.nextInt (VbmEditor::baseH), 1.0f, 1.0f);
        }
        ColourGradient vig (Colours::transparentBlack, (float) VbmEditor::baseW * 0.5f, (float) VbmEditor::baseH * 0.45f,
                            Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
        g.setGradientFill (vig);
        g.fillAll();
    }

    VbmProcessor& proc;
    UiState st;
    Image bgImage;
    TooltipWindow tooltips { this, 700 };

    Logo logo;
    PatternDisplay patDisp;
    TextButton prevBtn { "<" }, nextBtn { ">" }, randomBtn { "RANDOM" };
    ComboBox styleBox, barsBox, keyBox;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt, barsAtt, keyAtt;
    TextButton freeRunBtn { "FREE RUN" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> freeRunAtt;
    BpmDisplay bpm;
    PlayButton play;
    Timeline timeline;
    StepGrid grid;
    TextButton followBtn { "FOLLOW" };
    OwnedArray<LaneStrip> strips;
    OwnedArray<LaneKnobs> knobs;
    OwnedArray<KnobBlock> fx;
    TextButton transBtn { "ON" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> transAtt;
    DuckView duck;
    Meter meter;
    DragExport dragMidi, dragAudio;

    struct Group { Rectangle<int> r; String title; };
    std::vector<Group> groups;
    Rectangle<int> laneArea, knobsArea;
    int lastVersion = -1, lastSel = -1;
};

// =====================================================================================  editor
VbmEditor::VbmEditor (VbmProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    content = std::make_unique<VbmContent> (p);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
}

VbmEditor::~VbmEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void VbmEditor::paint (Graphics& g)
{
    g.fillAll (C::bg);
}

void VbmEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content->setBounds (0, 0, baseW, baseH);
    content->setTransform (AffineTransform::scale (s));
}
