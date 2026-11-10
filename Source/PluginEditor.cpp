#include "PluginEditor.h"
#include <BinaryData.h>

using namespace ui;
namespace an = swell::analog;

namespace
{
juce::String fr (const char* s) { return juce::String::fromUTF8 (s); }

juce::Colour moduleColour (chain::Type t) { return juce::Colour (chain::info (t).colour); }

/** Capitals for the panel, French accents included (toUpperCase depends on the system locale). */
juce::String upper (const juce::String& s)
{
    auto u = s.toUpperCase();
    const char* pairs[][2] { { "é", "É" }, { "è", "È" }, { "ê", "Ê" }, { "ë", "Ë" }, { "à", "À" }, { "â", "Â" }, { "ù", "Ù" },
                             { "û", "Û" }, { "ü", "Ü" }, { "î", "Î" }, { "ï", "Ï" }, { "ô", "Ô" }, { "ç", "Ç" } };
    for (auto& pr : pairs) u = u.replace (juce::String::fromUTF8 (pr[0]), juce::String::fromUTF8 (pr[1]));
    return u;
}

juce::String styleUpper (const char* s) { return upper (fr (s)); }

juce::Colour styleColour (const juce::String& style)
{
    if (style == "Trap") return juce::Colour (0xffffb27a);
    if (style == "R&B") return juce::Colour (0xffc9a0ff);
    if (style == "Rap") return juce::Colour (0xffaebeff);
    if (style == "Afro") return juce::Colour (0xff8fe3b0);
    if (style == "Drill") return juce::Colour (0xffb9c0cc);
    if (style == "Rage") return juce::Colour (0xffff8c8c);
    if (style == "FX") return juce::Colour (0xfff2b25c);
    return juce::Colour (0xff6fd3c1);
}

const char* shortName (chain::Type t)
{
    switch (t)
    {
        case chain::Type::match: return "MATCH";
        case chain::Type::vox: return "VOX";
        case chain::Type::lift: return "LIFT";
        case chain::Type::adlib: return "AD-LIB";
        case chain::Type::space: return "SPACE";
        case chain::Type::knock: return "KNOCK";
        case chain::Type::glue: return "GLUE";
        default: return "";
    }
}

void styleButton (juce::TextButton& b, bool on)
{
    b.setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff3a2f1e) : colours::field);
    b.setColour (juce::TextButton::textColourOffId, on ? colours::vfd : colours::dim);
}

// Ink printed on the anthracite panel.
const juce::Colour ink { 0xffece5d3 }, inkDim { 0xffa59e8c };

/** Letter-spaced capitals, as screen-printed on hardware. */
void printSpaced (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c,
                  juce::Justification j = juce::Justification::centredLeft, float spacing = 0.14f)
{
    g.setColour (c);
    g.setFont (f.withExtraKerningFactor (spacing));
    g.drawText (text, r, j, false);
}

/** The brushed aluminium shared by the façade and the module plates (computed once, deterministic). */
const juce::Image& brushedFace()
{
    static const juce::Image img = an::brushedMetal (1600, 1000, juce::Colour (0xff2b2c30), 0.10f, 2024);
    return img;
}
const juce::Image& brushedPlate()
{
    static const juce::Image img = an::brushedMetal (360, 440, juce::Colour (0xff35363b), 0.12f, 77);
    return img;
}

/** A recessed area: dark well with its inner shadow and a lit lower lip. */
void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour fill, float depth = 1.0f)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.75f * depth), r.getX(), r.getY(),
                                                 juce::Colours::transparentBlack, r.getX(), r.getY() + 16, false));
        g.fillRect (r.withHeight (16));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.4f * depth), r.getX(), r.getY(),
                                                 juce::Colours::transparentBlack, r.getX() + 10, r.getY(), false));
        g.fillRect (r.withWidth (10));
    }
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (r.getX() + corner, r.getBottom() + 1.0f, r.getRight() - corner, r.getBottom() + 1.0f, 1.0f);
}

/** A raised sub-panel (module section, output section). */
void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Path p;
    p.addRoundedRectangle (r, 6);
    an::softShadow (g, p, juce::Colours::black.withAlpha (0.55f), 8, { 0, 3 });
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (p);
        g.drawImage (brushedFace(), r, juce::RectanglePlacement::fillDestination);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x18ffffff), r.getX(), r.getY(), juce::Colour (0x30000000), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 6, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 6, r.getY() + 1.2f, r.getRight() - 6, r.getY() + 1.2f, 1.0f);
}

void drawSmallArrow (juce::Graphics& g, juce::Point<float> c, juce::Colour col)
{
    juce::Path a;
    a.startNewSubPath (c.x - 6, c.y); a.lineTo (c.x + 4, c.y);
    juce::Path head;
    head.addTriangle (c.x + 3, c.y - 4, c.x + 8, c.y, c.x + 3, c.y + 4);
    g.setColour (col);
    g.strokePath (a, juce::PathStrokeType (1.6f));
    g.fillPath (head);
}
}

//==============================================================================
juce::Font ui::font (float size, bool bold)
{
    return an::panelFont (size, bold);
}

juce::Font ui::vfdFont (float size)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Menlo", size, juce::Font::bold));
   #else
    return juce::Font (juce::FontOptions ("Consolas", size, juce::Font::bold));
   #endif
}

void ui::drawVfdWindow (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.expanded (1.5f), 5);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0806), r.getX(), r.getY(), juce::Colour (0xff17100a), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 8, false));
    g.fillRoundedRectangle (r.withHeight (8), 4);
    // The glass of the display: a faint diagonal reflection.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::transparentWhite,
                                             r.getX() + r.getWidth() * 0.3f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 4, r.getBottom() + 2.0f, r.getRight() - 4, r.getBottom() + 2.0f, 1.0f);
}

void ui::drawVfdText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size, juce::Justification j, float alpha)
{
    const auto f = vfdFont (size).withExtraKerningFactor (0.06f);
    g.setFont (f);
    // Glow: two soft passes behind the lit segments (cheap: plain offset text, no blur).
    g.setColour (colours::accent.withAlpha (0.16f * alpha));
    for (auto o : { juce::Point<float> (-0.8f, 0), juce::Point<float> (0.8f, 0), juce::Point<float> (0, -0.8f), juce::Point<float> (0, 0.8f) })
        g.drawText (text, r.translated (o.x, o.y), j, false);
    g.setColour (colours::vfd.withAlpha (alpha));
    g.drawText (text, r, j, false);
}

Look::Look()
{
    setColour (juce::TextButton::buttonColourId, colours::field);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a2f1e));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd9d1bd));
    setColour (juce::TextButton::textColourOnId, colours::vfd);
    setColour (juce::ComboBox::backgroundColourId, colours::field);
    setColour (juce::ComboBox::textColourId, colours::vfd);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff343a46));
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1917));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a2f1e));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::vfd);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4a4640));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xee151413));
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff5a5650));
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (over) fill = fill.brighter (0.08f);
    if (down) fill = fill.darker (0.12f);
    g.setGradientFill (juce::ColourGradient (fill.brighter (0.08f), r.getX(), r.getY(), fill.darker (0.15f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 4, 1);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 4, r.getY() + 1.2f, r.getRight() - 4, r.getY() + 1.2f, 1.0f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    drawVfdWindow (g, juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2));
}

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (6, 2));
    label.setFont (getComboBoxFont (box).withExtraKerningFactor (0.08f));
    label.setJustificationType (juce::Justification::centred);
}

juce::String ui::formatValue (const chain::KnobSpec& s, float v)
{
    const juce::String unit (s.unit);
    if (s.toggle) return v > 0.5f ? "ON" : "OFF";
    if (unit == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
    if (unit == "st") return (v > 0.05f ? "+" : "") + juce::String (juce::roundToInt (v)) + " dt";
    return juce::String (juce::roundToInt (v)) + " %";
}

//==============================================================================
void ArrowButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (9, 11);
    juce::Path p;
    if (pointsRight) p.addTriangle (r.getX(), r.getY(), r.getRight(), r.getCentreY(), r.getX(), r.getBottom());
    else p.addTriangle (r.getRight(), r.getY(), r.getX(), r.getCentreY(), r.getRight(), r.getBottom());
    const float a = down ? 0.7f : over ? 1.0f : 0.8f;
    g.setColour (colours::accent.withAlpha (0.25f * a));
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::vfd.withAlpha (a));
    g.fillPath (p);
}

void LetterKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    PushKey::paintButton (g, over, down);
    const bool pressed = getToggleState() || down;
    auto r = getLocalBounds().toFloat().translated (0, pressed ? 1.0f : -1.0f);
    g.setColour (juce::Colour (pressed ? 0xff3a2a10 : 0xff2a261d));
    g.setFont (font (15, true));
    g.drawText (letter, r, juce::Justification::centred, false);
}

void LedKey::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    // Seat in the plate, then the cap (it sinks when pressed).
    g.setColour (juce::Colour (0xff0a0a0b));
    g.fillRoundedRectangle (b, 4);
    auto cap = b.reduced (2).translated (0, down ? 1.0f : 0.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff4a4b51 : 0xff434449), cap.getX(), cap.getY(),
                                             juce::Colour (0xff232427), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawLine (cap.getX() + 3, cap.getY() + 0.8f, cap.getRight() - 3, cap.getY() + 0.8f, 0.8f);
    auto led = cap.withSizeKeepingCentre (cap.getWidth() * 0.52f, 5.0f);
    if (getToggleState())
    {
        g.setColour (colour.withAlpha (0.28f));
        g.fillRoundedRectangle (led.expanded (3), 4);
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.8f), led.getX(), led.getY(), colour.darker (0.2f), led.getX(), led.getBottom(), false));
    }
    else g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (led, 2);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (led, 2, 0.8f);
}

void RoundX::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = juce::jmin (getWidth(), getHeight()) * 0.5f - 2;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r + 0.8f, c.y - r + 1.6f, r * 2, r * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff6a6b70 : 0xff5a5b60), c.x - r, c.y - r, juce::Colour (0xff1e1e21), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r + (down ? 0.8f : 0.0f), r * 2, r * 2);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
    drawGlyph (g, Glyph::close, juce::Rectangle<float> (r * 1.0f, r * 1.0f).withCentre (c), juce::Colour (0xffd9d1bd), 2.6f);
}

void LitButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0, 2), 5);
    auto cap = b.translated (0, down ? 1.5f : 0.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff47331a : 0xff3d2c15), cap.getX(), cap.getY(),
                                             juce::Colour (0xff1e1509), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 5);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 5, 1);
    g.setColour (juce::Colour (0x30ffc878));
    g.drawLine (cap.getX() + 5, cap.getY() + 1.2f, cap.getRight() - 5, cap.getY() + 1.2f, 1.0f);
    // The bulb behind its jewel.
    const auto c = juce::Point<float> (cap.getX() + 20, cap.getCentreY());
    if (lit)
    {
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.55f), c.x, c.y, colours::accent.withAlpha (0.0f), c.x + 16, c.y, true));
        g.fillEllipse (c.x - 16, c.y - 16, 32, 32);
    }
    juce::ColourGradient bulb (lit ? juce::Colour (0xfffff0c8) : juce::Colour (0xff5a3a12), c.x - 2, c.y - 2,
                               lit ? juce::Colour (0xff8a4b06) : juce::Colour (0xff1a1006), c.x + 7, c.y + 7, true);
    if (lit) bulb.addColour (0.5, colours::accent);
    g.setGradientFill (bulb);
    g.fillEllipse (c.x - 7, c.y - 7, 14, 14);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - 7, c.y - 7, 14, 14, 1.0f);
    g.setColour (juce::Colour (lit ? 0xffffd79a : 0xffa58a60));
    g.setFont (font (12.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (upper (getButtonText()), cap.withTrimmedLeft (38).withTrimmedRight (8), juce::Justification::centredLeft, false);
}

//==============================================================================

namespace
{
/** Maps a point of the 24 x 24 icon grid into r. */
struct Grid
{
    juce::Rectangle<float> r;
    juce::Point<float> operator() (float x, float y) const { return { r.getX() + x * r.getWidth() / 24, r.getY() + y * r.getHeight() / 24 }; }
    float s (float v) const { return v * r.getWidth() / 24; }
};

void stroke (juce::Graphics& g, const juce::Path& p, float width)
{
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

void ui::drawModuleIcon (juce::Graphics& g, chain::Type t, juce::Rectangle<float> r, juce::Colour c, float w)
{
    const Grid P { r };
    w = P.s (w);
    juce::Path p;
    g.setColour (c);
    switch (t)
    {
        case chain::Type::match:   // an EQ curve with two points
            p.startNewSubPath (P (2, 15));
            p.cubicTo (P (6, 15), P (7, 7), P (10, 7));
            p.cubicTo (P (13, 7), P (13, 17), P (16, 17));
            p.cubicTo (P (19, 17), P (20, 11), P (22, 11));
            stroke (g, p, w);
            g.fillEllipse (juce::Rectangle<float> (P.s (4.4f), P.s (4.4f)).withCentre (P (10, 7)));
            g.fillEllipse (juce::Rectangle<float> (P.s (4.4f), P.s (4.4f)).withCentre (P (16, 17)));
            break;
        case chain::Type::vox:     // a microphone
            p.addRoundedRectangle (juce::Rectangle<float> (P (9, 2.5f), P (15, 14)), P.s (3));
            p.startNewSubPath (P (5.5f, 11));
            p.cubicTo (P (5.5f, 19.5f), P (18.5f, 19.5f), P (18.5f, 11));
            p.startNewSubPath (P (12, 17.5f)); p.lineTo (P (12, 21.5f));
            p.startNewSubPath (P (8.5f, 21.5f)); p.lineTo (P (15.5f, 21.5f));
            stroke (g, p, w);
            break;
        case chain::Type::lift:    // an arrow going up, over a line
            p.startNewSubPath (P (12, 18)); p.lineTo (P (12, 4));
            p.startNewSubPath (P (6.5f, 9.5f)); p.lineTo (P (12, 4)); p.lineTo (P (17.5f, 9.5f));
            p.startNewSubPath (P (5, 21)); p.lineTo (P (19, 21));
            stroke (g, p, w);
            break;
        case chain::Type::adlib:   // a speech bubble
            p.startNewSubPath (P (7, 4));
            p.lineTo (P (17, 4)); p.quadraticTo (P (21, 4), P (21, 8));
            p.lineTo (P (21, 12)); p.quadraticTo (P (21, 16), P (17, 16));
            p.lineTo (P (11, 16)); p.lineTo (P (6.5f, 20.5f)); p.lineTo (P (7, 16));
            p.quadraticTo (P (3, 16), P (3, 12)); p.lineTo (P (3, 8)); p.quadraticTo (P (3, 4), P (7, 4));
            p.closeSubPath();
            stroke (g, p, w);
            for (float x : { 8.0f, 12.0f, 16.0f }) g.fillEllipse (juce::Rectangle<float> (P.s (2.6f), P.s (2.6f)).withCentre (P (x, 10)));
            break;
        case chain::Type::space:   // a source and its echoes
        {
            g.fillEllipse (juce::Rectangle<float> (P.s (5), P.s (5)).withCentre (P (5, 12)));
            const auto centre = P (5, 12);
            float alpha = 1;
            for (float radius : { 6.5f, 11.5f, 16.5f })
            {
                juce::Path a;
                a.addCentredArc (centre.x, centre.y, P.s (radius), P.s (radius), 0,
                                 juce::MathConstants<float>::halfPi - 0.75f, juce::MathConstants<float>::halfPi + 0.75f, true);
                g.setColour (c.withAlpha (alpha));
                stroke (g, a, w);
                alpha -= 0.3f;
            }
            break;
        }
        case chain::Type::knock:   // a lightning bolt
            p.startNewSubPath (P (13.5f, 2));
            p.lineTo (P (5, 13.5f)); p.lineTo (P (11, 13.5f)); p.lineTo (P (10, 22));
            p.lineTo (P (19, 10)); p.lineTo (P (13, 10)); p.closeSubPath();
            stroke (g, p, w);
            break;
        case chain::Type::glue:    // two arrows squeezing a signal
            p.startNewSubPath (P (2, 12)); p.lineTo (P (8.5f, 12));
            p.startNewSubPath (P (5.5f, 8.5f)); p.lineTo (P (8.5f, 12)); p.lineTo (P (5.5f, 15.5f));
            p.startNewSubPath (P (22, 12)); p.lineTo (P (15.5f, 12));
            p.startNewSubPath (P (18.5f, 8.5f)); p.lineTo (P (15.5f, 12)); p.lineTo (P (18.5f, 15.5f));
            p.startNewSubPath (P (11, 6)); p.lineTo (P (11, 18));
            p.startNewSubPath (P (13, 6)); p.lineTo (P (13, 18));
            stroke (g, p, w);
            break;
        default:
            break;
    }
}

void ui::drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> r, juce::Colour c, float w)
{
    const Grid P { r };
    w = P.s (w);
    juce::Path p;
    g.setColour (c);
    switch (glyph)
    {
        case Glyph::power:
        {
            const auto centre = P (12, 13);
            p.addCentredArc (centre.x, centre.y, P.s (7.5f), P.s (7.5f), 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
            p.startNewSubPath (P (12, 3)); p.lineTo (P (12, 11));
            break;
        }
        case Glyph::solo:   // headphones
        {
            const auto centre = P (12, 14);
            p.addCentredArc (centre.x, centre.y, P.s (8), P.s (8), 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            p.addRoundedRectangle (juce::Rectangle<float> (P (3.5f, 13.5f), P (7.5f, 21)), P.s (1.5f));
            p.addRoundedRectangle (juce::Rectangle<float> (P (16.5f, 13.5f), P (20.5f, 21)), P.s (1.5f));
            break;
        }
        case Glyph::close:
            p.startNewSubPath (P (6, 6)); p.lineTo (P (18, 18));
            p.startNewSubPath (P (18, 6)); p.lineTo (P (6, 18));
            break;
        case Glyph::analyse:   // a voice waveform
            p.startNewSubPath (P (2, 12)); p.lineTo (P (6, 12)); p.lineTo (P (9, 5)); p.lineTo (P (13, 19));
            p.lineTo (P (16, 12)); p.lineTo (P (22, 12));
            break;
    }
    stroke (g, p, w);
}

//==============================================================================
SlotCard::SlotCard (ChainEditor& e, ChainProcessor& p, int i) : index (i), editor (e), processor (p)
{
    onButton.setClickingTogglesState (true);
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::on (i), onButton);
    soloButton.onClick = [this] { processor.setSolo (processor.soloSlot() == index ? -1 : index); };
    removeButton.onClick = [this] { processor.removeSlot (index); editor.select (index); };
    for (auto* b : std::initializer_list<juce::Button*> { &onButton, &soloButton, &removeButton }) addAndMakeVisible (b);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refresh();
}

void SlotCard::refresh()
{
    const auto s = processor.slot (index);
    const bool filled = s.type != chain::Type::empty;
    const bool solo = processor.soloSlot() == index;
    const bool selected = editor.selected() == index;
    const bool drop = editor.dropTarget() == index && editor.dragSource() != index;
    const bool active = s.on && (processor.soloSlot() < 0 || solo);
    const juce::int64 sig = (juce::int64) s.type | ((juce::int64) s.preset << 8) | ((juce::int64) s.on << 16) | ((juce::int64) solo << 17)
                          | ((juce::int64) selected << 18) | ((juce::int64) drop << 19) | ((juce::int64) active << 20);
    if (sig == shown) return;   // nothing changed on this plate: no repaint at all
    shown = sig;
    for (auto* b : std::initializer_list<juce::Button*> { &onButton, &soloButton, &removeButton }) b->setVisible (filled);
    soloButton.setToggleState (solo, juce::dontSendNotification);
    setTooltip (filled ? fr (chain::info (s.type).name) + fr (" : glisser pour déplacer") : fr ("Ajouter un module"));
    setMouseCursor (filled ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
    repaint();
}

void SlotCard::resized()
{
    onButton.setBounds (14, getHeight() - 44, 38, 28);
    removeButton.setBounds (getWidth() / 2 - 14, getHeight() - 43, 28, 28);
    soloButton.setBounds (getWidth() - 52, getHeight() - 44, 38, 28);
}

void SlotCard::paint (juce::Graphics& g)
{
    const auto s = processor.slot (index);
    const auto bounds = getLocalBounds();
    const auto r = bounds.toFloat();
    const bool selected = editor.selected() == index;
    const bool drop = editor.dropTarget() == index && editor.dragSource() != index;
    const auto screws = [&] (juce::Graphics& ig)
    {
        for (auto p : { juce::Point<float> (10, 10), juce::Point<float> (r.getRight() - 10, 10),
                        juce::Point<float> (10, r.getBottom() - 10), juce::Point<float> (r.getRight() - 10, r.getBottom() - 10) })
            an::drawScrew (ig, p, 4.2f, 0.6f + p.x * 0.013f + p.y * 0.007f, true);
    };

    if (s.type == chain::Type::empty)
    {
        // Blank plate with ventilation slots (rendered once).
        blank.get (g, bounds, [&] (juce::Graphics& ig)
        {
            ig.drawImage (brushedPlate(), r, juce::RectanglePlacement::fillDestination);
            ig.setColour (juce::Colours::black.withAlpha (0.38f));
            ig.fillRoundedRectangle (r, 4);
            an::drawBevelEdge (ig, r, 4, 0.8f);
            ig.setColour (juce::Colours::black);
            ig.drawRoundedRectangle (r.reduced (0.5f), 4, 1);
            for (int i = 0; i < 8; ++i)
            {
                auto slot = juce::Rectangle<float> (30, 46 + (float) i * 11, r.getWidth() - 60, 4.5f);
                ig.setColour (juce::Colour (0xff070708));
                ig.fillRoundedRectangle (slot, 2);
                ig.setColour (juce::Colours::white.withAlpha (0.07f));
                ig.drawLine (slot.getX() + 2, slot.getBottom() + 0.8f, slot.getRight() - 2, slot.getBottom() + 0.8f, 0.8f);
            }
            screws (ig);
            ig.setColour (inkDim.withAlpha (0.55f));
            ig.setFont (font (24, false));
            ig.drawText ("+", juce::Rectangle<float> (0, 140, r.getWidth(), 26), juce::Justification::centred, false);
            printSpaced (ig, "MODULE", juce::Rectangle<float> (0, 168, r.getWidth(), 14), font (9.5f, true), inkDim, juce::Justification::centred, 0.2f);
        });
        blank.blit (g, r);
        if (drop || selected)
        {
            g.setColour (colours::accent.withAlpha (selected ? 0.55f : 0.9f));
            g.drawRoundedRectangle (r.reduced (1.5f), 4, 2.0f);
        }
        return;
    }

    // Module plate body (rendered once per plate): brushed metal, bevel, screws, grip, porthole ring, printed labels.
    plate.get (g, bounds, [&] (juce::Graphics& ig)
    {
        ig.drawImage (brushedPlate(), r, juce::RectanglePlacement::fillDestination);
        an::drawPanelLight (ig, r, 0.8f);
        an::drawBevelEdge (ig, r, 4, 1.0f);
        ig.setColour (juce::Colours::black);
        ig.drawRoundedRectangle (r.reduced (0.5f), 4, 1);
        screws (ig);
        // Grip: knurled bar used to pull the plate out of the rack.
        auto grip = juce::Rectangle<float> (r.getCentreX() - 20, 8, 40, 8);
        ig.setColour (juce::Colour (0xff0e0e10));
        ig.fillRoundedRectangle (grip, 1.5f);
        for (float x = grip.getX() + 2; x < grip.getRight() - 1; x += 3)
        {
            ig.setColour (juce::Colours::white.withAlpha (0.16f));
            ig.drawLine (x, grip.getY() + 1.5f, x, grip.getBottom() - 1.5f, 1.0f);
        }
        // Porthole: machined ring around a dark glass.
        const juce::Point<float> c (r.getCentreX(), 118);
        juce::Path ring;
        ring.addEllipse (c.x - 36, c.y - 36, 72, 72);
        an::softShadow (ig, ring, juce::Colours::black.withAlpha (0.6f), 5, { 0, 2 });
        ig.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a6b70), c.x - 30, c.y - 34, juce::Colour (0xff1d1d20), c.x + 30, c.y + 34, false));
        ig.fillPath (ring);
        ig.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c1f), c.x, c.y - 30, juce::Colour (0xff060607), c.x, c.y + 30, false));
        ig.fillEllipse (c.x - 31, c.y - 31, 62, 62);
        ig.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.85f), c.x, c.y - 31, juce::Colours::transparentBlack, c.x, c.y - 18, false));
        ig.fillEllipse (c.x - 31, c.y - 31, 62, 62);
        printSpaced (ig, "ON", juce::Rectangle<float> (14, r.getBottom() - 58, 38, 12), font (8.5f, true), inkDim, juce::Justification::centred, 0.18f);
        printSpaced (ig, "SOLO", juce::Rectangle<float> (r.getRight() - 52, r.getBottom() - 58, 38, 12), font (8.5f, true), inkDim, juce::Justification::centred, 0.18f);
    });
    plate.blit (g, r);

    // What changes with the module: colour strip, name, preset, icon, light.
    const auto& info = chain::info (s.type);
    const auto colour = moduleColour (s.type);
    const bool active = s.on && (processor.soloSlot() < 0 || processor.soloSlot() == index);
    auto strip = juce::Rectangle<float> (12, 26, r.getWidth() - 24, 4);
    if (active)
    {
        g.setColour (colour.withAlpha (0.22f));
        g.fillRoundedRectangle (strip.expanded (2, 3), 4);
    }
    g.setColour (colour.withAlpha (active ? 1.0f : 0.28f));
    g.fillRoundedRectangle (strip, 2);
    printSpaced (g, shortName (s.type), juce::Rectangle<float> (0, 38, r.getWidth(), 20), font (15, true),
                 active ? ink : ink.withAlpha (0.45f), juce::Justification::centred, 0.16f);
    const auto sub = info.presets.empty() ? upper (fr (info.role))
                                          : upper (fr (info.presets[(size_t) juce::jlimit (0, (int) info.presets.size() - 1, s.preset)].c_str()));
    g.setColour (info.presets.empty() ? inkDim.withAlpha (active ? 1.0f : 0.5f) : colours::vfd.withAlpha (active ? 0.9f : 0.4f));
    g.setFont (font (9, true).withExtraKerningFactor (0.1f));
    g.drawFittedText (sub, juce::Rectangle<int> (8, 60, getWidth() - 16, 14), juce::Justification::centred, 1, 0.8f);

    const juce::Point<float> c (r.getCentreX(), 118);
    if (active)
    {
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.20f), c.x, c.y, colour.withAlpha (0.0f), c.x + 30, c.y, true));
        g.fillEllipse (c.x - 30, c.y - 30, 60, 60);
    }
    drawModuleIcon (g, s.type, juce::Rectangle<float> (34, 34).withCentre (c), active ? colour : colour.withAlpha (0.3f), 1.8f);
    // Glass reflection over the porthole.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), c.x - 20, c.y - 28, juce::Colours::transparentWhite, c.x + 6, c.y, false));
    g.fillEllipse (c.x - 31, c.y - 31, 62, 62);

    if (selected || drop)
    {
        const auto edge = drop ? colours::accent : colour;
        g.setColour (edge.withAlpha (0.18f));
        g.drawRoundedRectangle (r.reduced (3.5f), 4, 5.0f);
        g.setColour (edge);
        g.drawRoundedRectangle (r.reduced (1.0f), 4, 2.0f);
    }
}

void SlotCard::mouseDown (const juce::MouseEvent&) { dragging = false; }

void SlotCard::mouseDrag (const juce::MouseEvent& e)
{
    if (processor.slot (index).type == chain::Type::empty) return;
    if (e.getDistanceFromDragStart() > 8) dragging = true;
    if (dragging) editor.dragOver (index, e.getScreenPosition(), false);
}

void SlotCard::mouseUp (const juce::MouseEvent& e)
{
    if (dragging) { editor.dragOver (index, e.getScreenPosition(), true); dragging = false; return; }
    if (processor.slot (index).type == chain::Type::empty) editor.openModules (index);
    editor.select (index);
}

//==============================================================================
KnobView::KnobView (ChainProcessor& p, int s, int k, const chain::KnobSpec& sp, juce::Colour c, int knobSize)
    : processor (p), spec (sp), slot (s), knob (k), size (knobSize), colour (c),
      slider (knobSize >= 96 ? an::Knob::Style::FetLarge : an::Knob::Style::FetSmall)
{
    if (spec.toggle)
    {
        toggle.setTooltip (fr (spec.label));
        toggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::knob (s, k), toggle);
        toggle.onStateChange = [this] { repaint (valueBox().toNearestInt().expanded (4)); };
        addAndMakeVisible (toggle);
        return;
    }
    slider.setPopupDisplayEnabled (false, false, nullptr);   // the value window under the knob shows it
    slider.setTitle (fr (spec.label));
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::knob (s, k), slider);
    slider.onValueChange = [this] { repaint (valueBox().toNearestInt().expanded (4)); };
    slider.setDoubleClickReturnValue (true, (spec.def - spec.min) / (spec.max - spec.min));
}

namespace
{
int knobTop (int height, int size) { return juce::jmax (18, (height - (size + 96)) / 2 + 16); }
}

juce::Rectangle<float> KnobView::valueBox() const
{
    const float y = (float) (knobTop (getHeight(), size) + size + 38);
    return juce::Rectangle<float> (78, 22).withCentre ({ getWidth() / 2.0f, y + 11 });
}

void KnobView::resized()
{
    const auto knobArea = juce::Rectangle<int> (size, size).withCentre ({ getWidth() / 2, knobTop (getHeight(), size) + size / 2 });
    if (spec.toggle) toggle.setBounds (knobArea.withSizeKeepingCentre (64, 64));
    else slider.setBounds (knobArea);
    scale.invalidate();
}

void KnobView::paint (juce::Graphics& g)
{
    const int top = knobTop (getHeight(), size);
    const auto centre = juce::Point<float> (getWidth() / 2.0f, (float) top + size / 2.0f);
    // Printed scale, label and the empty value window: rendered once.
    scale.get (g, getLocalBounds(), [&] (juce::Graphics& ig)
    {
        if (spec.toggle)
        {
            printSpaced (ig, "ON", juce::Rectangle<float> (0, (float) top - 6, (float) getWidth(), 12), font (9, true), inkDim, juce::Justification::centred, 0.18f);
            printSpaced (ig, "OFF", juce::Rectangle<float> (0, (float) top + size - 6, (float) getWidth(), 12), font (9, true), inkDim, juce::Justification::centred, 0.18f);
        }
        else
        {
            an::ScaleSpec sc;
            sc.centre = centre;
            sc.radius = size * 0.42f + 4;
            sc.tickLen = 6;
            sc.labelRadius = sc.radius + 17;
            sc.start = -2.36f; sc.end = 2.36f;
            sc.ink = ink.withAlpha (0.8f);
            sc.fontSize = 9;
            const bool labelled = getWidth() >= size + 44;
            auto num = [this] (float v) { const bool signedUnit = juce::String (spec.unit) == "dB" || juce::String (spec.unit) == "st";
                                          return (signedUnit && v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)); };
            if (labelled) sc.labels = { num (spec.min), "", num ((spec.min + spec.max) / 2), "", num (spec.max) };
            else sc.labels = { "", "", "", "", "" };
            sc.minorPerMajor = 2;
            an::drawKnobScale (ig, sc);
        }
        const float ly = (float) top + size + 14;
        ig.setColour (colour);
        const float tw = juce::GlyphArrangement::getStringWidth (font (10.5f, true).withExtraKerningFactor (0.16f), upper (fr (spec.label)));
        ig.fillEllipse (getWidth() / 2.0f - tw / 2 - 10, ly + 4.5f, 5, 5);
        printSpaced (ig, upper (fr (spec.label)), juce::Rectangle<float> (0, ly, (float) getWidth(), 14), font (10.5f, true), ink, juce::Justification::centred, 0.16f);
        drawVfdWindow (ig, valueBox());
    });
    scale.blit (g, getLocalBounds().toFloat());

    const float real = spec.toggle ? (toggle.getToggleState() ? 1.0f : 0.0f) : spec.min + (float) slider.getValue() * (spec.max - spec.min);
    drawVfdText (g, formatValue (spec, real), valueBox(), 13, juce::Justification::centred);
}

//==============================================================================
void SpectrumView::update (ChainProcessor& p)
{
    rate = p.currentRate();
    for (int w = 0; w < 2; ++w)
    {
        const int got = p.readScope (w == 1, incoming.data(), (int) incoming.size());
        auto& h = history[(size_t) w];
        if (got >= size) std::copy (incoming.begin() + (got - size), incoming.begin() + got, h.begin());
        else if (got > 0)
        {
            std::move (h.begin() + got, h.end(), h.begin());
            std::copy (incoming.begin(), incoming.begin() + got, h.end() - got);
        }
        std::fill (work.begin(), work.end(), 0.0f);
        std::copy (h.begin(), h.end(), work.begin());
        window.multiplyWithWindowingTable (work.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (work.data());
        auto& l = levels[(size_t) w];
        for (int b = 0; b < bins; ++b)
        {
            const float db = juce::Decibels::gainToDecibels (work[(size_t) b] / (float) size * 4.0f, -100.0f);
            l[(size_t) b] = db > l[(size_t) b] ? 0.5f * l[(size_t) b] + 0.5f * db : 0.85f * l[(size_t) b] + 0.15f * db;
        }
    }
    repaint();
}


void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    auto r = bounds.reduced (12);
    const float lowHz = 30, highHz = 16000, top = -12, bottom = -96;
    auto xOf = [&] (float hz) { return r.getX() + r.getWidth() * std::log (hz / lowHz) / std::log (highHz / lowHz); };
    auto yOf = [&] (float db) { return r.getY() + r.getHeight() * (top - juce::jlimit (bottom, top, db)) / (top - bottom); };

    // Bezel, phosphor background and graticule: rendered once.
    screen.get (g, getLocalBounds(), [&] (juce::Graphics& ig)
    {
        ig.setColour (juce::Colours::black);
        ig.fillRoundedRectangle (bounds, 14);
        ig.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), bounds.getX(), bounds.getY(), juce::Colour (0xff111113), bounds.getX(), bounds.getBottom(), false));
        ig.fillRoundedRectangle (bounds.reduced (2), 13);
        ig.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a140a), r.getCentreX(), r.getCentreY(), juce::Colour (0xff060504), r.getX(), r.getY(), true));
        ig.fillRoundedRectangle (r, 10);
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            ig.setColour (colours::accent.withAlpha (0.08f));
            ig.drawVerticalLine ((int) xOf (hz), r.getY() + 4, r.getBottom() - 4);
            if (hz == 100 || hz == 1000 || hz == 10000 || hz == 500 || hz == 5000)
            {
                ig.setColour (colours::vfd.withAlpha (0.45f));
                ig.setFont (vfdFont (10));
                ig.drawText (hz >= 1000 ? juce::String ((int) hz / 1000) + "k" : juce::String ((int) hz), (int) xOf (hz) + 4, (int) r.getBottom() - 18, 40, 14, juce::Justification::left);
            }
        }
        for (int i = 1; i < 6; ++i)
        {
            ig.setColour (colours::accent.withAlpha (0.06f));
            ig.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 6.0f), r.getX() + 4, r.getRight() - 4);
        }
        ig.setFont (vfdFont (11));
        ig.setColour (juce::Colour (0xff9a958a));
        ig.drawText ("AVANT", (int) r.getRight() - 150, (int) r.getY() + 8, 60, 16, juce::Justification::right);
        ig.setColour (colours::vfd);
        ig.drawText (fr ("APRÈS"), (int) r.getRight() - 80, (int) r.getY() + 8, 66, 16, juce::Justification::right);
    });
    screen.blit (g, bounds);

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.toNearestInt());
    for (int w = 0; w < 2; ++w)
    {
        juce::Path path;
        bool started = false;
        for (int b = 1; b < bins; ++b)
        {
            const float hz = (float) b * (float) rate / (float) size;
            if (hz < lowHz || hz > highHz) continue;
            const auto pt = juce::Point<float> (xOf (hz), yOf (levels[(size_t) w][(size_t) b]));
            if (! started) { path.startNewSubPath (pt); started = true; } else path.lineTo (pt);
        }
        if (! started) continue;
        if (w == 1)
        {
            juce::Path fill (path);
            fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
            g.setColour (colours::accent.withAlpha (0.08f));
            g.fillPath (fill);
            g.setColour (colours::accent.withAlpha (0.22f));   // phosphor glow: a wider faint trace under the sharp one
            g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (w == 0 ? juce::Colour (0xff8d877a) : colours::vfd);
        g.strokePath (path, juce::PathStrokeType (w == 0 ? 1.3f : 2.0f));
    }
    // Curved glass over the tube: rendered once.
    glass.get (g, getLocalBounds(), [&] (juce::Graphics& ig)
    {
        ig.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(), juce::Colours::transparentWhite,
                                                  r.getX() + r.getWidth() * 0.35f, r.getBottom(), false));
        ig.fillRoundedRectangle (r, 10);
        juce::Path vignette;
        vignette.addRoundedRectangle (r, 10);
        vignette.setUsingNonZeroWinding (false);
        vignette.addRoundedRectangle (r.reduced (r.getHeight() * 0.06f), 24);
        ig.setColour (juce::Colours::black.withAlpha (0.35f));
        ig.fillPath (vignette);
    });
    glass.blit (g, bounds);
}

PresetsPage::PresetsPage (ChainEditor& e, ChainProcessor& p) : editor (e), processor (p)
{
    close.onClick = [this] { editor.closePages(); };
    nameField.setTextToShowWhenEmpty (fr ("Nom de ta chaîne"), colours::faint);
    nameField.setColour (juce::TextEditor::backgroundColourId, colours::field);
    nameField.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff343a46));
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    nameField.setColour (juce::TextEditor::textColourId, colours::text);
    nameField.setFont (font (14));
    nameField.setIndents (10, 8);
    nameField.onReturnKey = [this] { saveCurrent(); };
    saveButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    saveButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1209));
    saveButton.onClick = [this] { saveCurrent(); };
    for (auto* c : std::initializer_list<juce::Component*> { &close, &nameField, &saveButton }) addAndMakeVisible (c);
    for (auto name : { "Tous", "Favoris", "Trap", "R&B", "Rap", "Afro", "Drill", "Rage", "FX", "Topline", "Mes chaînes" })
    {
        auto* b = styleButtons.add (new juce::TextButton (fr (name)));
        b->onClick = [this, b] { setStyle (b->getButtonText()); };
        addAndMakeVisible (b);
    }
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    setStyle ("Tous");
}

void PresetsPage::visibilityChanged()
{
    if (! isVisible()) return;
    library.load();
    setStyle (style);
}

std::vector<PresetsPage::Row> PresetsPage::visible() const
{
    std::vector<Row> v;
    if (style == fr ("Mes chaînes"))
    {
        for (int i = 0; i < library.chainCount(); ++i) v.push_back ({ true, i });
        return v;
    }
    const auto& all = chain::chainPresets();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto name = fr (all[(size_t) i].name);
        if (style == "Tous" || (style == "Favoris" && library.isFavourite (name)) || fr (all[(size_t) i].style) == style)
            v.push_back ({ false, i });
    }
    return v;
}

void PresetsPage::saveCurrent()
{
    auto name = nameField.getText().trim();
    if (name.isEmpty()) { nameField.grabKeyboardFocus(); return; }
    library.saveChain (name, processor.captureChain());
    nameField.clear();
    // Load it back so the header shows the saved chain.
    for (int i = 0; i < library.chainCount(); ++i)
        if (library.chain (i).getProperty ("name").toString() == name) processor.loadUserChain (library.chain (i));
    setStyle (fr ("Mes chaînes"));
}

void PresetsPage::setStyle (const juce::String& s)
{
    style = s;
    for (auto* b : styleButtons) styleButton (*b, b->getButtonText() == style);
    list.setSize (viewport.getWidth() > 0 ? viewport.getWidth() - 10 : 1060, std::max (1, (int) visible().size()) * 40 + 4);
    list.repaint();
    repaint();
}

void PresetsPage::resized()
{
    close.setBounds (getWidth() - 62, 14, 34, 34);
    saveButton.setBounds (getWidth() - 270, 14, 196, 34);
    nameField.setBounds (getWidth() - 540, 14, 260, 34);
    int y = 70;
    for (auto* b : styleButtons) { b->setBounds (12, y, 156, 34); y += 38; }
    viewport.setBounds (181, 96, getWidth() - 181, getHeight() - 96);
    setStyle (style);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.drawImage (brushedFace(), getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    g.setColour (colours::text);
    g.setFont (font (18, true));
    g.drawText (fr ("Presets de chaînes"), 28, 14, 300, 34, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (font (12));
    g.drawText (juce::String ((int) chain::chainPresets().size()) + fr (" chaînes"), 220, 14, 200, 34, juce::Justification::centredLeft);
    g.setColour (colours::line);
    g.drawHorizontalLine (60, 0, (float) getWidth());
    g.drawVerticalLine (180, 60, (float) getHeight());
    g.setColour (juce::Colour (0xff7d8492));
    g.setFont (font (11, true));
    g.drawText ("NOM", 230, 64, 200, 28, juce::Justification::centredLeft);
    g.drawText (fr ("CHAÎNE"), 440, 64, 200, 28, juce::Justification::centredLeft);
    g.drawText ("STYLE", getWidth() - 150, 64, 100, 28, juce::Justification::centredLeft);
    g.setColour (colours::line);
    g.drawHorizontalLine (95, 181, (float) getWidth());
    if (visible().empty())
    {
        g.setColour (colours::dim);
        g.setFont (font (14));
        g.drawText (style == "Favoris" ? fr ("Clique sur l’étoile d’un preset pour le garder ici.")
                                       : fr ("Donne un nom à ta chaîne en haut, puis « Enregistrer ma chaîne »."),
                    181, 120, getWidth() - 181, 40, juce::Justification::centred);
    }
}

namespace
{
void drawStar (juce::Graphics& g, juce::Rectangle<float> r, bool filled)
{
    juce::Path star;
    star.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0);
    g.setColour (filled ? juce::Colour (0xfff2b25c) : juce::Colour (0xff4a5161));
    if (filled) g.fillPath (star);
    else g.strokePath (star, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
}

void PresetsPage::List::paint (juce::Graphics& g)
{
    const auto rows = page.visible();
    const auto& all = chain::chainPresets();
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& rowInfo = rows[(size_t) i];
        const auto row = juce::Rectangle<int> (0, i * 40, getWidth(), 40);
        juce::String name, styleName;
        std::vector<chain::Type> types;
        bool current;
        if (rowInfo.user)
        {
            const auto t = page.library.chain (rowInfo.index);
            name = t.getProperty ("name").toString();
            styleName = "Perso";
            for (auto child : t) if ((int) child.getProperty ("type", 0) > 0) types.push_back ((chain::Type) (int) child.getProperty ("type"));
            current = page.processor.userChainName() == name;
        }
        else
        {
            const auto& p = all[(size_t) rowInfo.index];
            name = fr (p.name);
            styleName = fr (p.style);
            for (const auto& step : p.steps) types.push_back (step.type);
            current = page.processor.userChainName().isEmpty() && rowInfo.index == page.processor.currentChainIndex();
        }
        if (current) { g.setColour (juce::Colour (0xff2e2618)); g.fillRect (row); g.setColour (colours::accent); g.fillRect (row.withWidth (3)); }
        else if (i == hover) { g.setColour (juce::Colour (0x22ffffff)); g.fillRect (row); }
        g.setColour (juce::Colour (0xff1d2027));
        g.drawHorizontalLine (row.getBottom() - 1, 0, (float) getWidth());
        if (! rowInfo.user) drawStar (g, juce::Rectangle<float> (18, (float) row.getY() + 12, 16, 16), page.library.isFavourite (name));
        g.setColour (colours::text);
        g.setFont (font (14, current));
        g.drawText (name, 49, row.getY(), 200, 40, juce::Justification::centredLeft, true);
        int x = 259;
        for (auto type : types)
        {
            const auto c = moduleColour (type);
            const juce::String label (shortName (type));
            const int w = (int) juce::GlyphArrangement::getStringWidth (font (11, true), label) + 16;
            g.setColour (c.withAlpha (0.14f));
            g.fillRoundedRectangle ((float) x, (float) row.getY() + 9, (float) w, 22, 4);
            g.setColour (c);
            g.setFont (font (11, true));
            g.drawText (label, x, row.getY() + 9, w, 22, juce::Justification::centred);
            x += w + 4;
        }
        g.setColour (colours::dim);
        g.setFont (font (12));
        g.drawText (styleName, getWidth() - 140, row.getY(), 80, 40, juce::Justification::centredLeft);
        if (rowInfo.user) drawGlyph (g, Glyph::close, juce::Rectangle<float> ((float) getWidth() - 44, (float) row.getY() + 12, 16, 16), colours::dim, 2.0f);
    }
}

void PresetsPage::List::mouseMove (const juce::MouseEvent& e) { hover = e.y / 40; repaint(); }

void PresetsPage::List::mouseUp (const juce::MouseEvent& e)
{
    const auto rows = page.visible();
    const int i = e.y / 40;
    if (i < 0 || i >= (int) rows.size()) return;
    const auto row = rows[(size_t) i];
    if (row.user)
    {
        if (e.x > getWidth() - 56) { page.library.removeChain (row.index); page.setStyle (page.style); return; }
        page.processor.loadUserChain (page.library.chain (row.index));
    }
    else
    {
        if (e.x < 44) { page.library.toggleFavourite (fr (chain::chainPresets()[(size_t) row.index].name)); page.setStyle (page.style); return; }
        page.processor.loadChain (row.index);
    }
    page.editor.closePages();
}

//==============================================================================
ModulesPage::ModulesPage (ChainEditor& e, ChainProcessor& p) : editor (e), processor (p)
{
    close.onClick = [this] { editor.closePages(); };
    addAndMakeVisible (close);
}

void ModulesPage::resized() { close.setBounds (getWidth() - 62, 14, 34, 34); }

juce::Rectangle<int> ModulesPage::tile (int i) const
{
    const int columns = 4, w = (getWidth() - 96 - 3 * 20) / columns, h = 260;
    return { 48 + (i % columns) * (w + 20), 90 + (i / columns) * (h + 20), w, h };
}

void ModulesPage::paint (juce::Graphics& g)
{
    g.drawImage (brushedFace(), getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    g.setColour (colours::text);
    g.setFont (font (18, true));
    g.drawText ("Ajouter un module", 28, 14, 300, 34, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (font (12));
    g.drawText ("Emplacement " + juce::String (target + 1), 230, 14, 200, 34, juce::Justification::centredLeft);
    g.setColour (colours::line);
    g.drawHorizontalLine (60, 0, (float) getWidth());
    for (int t = 1; t < chain::typeCount; ++t)
    {
        const auto type = (chain::Type) t;
        const auto r = tile (t - 1).toFloat();
        bool inChain = false;
        for (int s = 0; s < chain::slotCount; ++s) inChain = inChain || processor.slot (s).type == type;
        const auto c = moduleColour (type);
        g.setColour (hover == t - 1 ? juce::Colour (0x26ffffff) : colours::panel);
        g.fillRoundedRectangle (r, 14);
        g.setColour (hover == t - 1 ? c : colours::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 14, 1);
        auto top = r.withHeight (180).reduced (1);
        g.setColour (c.withAlpha (0.12f));
        g.fillRoundedRectangle (top, 13);
        drawModuleIcon (g, type, top.withSizeKeepingCentre (84, 84), c, 1.8f);
        g.setColour (colours::text);
        g.setFont (font (17, true));
        g.drawText (fr (chain::info (type).name), (int) r.getX() + 18, (int) r.getY() + 196, (int) r.getWidth() - 70, 40, juce::Justification::centredLeft);
        auto badge = juce::Rectangle<float> (r.getRight() - 48, r.getY() + 201, 30, 30);
        g.setColour (inChain ? juce::Colour (0xff23272f) : colours::accent);
        g.fillEllipse (badge);
        g.setColour (inChain ? colours::good : juce::Colour (0xff1a1209));
        g.setFont (font (15, true));
        g.drawText (inChain ? fr ("✓") : "+", badge, juce::Justification::centred);
    }
}

void ModulesPage::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < chain::typeCount - 1; ++i) if (tile (i).contains (e.getPosition())) h = i;
    if (h != hover) { hover = h; repaint(); }
}

void ModulesPage::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < chain::typeCount - 1; ++i)
        if (tile (i).contains (e.getPosition()))
        {
            processor.setSlotType (target, (chain::Type) (i + 1));
            editor.closePages();
            editor.select (target);
            return;
        }
}

//==============================================================================
namespace
{
an::VuMeter::Style meterStyle (const char* caption)
{
    an::VuMeter::Style s;
    s.caption = caption;
    s.glowAmount = 0.42f;
    return s;
}

// The VU needles read the chain's peak level, integrated by the VU ballistics: 0 VU = -8 dBFS.
constexpr float vuOffsetDb = 8.0f;
// The PEAK lamps light above -1 dBFS (where the protected output starts to act) and stay lit 1/3 s.
constexpr float peakThreshold = 0.891f;
constexpr int peakHoldTicks = 10;

constexpr juce::Point<int> voiceCentre { 405, 110 };
constexpr std::array<int, 3> masterKnobX { 994, 1086, 1178 };
constexpr int masterKnobY = 610, masterKnobSize = 58;
}

ChainEditor::ChainEditor (ChainProcessor& p)
    : AudioProcessorEditor (p), processor (p), vuIn (meterStyle ("IN")), vuOut (meterStyle ("OUT")), presets (*this, p), modules (*this, p)
{
    setLookAndFeel (&look);
    setOpaque (true);
    logo = juce::ImageCache::getFromMemory (BinaryData::swellmark_png, BinaryData::swellmark_pngSize);

    // Header: chain preset, A/B memories, bypass.
    previous.onClick = [this] { const int n = (int) chain::chainPresets().size(); processor.loadChain ((processor.currentChainIndex() + n - 1) % n); };
    next.onClick = [this] { processor.loadChain ((processor.currentChainIndex() + 1) % (int) chain::chainPresets().size()); };
    slotA.setTooltip (fr ("Mémoire A"));
    slotB.setTooltip (fr ("Mémoire B"));
    slotA.onClick = [this] { processor.switchAB (0); };
    slotB.onClick = [this] { processor.switchAB (1); };
    bypass.setTooltip ("Bypass");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::bypass, bypass);
    for (auto* c : std::initializer_list<juce::Component*> { &previous, &next, &slotA, &slotB, &bypass, &bypassLamp }) addAndMakeVisible (c);

    // Voice analysis.
    analyse.setButtonText ("Analyser ma voix");
    analyse.setTooltip (fr ("Chante 12 secondes : Vocal Match se règle sur ta voix"));
    analyse.onClick = [this] { processor.startAnalysis(); };
    addAndMakeVisible (analyse);
    voiceKnob.setRotaryParameters (juce::MathConstants<float>::twoPi - 0.9f, juce::MathConstants<float>::twoPi + 0.9f, true);
    voiceKnob.setPopupDisplayEnabled (false, false, nullptr);
    voiceKnob.setMouseDragSensitivity (90);
    voiceKnob.setTooltip (fr ("Type de voix : Grave, Médium ou Aiguë"));
    voiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::voiceType, voiceKnob);
    addAndMakeVisible (voiceKnob);

    for (int s = 0; s < chain::slotCount; ++s) addAndMakeVisible (cards.add (new SlotCard (*this, processor, s)));

    // Selected module.
    modulePreset.onChange = [this]
    {
        if (modulePreset.getSelectedId() > 0 && modulePreset.getSelectedId() - 1 != processor.slot (selectedSlot).preset)
            processor.setModulePreset (selectedSlot, modulePreset.getSelectedId() - 1);
    };
    auto stepPreset = [this] (int delta)
    {
        const int n = (int) chain::info (processor.slot (selectedSlot).type).presets.size();
        if (n > 0) processor.setModulePreset (selectedSlot, (processor.slot (selectedSlot).preset + delta + n) % n);
    };
    presetPrev.onClick = [stepPreset] { stepPreset (-1); };
    presetNext.onClick = [stepPreset] { stepPreset (1); };
    removeModule.onClick = [this] { processor.removeSlot (selectedSlot); };
    viewToggle.setTooltip (fr ("Réglages / Avant-Après"));
    viewToggle.onClick = [this] { showSpectrum = viewToggle.getToggleState(); rebuildDetail(); };
    for (auto* c : std::initializer_list<juce::Component*> { &modulePreset, &presetPrev, &presetNext, &removeModule }) addChildComponent (c);
    addAndMakeVisible (viewToggle);
    addChildComponent (spectrum);

    // Output section.
    addAndMakeVisible (vuIn);
    addAndMakeVisible (vuOut);
    addAndMakeVisible (peakIn);
    addAndMakeVisible (peakOut);
    peakIn.setLit (false);
    peakOut.setLit (false);
    struct M { an::Knob* k; const juce::String* id; double def; const char* tip; };
    for (auto m : { M { &inputKnob, &ids::input, 0.0, "Entrée" }, M { &mixKnob, &ids::mix, 100.0, "Mix de toute la chaîne" }, M { &gainKnob, &ids::output, 0.0, "Gain de sortie" } })
    {
        m.k->setPopupDisplayEnabled (false, false, nullptr);
        m.k->setTooltip (fr (m.tip));
        m.k->onValueChange = [this] { repaint (masterValues()); };
        addAndMakeVisible (m.k);
    }
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::input, inputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::mix, mixKnob);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::output, gainKnob);
    inputKnob.setDoubleClickReturnValue (true, 0);
    mixKnob.setDoubleClickReturnValue (true, 100);
    gainKnob.setDoubleClickReturnValue (true, 0);
    autoGain.setTooltip ("Volume auto");
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::autoGain, autoGain);
    addAndMakeVisible (autoGain);
    addAndMakeVisible (autoLamp);
    addAndMakeVisible (badge);

    addChildComponent (presets);
    addChildComponent (modules);

    setSize (W, H);
    rebuildDetail();
    startTimerHz (30);
}

ChainEditor::~ChainEditor()
{
    stopTimer();
    voiceAttachment.reset(); inputAttachment.reset(); mixAttachment.reset(); gainAttachment.reset();
    bypassAttachment.reset(); autoGainAttachment.reset();
    setLookAndFeel (nullptr);
}

void ChainEditor::resized()
{
    previous.setBounds (446, 20, 28, 30);
    next.setBounds (812, 20, 28, 30);
    slotA.setBounds (892, 22, 46, 38);
    slotB.setBounds (944, 22, 46, 38);
    bypass.setBounds (1024, 18, 44, 44);
    bypassLamp.setBounds (1068, 26, 28, 28);
    analyse.setBounds (56, 86, 200, 40);
    voiceKnob.setBounds (juce::Rectangle<int> (50, 50).withCentre (voiceCentre));
    for (int s = 0; s < chain::slotCount; ++s) cards[s]->setBounds (cardBounds (s));

    presetPrev.setBounds (378, 424, 28, 32);
    modulePreset.setBounds (406, 424, 206, 32);
    presetNext.setBounds (612, 424, 28, 32);
    viewToggle.setTransform ({});
    viewToggle.setBounds (juce::Rectangle<int> (40, 40).withCentre ({ 744, 440 }));
    // Lever to the left: settings; to the right: before / after.
    viewToggle.setTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, 744.0f, 440.0f));
    removeModule.setBounds (888, 424, 28, 28);
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int n = knobs.size(), area = 848, kw = std::min (170, area / n);
        const int x0 = 66 + (area - kw * n) / 2;
        knobs[i]->setBounds (x0 + i * kw, 478, kw, 258);
    }
    spectrum.setBounds (68, 478, 844, 252);

    vuIn.setBounds (954, 442, 130, 100);
    vuOut.setBounds (1090, 442, 130, 100);
    peakIn.setBounds (juce::Rectangle<int> (20, 20).withCentre ({ 1000, 556 }));
    peakOut.setBounds (juce::Rectangle<int> (20, 20).withCentre ({ 1136, 556 }));
    an::Knob* mk[] { &inputKnob, &mixKnob, &gainKnob };
    for (int i = 0; i < 3; ++i) mk[i]->setBounds (juce::Rectangle<int> (masterKnobSize, masterKnobSize).withCentre ({ masterKnobX[(size_t) i], masterKnobY }));
    autoGain.setTransform ({});
    autoGain.setBounds (juce::Rectangle<int> (36, 36).withCentre ({ 976, 717 }));
    autoGain.setTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, 976.0f, 717.0f));
    autoLamp.setBounds (juce::Rectangle<int> (26, 26).withCentre ({ 1008, 717 }));
    badge.setBounds (860, 758, 368, 30);

    presets.setBounds (0, 77, W, H - 77);
    modules.setBounds (0, 77, W, H - 77);
}

//==============================================================================
void ChainEditor::drawWalnut (juce::Graphics& g, juce::Rectangle<float> r, bool flip)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a170d), flip ? r.getRight() : r.getX(), 0,
                                             juce::Colour (0xff4f2d1a), flip ? r.getX() : r.getRight(), 0, false));
    g.fillRect (r);
    an::Rng rng (flip ? 41u : 17u);
    for (int i = 0; i < 70; ++i)   // grain: long wavy fibres, darker and lighter
    {
        const float x0 = r.getX() + rng.next() * r.getWidth();
        const float amp = 0.6f + rng.next() * 2.2f, freq = 0.004f + rng.next() * 0.012f, ph = rng.next() * 6.28f;
        juce::Path fibre;
        for (float y = 0; y <= r.getHeight(); y += 8)
        {
            const auto pt = juce::Point<float> (x0 + amp * std::sin (y * freq + ph), r.getY() + y);
            if (y == 0) fibre.startNewSubPath (pt); else fibre.lineTo (pt);
        }
        const bool dark = rng.next() < 0.6f;
        g.setColour (dark ? juce::Colours::black.withAlpha (0.12f + 0.2f * rng.next()) : juce::Colour (0xffd9a070).withAlpha (0.05f + 0.06f * rng.next()));
        g.strokePath (fibre, juce::PathStrokeType (0.6f + rng.next() * 1.2f));
    }
    // Satin varnish and rounded edges.
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getCentreX(), 0, juce::Colours::transparentWhite, r.getCentreX(), r.getHeight() * 0.5f, false));
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), flip ? r.getX() : r.getRight(), 0,
                                             juce::Colours::transparentBlack, flip ? r.getX() + 6 : r.getRight() - 6, 0, false));
    g.fillRect (r);
}

void ChainEditor::drawFacade (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    drawWalnut (g, { 0, 0, 26, (float) H }, false);
    drawWalnut (g, { (float) W - 26, 0, 26, (float) H }, true);
    const auto face = juce::Rectangle<float> (26, 0, (float) W - 52, (float) H);
    g.drawImage (brushedFace(), face, juce::RectanglePlacement::stretchToFit);
    an::drawPanelLight (g, face, 1.0f);
    an::drawBevelEdge (g, face, 0, 1.0f);
    for (auto p : { juce::Point<float> (40, 12), juce::Point<float> ((float) W - 40, 12), juce::Point<float> (40, (float) H - 12), juce::Point<float> ((float) W - 40, (float) H - 12) })
        an::drawScrew (g, p, 5.5f, 0.5f + p.x * 0.001f, true);
    auto groove = [&g] (float y, float x0, float x1)
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (x0, y, x1, y, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (x0, y + 1, x1, y + 1, 1.0f);
    };

    // Header.
    if (logo.isValid()) { g.setOpacity (0.92f); g.drawImageWithin (logo, 54, 20, 28, 28, juce::RectanglePlacement::centred); g.setOpacity (1.0f); }
    printSpaced (g, "SWELL", { 92, 18, 140, 26 }, font (21, true), ink, juce::Justification::centredLeft, 0.42f);
    g.setColour (inkDim.withAlpha (0.7f));
    g.fillRect (92.0f, 48.0f, 140.0f, 1.0f);
    printSpaced (g, "CHAIN", { 250, 14, 160, 30 }, font (23, false), ink, juce::Justification::centredLeft, 0.06f);
    printSpaced (g, "VOCAL CHAIN PROCESSOR", { 251, 42, 180, 12 }, font (8.5f, true), inkDim, juce::Justification::centredLeft, 0.2f);
    drawVfdWindow (g, presetBox().toFloat());
    printSpaced (g, fr ("MÉMOIRE"), { 892, 6, 98, 12 }, font (8.5f, true), inkDim, juce::Justification::centred, 0.2f);
    printSpaced (g, "BYPASS", { 1010, 6, 100, 12 }, font (8.5f, true), inkDim, juce::Justification::centred, 0.2f);
    printSpaced (g, "LATENCE FIXE", { 1110, 24, 116, 12 }, font (8.5f, true), inkDim, juce::Justification::centredRight, 0.16f);
    printSpaced (g, fr ("SORTIE PROTÉGÉE"), { 1090, 38, 136, 12 }, font (8.5f, true), inkDim, juce::Justification::centredRight, 0.16f);
    groove (72, 40, (float) W - 40);

    // Voice analysis row.
    {
        const auto c = voiceCentre.toFloat();
        const char* labels[] { "GRAVE", "MÉDIUM", "AIGUË" };
        const float angles[] { -0.9f, 0.0f, 0.9f };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (ink.withAlpha (0.8f));
            g.drawLine ({ c.getPointOnCircumference (27, angles[i]), c.getPointOnCircumference (32, angles[i]) }, 1.5f);
        }
        printSpaced (g, fr (labels[0]), { 306, 86, 64, 12 }, font (9, true), ink, juce::Justification::centredRight, 0.14f);
        printSpaced (g, fr (labels[1]), { 365, 75, 80, 10 }, font (9, true), ink, juce::Justification::centred, 0.14f);
        printSpaced (g, fr (labels[2]), { 440, 86, 64, 12 }, font (9, true), ink, juce::Justification::centredLeft, 0.14f);
        printSpaced (g, "VOIX", { 306, 112, 64, 12 }, font (8.5f, true), inkDim, juce::Justification::centredRight, 0.24f);
    }
    drawVfdWindow (g, findingsBox().toFloat());

    // Rack.
    const auto well = rackWell().toFloat();
    drawWell (g, well, 6, juce::Colour (0xff0d0d0f));
    for (float y : { well.getY() + 5, well.getBottom() - 9 })   // rack rails with their tapped holes
    {
        g.setColour (juce::Colour (0xff1c1c1f));
        g.fillRect (well.getX() + 6, y, well.getWidth() - 12, 4.0f);
        for (float x = well.getX() + 14; x < well.getRight() - 10; x += 24)
        {
            g.setColour (juce::Colours::black);
            g.fillEllipse (x, y + 0.5f, 3, 3);
        }
    }
    for (int s = 0; s < chain::slotCount; ++s)
    {
        const auto cb = cardBounds (s).toFloat();
        printSpaced (g, juce::String (s + 1), { cb.getX(), well.getBottom() + 3, cb.getWidth(), 12 }, font (9, true), inkDim, juce::Justification::centred, 0.1f);
        if (s < chain::slotCount - 1) drawSmallArrow (g, { cb.getRight() + 5, cb.getCentreY() }, juce::Colour (0xff6a6b70));
    }
    // Input and output jacks beside the rack.
    for (auto [y, label] : { std::pair<float, const char*> { 172.0f, "IN" }, std::pair<float, const char*> { 356.0f, "OUT" } })
    {
        const juce::Point<float> c (1224, y);
        juce::ColourGradient nut (juce::Colour (0xffd8d8d4), c.x - 9, c.y - 9, juce::Colour (0xff4a4b4e), c.x + 9, c.y + 9, false);
        g.setGradientFill (nut);
        g.fillEllipse (c.x - 10, c.y - 10, 20, 20);
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
        printSpaced (g, label, { c.x - 20, y + (label[0] == 'I' ? -24.0f : 12.0f), 40, 10 }, font (8, true), inkDim, juce::Justification::centred, 0.2f);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5b60), 1224, 184, juce::Colour (0xff2a2b2f), 1224, 344, false));
    g.fillRect (1223.0f, 184.0f, 2.0f, 160.0f);

    // Module section.
    drawSubPanel (g, modulePanel().toFloat());
    printSpaced (g, fr ("RÉGLAGES"), { 650, 434, 68, 12 }, font (8.5f, true), inkDim, juce::Justification::centredRight, 0.16f);
    printSpaced (g, fr ("AVANT / APRÈS"), { 768, 434, 100, 12 }, font (8.5f, true), inkDim, juce::Justification::centredLeft, 0.16f);
    groove (470, 66, 914);

    // Output section.
    const auto mp = masterPanel().toFloat();
    drawSubPanel (g, mp);
    printSpaced (g, "SORTIE", { mp.getX() + 14, 418, 100, 14 }, font (11, true), ink, juce::Justification::centredLeft, 0.2f);
    for (float x : { 1012.0f, 1148.0f })
        printSpaced (g, "PEAK", { x, 550, 50, 12 }, font (8, true), inkDim, juce::Justification::centredLeft, 0.2f);
    const char* names[] { "ENTRÉE", "MIX", "GAIN" };
    for (int i = 0; i < 3; ++i)
    {
        const auto cx = (float) masterKnobX[(size_t) i];
        an::ScaleSpec sc;
        sc.centre = { cx, (float) masterKnobY };
        sc.radius = masterKnobSize * 0.42f + 3;
        sc.tickLen = 5;
        sc.start = -2.36f; sc.end = 2.36f;
        sc.labels = { "", "", "", "", "" };
        sc.minorPerMajor = 2;
        sc.ink = ink.withAlpha (0.75f);
        an::drawKnobScale (g, sc);
        printSpaced (g, fr (names[i]), { cx - 46, 644, 92, 12 }, font (9.5f, true), ink, juce::Justification::centred, 0.18f);
        drawVfdWindow (g, { cx - 36, 662, 72, 22 });
    }
    groove (696, mp.getX() + 10, mp.getRight() - 10);
    printSpaced (g, "VOLUME AUTO", { 1026, 711, 110, 12 }, font (9.5f, true), ink, juce::Justification::centredLeft, 0.18f);
    drawVfdWindow (g, autoValueBox().toFloat());

    // Footer.
    groove (750, 40, (float) W - 40);
    printSpaced (g, fr ("SWELL-AUDIO  ·  FRANCE  ·  CHAIN V" JucePlugin_VersionString "  ·  BY @SEVYONDATRACKS"), { 56, 760, 520, 28 },
                 font (8.5f, true), inkDim, juce::Justification::centredLeft, 0.16f);
}

void ChainEditor::paint (juce::Graphics& g)
{
    facade.get (g, getLocalBounds(), [this] (juce::Graphics& ig) { drawFacade (ig); });
    facade.blit (g, getLocalBounds().toFloat());
    const auto clip = g.getClipBounds();

    // Chain preset display.
    if (clip.intersects (presetBox()))
    {
        const auto& preset = chain::chainPresets()[(size_t) processor.currentChainIndex()];
        const bool user = processor.userChainName().isNotEmpty();
        const auto style = user ? juce::String ("PERSO") : styleUpper (preset.style);
        auto box = presetBox().toFloat();
        const auto f = vfdFont (11).withExtraKerningFactor (0.1f);
        const float chipW = juce::GlyphArrangement::getStringWidth (f, style) + 14;
        auto chip = juce::Rectangle<float> (box.getX() + 46, box.getCentreY() - 10, chipW, 20);
        g.setColour ((user ? colours::good : styleColour (fr (preset.style))).withAlpha (0.6f));
        g.drawRect (chip, 1.0f);
        g.setColour (user ? colours::good : styleColour (fr (preset.style)));
        g.setFont (f);
        g.drawText (style, chip, juce::Justification::centred, false);
        drawVfdText (g, upper (user ? processor.userChainName() : fr (preset.name)),
                     { chip.getRight() + 14, box.getY(), box.getRight() - chip.getRight() - 120, box.getHeight() }, 19, juce::Justification::centredLeft);
        drawVfdText (g, fr ("LISTE ▾"), { box.getRight() - 112, box.getY(), 70, box.getHeight() }, 10, juce::Justification::centredRight, 0.6f);
    }

    // Analysis display: progress while listening, then what was found.
    if (clip.intersects (findingsBox()))
    {
        auto box = findingsBox().toFloat().reduced (14, 0);
        if (processor.isAnalysing())
        {
            drawVfdText (g, processor.isRecording() ? fr ("ÉCOUTE") : fr ("ANALYSE…"), box.withWidth (90), 12, juce::Justification::centredLeft);
            const int segments = 40, lit = juce::roundToInt (progress * segments);
            const float x0 = box.getX() + 100, w = (box.getWidth() - 110) / (float) segments;
            for (int i = 0; i < segments; ++i)
            {
                g.setColour (i < lit ? colours::vfd : colours::accent.withAlpha (0.12f));
                g.fillRect (x0 + (float) i * w, box.getCentreY() - 5, w - 3, 10.0f);
            }
        }
        else if (processor.isAnalysed())
        {
            drawVfdText (g, "ANALYSE  >", box.withWidth (90), 11, juce::Justification::centredLeft, 0.55f);
            drawVfdText (g, upper (findings.joinIntoString ("  |  ")), box.withTrimmedLeft (96), 12.5f, juce::Justification::centredLeft);
        }
        else
            drawVfdText (g, fr ("ANALYSE  >  CHANTE 12 SECONDES APRÈS « ANALYSER MA VOIX »"), box, 11.5f, juce::Justification::centredLeft, 0.5f);
    }

    // Selected module: colour, name and place.
    const auto s = processor.slot (selectedSlot);
    if (clip.intersects (moduleHeader()) || clip.intersects (juce::Rectangle<int> (870, 452, 64, 14)))
    {
        const auto hdr = moduleHeader().toFloat();
        if (s.type != chain::Type::empty)
        {
            const auto colour = moduleColour (s.type);
            g.setColour (colour.withAlpha (0.3f));
            g.fillEllipse (hdr.getX() + 6, hdr.getY() + 10, 14, 14);
            g.setColour (colour);
            g.fillEllipse (hdr.getX() + 9, hdr.getY() + 13, 8, 8);
            printSpaced (g, upper (fr (chain::info (s.type).name)), { hdr.getX() + 28, hdr.getY() + 4, 300, 26 }, font (19, true), ink, juce::Justification::centredLeft, 0.18f);
            printSpaced (g, "RETIRER", { 870, 452, 64, 12 }, font (7.5f, true), inkDim, juce::Justification::centred, 0.18f);
        }
        else
            printSpaced (g, "EMPLACEMENT VIDE", { hdr.getX() + 8, hdr.getY() + 4, 320, 26 }, font (17, true), inkDim, juce::Justification::centredLeft, 0.18f);
        printSpaced (g, "EMPLACEMENT " + juce::String (selectedSlot + 1), { hdr.getX() + 30, hdr.getY() + 30, 200, 12 }, font (8.5f, true), inkDim, juce::Justification::centredLeft, 0.2f);
    }
    if (s.type == chain::Type::empty && ! showSpectrum)
    {
        printSpaced (g, "AUCUN MODULE DANS CET EMPLACEMENT", { 66, 570, 848, 16 }, font (11, true), inkDim, juce::Justification::centred, 0.18f);
        printSpaced (g, "CLIQUE SUR LA PLAQUE « + MODULE » DU RACK", { 66, 594, 848, 16 }, font (10, true), colours::vfd.withAlpha (0.8f), juce::Justification::centred, 0.18f);
    }

    // Output values.
    if (clip.intersects (masterValues()))
    {
        auto db = [] (double v) { return (v > 0.05 ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB"; };
        const juce::String values[] { db (inputKnob.getValue()), juce::String (juce::roundToInt (mixKnob.getValue())) + " %", db (gainKnob.getValue()) };
        for (int i = 0; i < 3; ++i)
            drawVfdText (g, values[i], { (float) masterKnobX[(size_t) i] - 36, 662, 72, 22 }, 12, juce::Justification::centred);
    }
    if (clip.intersects (autoValueBox()))
        drawVfdText (g, shownAuto.dropLastCharacters (1), autoValueBox().toFloat(), 13, juce::Justification::centred, autoGain.getToggleState() ? 1.0f : 0.25f);
}

void ChainEditor::mouseUp (const juce::MouseEvent& e)
{
    if (presetBox().contains (e.getPosition())) openPresets();
}

void ChainEditor::select (int slot)
{
    selectedSlot = juce::jlimit (0, chain::slotCount - 1, slot);
    rebuildDetail();
    for (auto* c : cards) c->refresh();
}

void ChainEditor::rebuildDetail()
{
    knobs.clear();
    const auto s = processor.slot (selectedSlot);
    detailType = s.type;
    detailFor = selectedSlot;
    const auto& info = chain::info (s.type);
    const int n = (int) info.knobs.size();
    const int size = n <= 3 ? 112 : n <= 5 ? 96 : n == 6 ? 86 : 74;
    for (int k = 0; k < n; ++k)
        addChildComponent (knobs.add (new KnobView (processor, selectedSlot, k, info.knobs[(size_t) k], moduleColour (s.type), size)));
    modulePreset.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) info.presets.size(); ++i) modulePreset.addItem (upper (fr (info.presets[(size_t) i].c_str())), i + 1);
    modulePreset.setSelectedId (s.preset + 1, juce::dontSendNotification);
    const bool hasPresets = ! info.presets.empty();
    for (auto* c : std::initializer_list<juce::Component*> { &modulePreset, &presetPrev, &presetNext }) c->setVisible (hasPresets);
    removeModule.setVisible (s.type != chain::Type::empty);
    for (auto* k : knobs) k->setVisible (! showSpectrum);
    spectrum.setVisible (showSpectrum);
    resized();
    repaint (modulePanel());
}

void ChainEditor::openPresets() { modules.setVisible (false); presets.setVisible (true); presets.toFront (false); }

void ChainEditor::openModules (int slot)
{
    presets.setVisible (false);
    modules.target = slot;
    modules.setVisible (true);
    modules.toFront (false);
    modules.repaint();
}

void ChainEditor::closePages() { presets.setVisible (false); modules.setVisible (false); }

void ChainEditor::dragOver (int from, juce::Point<int> screenPos, bool finished)
{
    dragSlot = from;
    dropSlot = -1;
    const auto local = getLocalPoint (nullptr, screenPos);
    for (auto* c : cards) if (c->getBounds().contains (local)) dropSlot = c->index;
    if (finished)
    {
        if (dropSlot >= 0 && dropSlot != from)
        {
            // Dropped on an empty place: the module goes to the first free place, never leaving a gap.
            int target = dropSlot;
            if (processor.slot (target).type == chain::Type::empty)
            {
                int last = -1;
                for (int s = 0; s < chain::slotCount; ++s) if (processor.slot (s).type != chain::Type::empty) last = s;
                target = std::max (0, last);
            }
            processor.moveSlot (from, target);
            select (target);
        }
        dragSlot = dropSlot = -1;
    }
    for (auto* c : cards) c->refresh();
}

void ChainEditor::showBeforeAfter (bool on)
{
    viewToggle.setToggleState (on, juce::sendNotification);
    showSpectrum = on;
    rebuildDetail();
}

void ChainEditor::settleMeters()
{
    timerCallback();
    vuIn.jumpToTarget();
    vuOut.jumpToTarget();
}

void ChainEditor::timerCallback()
{
    processor.pollAnalysis();

    // Meters: the levels come from the audio thread through atomics; the needles move on their own, repainting only when they move.
    const float inPeak = processor.takeInputPeak(), outPeak = processor.takeOutputPeak();
    vuIn.setTargetDb (juce::Decibels::gainToDecibels (inPeak, -100.0f) + vuOffsetDb);
    vuOut.setTargetDb (juce::Decibels::gainToDecibels (outPeak, -100.0f) + vuOffsetDb);
    peakHoldIn = inPeak > peakThreshold ? peakHoldTicks : std::max (0, peakHoldIn - 1);
    peakHoldOut = outPeak > peakThreshold ? peakHoldTicks : std::max (0, peakHoldOut - 1);
    peakIn.setLit (peakHoldIn > 0);
    peakOut.setLit (peakHoldOut > 0);
    bypassLamp.setLit (bypass.getToggleState());
    autoLamp.setLit (autoGain.getToggleState());

    // Analysis.
    if (processor.isAnalysed())
    {
        findings.clear();
        for (const auto& line : vm::describe (processor.analysisResult()))
            findings.add (fr (line.c_str()).replace (fr ("Résonance vers "), fr ("Résonance ")));
        for (const auto& tag : chain::describe (processor.voiceStats(), processor.voiceSettings())) findings.add (fr (tag.c_str()));
    }
    progress = processor.analysisProgress();
    analyse.setButtonText (processor.isRecording() ? juce::String (juce::roundToInt (progress * 100)) + " %"
                           : processor.isAnalysing() ? fr ("Analyse…")
                           : processor.isAnalysed() ? fr ("Relancer") : fr ("Analyser ma voix"));
    analyse.setLit (! processor.isRecording() || (juce::Time::getMillisecondCounter() / 350) % 2 == 0);
    const auto findingsKey = juce::String (processor.isAnalysing() ? juce::roundToInt (progress * 40) : -1) + (processor.isAnalysed() ? findings.joinIntoString ("|") : juce::String());
    if (findingsKey != shownFindings) { shownFindings = findingsKey; repaint (findingsBox()); }

    slotA.setToggleState (processor.activeSlotAB() == 0, juce::dontSendNotification);
    slotB.setToggleState (processor.activeSlotAB() == 1, juce::dontSendNotification);
    const auto header = juce::String (processor.currentChainIndex()) + processor.userChainName();
    if (header != shownHeader) { shownHeader = header; repaint (presetBox()); }

    const auto s = processor.slot (selectedSlot);
    if (s.type != detailType || detailFor != selectedSlot) rebuildDetail();
    else if (modulePreset.isVisible() && modulePreset.getSelectedId() != s.preset + 1)
        modulePreset.setSelectedId (s.preset + 1, juce::dontSendNotification);
    for (auto* c : cards) c->refresh();
    if (showSpectrum) spectrum.update (processor);

    const float autoDb = processor.autoGainDb();
    const auto autoText = (autoDb > 0.05f ? "+" : "") + juce::String (autoDb, 1).replaceCharacter ('.', ',') + " dB";
    const auto autoKey = autoText + (autoGain.getToggleState() ? "1" : "0");
    if (autoKey != shownAuto) { shownAuto = autoKey; repaint (autoValueBox()); }
}
