#include "PluginEditor.h"
#include <BinaryData.h>

using namespace ui;

namespace
{
juce::String fr (const char* s) { return juce::String::fromUTF8 (s); }

juce::Colour moduleColour (chain::Type t) { return juce::Colour (chain::info (t).colour); }

void arc (juce::Graphics& g, juce::Rectangle<float> r, float proportion, juce::Colour colour, float thickness)
{
    const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
    const float radius = std::min (r.getWidth(), r.getHeight()) / 2 - thickness / 2;
    juce::Path back, front;
    back.addCentredArc (r.getCentreX(), r.getCentreY(), radius, radius, 0, start, end, true);
    g.setColour (juce::Colour (0xff2a2f39));
    g.strokePath (back, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (proportion > 0.001f)
    {
        front.addCentredArc (r.getCentreX(), r.getCentreY(), radius, radius, 0, start, start + (end - start) * proportion, true);
        g.setColour (colour);
        g.strokePath (front, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::String styleUpper (const char* s) { return fr (s).toUpperCase(); }

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
    b.setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff2b3242) : colours::field);
    b.setColour (juce::TextButton::textColourOffId, on ? colours::text : colours::dim);
}
}

//==============================================================================
juce::Font ui::font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

Look::Look()
{
    setColour (juce::TextButton::buttonColourId, colours::field);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2b3242));
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc9cfda));
    setColour (juce::TextButton::textColourOnId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::field);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff343a46));
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1e25));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2b3242));
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a404c));
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
    const float size = std::min (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    arc (g, r, pos, s.findColour (juce::Slider::rotarySliderFillColourId), 5);
    g.setColour (juce::Colour (0xff20242c));
    g.fillEllipse (r.reduced (size * 0.17f));
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h / 2;
    g.setColour (juce::Colour (0xff2a2f39));
    g.fillRoundedRectangle ((float) x, cy - 3, (float) w, 6, 3);
    g.setColour (colours::accent);
    g.fillRoundedRectangle ((float) x, cy - 3, pos - (float) x, 6, 3);
    g.fillEllipse (pos - 8, cy - 8, 16, 16);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (over) fill = fill.brighter (0.06f);
    if (down) fill = fill.darker (0.1f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 7);
    g.setColour (b.getToggleState() ? juce::Colour (0xff4a5570) : juce::Colour (0xff343a46));
    if (fill != colours::accent) g.drawRoundedRectangle (r, 7, 1);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 7);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 7, 1);
    juce::Path p;
    const float cx = (float) w - 18, cy = (float) h / 2;
    p.startNewSubPath (cx - 5, cy - 2); p.lineTo (cx, cy + 3); p.lineTo (cx + 5, cy - 2);
    g.setColour (colours::dim);
    g.strokePath (p, juce::PathStrokeType (2, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::String ui::formatValue (const chain::KnobSpec& s, float v)
{
    const juce::String unit (s.unit);
    if (s.toggle) return v > 0.5f ? "On" : "Off";
    if (unit == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB";
    if (unit == "st") return (v > 0.05f ? "+" : "") + juce::String (juce::roundToInt (v)) + " dt";
    return juce::String (juce::roundToInt (v)) + " %";
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

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? colours::field.darker (0.1f) : over ? colours::field.brighter (0.06f) : colours::field);
    g.fillRoundedRectangle (r, 6);
    g.setColour (getToggleState() ? onColour.withAlpha (0.6f) : juce::Colour (0xff343a46));
    g.drawRoundedRectangle (r, 6, 1);
    const float size = std::min (r.getWidth(), r.getHeight()) * 0.62f;
    drawGlyph (g, glyph, r.withSizeKeepingCentre (size, size), getToggleState() ? onColour : colours::dim, 2.2f);
}

//==============================================================================
SlotCard::SlotCard (ChainEditor& e, ChainProcessor& p, int i) : index (i), editor (e), processor (p)
{
    onButton.setClickingTogglesState (true);
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::on (i), onButton);
    soloButton.onColour = juce::Colour (0xfff2b25c);
    soloButton.onClick = [this] { processor.setSolo (processor.soloSlot() == index ? -1 : index); };
    removeButton.onClick = [this] { processor.removeSlot (index); editor.select (index); };
    for (auto* b : std::initializer_list<juce::Button*> { &onButton, &soloButton, &removeButton }) addAndMakeVisible (b);
    refresh();
}

void SlotCard::refresh()
{
    const auto s = processor.slot (index);
    const bool filled = s.type != chain::Type::empty;
    for (auto* b : std::initializer_list<juce::Button*> { &onButton, &soloButton, &removeButton }) b->setVisible (filled);
    soloButton.setToggleState (processor.soloSlot() == index, juce::dontSendNotification);
    setTooltip (filled ? fr (chain::info (s.type).name) : fr ("Ajouter un module"));
    repaint();
}

void SlotCard::resized()
{
    onButton.setBounds (getWidth() - 42, 22, 30, 30);
    soloButton.setBounds (getWidth() - 42, getHeight() - 38, 30, 28);
    removeButton.setBounds (getWidth() - 78, getHeight() - 38, 30, 28);
}

void SlotCard::paint (juce::Graphics& g)
{
    const auto s = processor.slot (index);
    auto r = getLocalBounds().toFloat().reduced (1);
    const bool selected = editor.selected() == index;
    const bool drop = editor.dropTarget() == index && editor.dragSource() != index;
    if (s.type == chain::Type::empty)
    {
        juce::Path outline;
        outline.addRoundedRectangle (r, 12);
        juce::Path dashed;
        const float dashes[] { 7, 5 };
        juce::PathStrokeType (2).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (drop || selected ? colours::accent : juce::Colour (0xff353b47));
        g.fillPath (dashed);
        auto c = r.withSizeKeepingCentre (52, 52);
        g.setColour (colours::field);
        g.fillEllipse (c);
        g.setColour (colours::dim);
        juce::Path plus;
        plus.startNewSubPath (c.getCentreX() - 10, c.getCentreY()); plus.lineTo (c.getCentreX() + 10, c.getCentreY());
        plus.startNewSubPath (c.getCentreX(), c.getCentreY() - 10); plus.lineTo (c.getCentreX(), c.getCentreY() + 10);
        g.strokePath (plus, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
    const auto& info = chain::info (s.type);
    const auto colour = moduleColour (s.type);
    const bool active = s.on && (processor.soloSlot() < 0 || processor.soloSlot() == index);
    g.setColour (active ? colours::panel : juce::Colour (0xff15171c));
    g.fillRoundedRectangle (r, 12);
    g.setColour (drop ? colours::accent : selected ? colour : colours::border);
    g.drawRoundedRectangle (r, 12, selected || drop ? 2.0f : 1.0f);
    g.setColour (colour.withAlpha (active ? 1.0f : 0.3f));
    g.fillRoundedRectangle (r.getX() + 13, r.getY() + 11, r.getWidth() - 26, 4, 2);

    // The module's icon stands for its name (shown in the tooltip and in the settings panel).
    auto iconBox = juce::Rectangle<float> (14, 26, 46, 46);
    g.setColour (colour.withAlpha (active ? 0.14f : 0.06f));
    g.fillRoundedRectangle (iconBox, 10);
    drawModuleIcon (g, s.type, iconBox.reduced (9), active ? colour : colour.withAlpha (0.35f), 2.0f);
    g.setColour (active ? colours::text : juce::Colour (0xff7d8492));
    g.setFont (font (15, true));
    g.drawText (fr (info.name), 14, 76, getWidth() - 28, 20, juce::Justification::centredLeft, true);

    auto presetRow = juce::Rectangle<float> (13, 102, (float) getWidth() - 26, 28);
    g.setColour (juce::Colour (0xff13161b));
    g.fillRoundedRectangle (presetRow, 6);
    g.setColour (colours::border);
    g.drawRoundedRectangle (presetRow, 6, 1);
    juce::String presetName = info.presets.empty() ? fr (info.role)
                                                   : fr (info.presets[(size_t) juce::jlimit (0, (int) info.presets.size() - 1, s.preset)].c_str());
    g.setColour (active ? juce::Colour (0xffd5dae3) : colours::faint);
    g.setFont (font (12, true));
    g.drawText (presetName, presetRow.reduced (9, 0), juce::Justification::centredLeft, true);

    // The first two knobs, as small read-only dials.
    const int shown = std::min (2, (int) info.knobs.size());
    for (int k = 0; k < shown; ++k)
    {
        const auto& spec = info.knobs[(size_t) k];
        const float cx = shown == 1 ? getWidth() / 2.0f : getWidth() * (k == 0 ? 0.3f : 0.7f);
        auto dial = juce::Rectangle<float> (cx - 25, 140, 50, 50);
        arc (g, dial, (s.knobs[(size_t) k] - spec.min) / (spec.max - spec.min), active ? colour : juce::Colour (0xff4a4558), 4.5f);
        g.setColour (juce::Colour (0xff1d2027));
        g.fillEllipse (dial.reduced (11));
        g.setColour (colours::text);
        g.setFont (font (11, true));
        g.drawText (formatValue (spec, s.knobs[(size_t) k]).upToFirstOccurrenceOf (" ", false, false), dial, juce::Justification::centred);
    }
    // Drag handle: six dots.
    g.setColour (colours::faint);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (16.0f + (float) col * 6, (float) getHeight() - 30 + (float) row * 6, 3, 3);
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
KnobView::KnobView (ChainProcessor& p, int s, int k, const chain::KnobSpec& sp, juce::Colour colour)
    : processor (p), spec (sp), slot (s), knob (k)
{
    if (spec.toggle)
    {
        toggle.setButtonText (fr (spec.label));
        toggle.onClick = [this] { processor.setKnob (slot, knob, processor.slot (slot).knobs[(size_t) knob] > 0.5f ? 0.0f : 1.0f); };
        addAndMakeVisible (toggle);
        return;
    }
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setTitle (fr (spec.label));
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::knob (s, k), slider);
    slider.onValueChange = [this] { repaint(); };
    const float def = (spec.def - spec.min) / (spec.max - spec.min);
    slider.setDoubleClickReturnValue (true, def);
}

void KnobView::resized()
{
    if (spec.toggle) { toggle.setBounds (getLocalBounds().withSizeKeepingCentre (getWidth() - 16, 40)); return; }
    slider.setBounds (getLocalBounds().withTrimmedTop (22).withHeight (96).withSizeKeepingCentre (96, 96));
}

void KnobView::paint (juce::Graphics& g)
{
    if (spec.toggle)
    {
        const bool on = processor.slot (slot).knobs[(size_t) knob] > 0.5f;
        toggle.setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xff2b2a24) : colours::field);
        toggle.setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xfff2b25c) : colours::dim);
        return;
    }
    g.setColour (colours::dim);
    g.setFont (font (12, true));
    g.drawText (fr (spec.label), 0, 0, getWidth(), 18, juce::Justification::centred);
}

void KnobView::paintOverChildren (juce::Graphics& g)
{
    if (spec.toggle) return;
    const float real = spec.min + (float) slider.getValue() * (spec.max - spec.min);
    g.setColour (colours::text);
    g.setFont (font (15, true));
    g.drawText (formatValue (spec, real), slider.getBounds(), juce::Justification::centred);
}

//==============================================================================
PresetsPage::PresetsPage (ChainEditor& e, ChainProcessor& p) : editor (e), processor (p)
{
    close.onClick = [this] { editor.closePages(); };
    addAndMakeVisible (close);
    for (auto name : { "Tous", "Trap", "R&B", "Rap", "Afro", "Drill", "Rage", "FX", "Topline" })
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

std::vector<int> PresetsPage::visible() const
{
    std::vector<int> v;
    const auto& all = chain::chainPresets();
    for (int i = 0; i < (int) all.size(); ++i)
        if (style == "Tous" || fr (all[(size_t) i].style) == style) v.push_back (i);
    return v;
}

void PresetsPage::setStyle (const juce::String& s)
{
    style = s;
    for (auto* b : styleButtons) styleButton (*b, b->getButtonText() == style);
    list.setSize (viewport.getWidth() > 0 ? viewport.getWidth() - 10 : 1060, std::max (1, (int) visible().size()) * 40 + 4);
    list.repaint();
}

void PresetsPage::resized()
{
    close.setBounds (getWidth() - 62, 14, 34, 34);
    int y = 70;
    for (auto* b : styleButtons) { b->setBounds (12, y, 156, 34); y += 38; }
    viewport.setBounds (181, 96, getWidth() - 181, getHeight() - 96);
    setStyle (style);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
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
    g.drawText ("NOM", 200, 64, 200, 28, juce::Justification::centredLeft);
    g.drawText (fr ("CHAÎNE"), 440, 64, 200, 28, juce::Justification::centredLeft);
    g.drawText ("STYLE", getWidth() - 150, 64, 100, 28, juce::Justification::centredLeft);
    g.setColour (colours::line);
    g.drawHorizontalLine (95, 181, (float) getWidth());
}

void PresetsPage::List::paint (juce::Graphics& g)
{
    const auto rows = page.visible();
    const auto& all = chain::chainPresets();
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& p = all[(size_t) rows[(size_t) i]];
        const auto row = juce::Rectangle<int> (0, i * 40, getWidth(), 40);
        const bool current = rows[(size_t) i] == page.processor.currentChainIndex();
        if (current) { g.setColour (juce::Colour (0xff232838)); g.fillRect (row); g.setColour (colours::accent); g.fillRect (row.withWidth (3)); }
        else if (i == hover) { g.setColour (juce::Colour (0xff1a1d24)); g.fillRect (row); }
        g.setColour (juce::Colour (0xff1d2027));
        g.drawHorizontalLine (row.getBottom() - 1, 0, (float) getWidth());
        g.setColour (colours::text);
        g.setFont (font (14, current));
        g.drawText (fr (p.name), 19, row.getY(), 230, 40, juce::Justification::centredLeft);
        int x = 259;
        for (const auto& step : p.steps)
        {
            const auto c = moduleColour (step.type);
            const juce::String label (shortName (step.type));
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
        g.drawText (fr (p.style), getWidth() - 140, row.getY(), 100, 40, juce::Justification::centredLeft);
    }
}

void PresetsPage::List::mouseMove (const juce::MouseEvent& e) { hover = e.y / 40; repaint(); }

void PresetsPage::List::mouseUp (const juce::MouseEvent& e)
{
    const auto rows = page.visible();
    const int i = e.y / 40;
    if (i < 0 || i >= (int) rows.size()) return;
    page.processor.loadChain (rows[(size_t) i]);
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
    g.fillAll (colours::background);
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
        g.setColour (hover == t - 1 ? juce::Colour (0xff1d2128) : colours::panel);
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
ChainEditor::ChainEditor (ChainProcessor& p) : AudioProcessorEditor (p), processor (p), presets (*this, p), modules (*this, p)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BinaryData::swellmark_png, BinaryData::swellmark_pngSize);

    previous.onClick = [this] { const int n = (int) chain::chainPresets().size(); processor.loadChain ((processor.currentChainIndex() + n - 1) % n); };
    next.onClick = [this] { processor.loadChain ((processor.currentChainIndex() + 1) % (int) chain::chainPresets().size()); };
    previous.setTooltip (fr ("Preset précédent"));
    next.setTooltip ("Preset suivant");
    slotA.onClick = [this] { processor.switchAB (0); };
    slotB.onClick = [this] { processor.switchAB (1); };
    bypass.setClickingTogglesState (true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::bypass, bypass);
    analyse.setColour (juce::TextButton::buttonColourId, colours::accent);
    analyse.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a1209));
    analyse.onClick = [this] { processor.startAnalysis(); };
    const char* voices[] { "Grave", "Médium", "Aiguë" };
    for (int i = 0; i < 3; ++i)
    {
        voice[i].setButtonText (fr (voices[i]));
        voice[i].onClick = [this, i]
        {
            auto* param = processor.state.getParameter (ids::voiceType);
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
            param->endChangeGesture();
        };
    }
    for (auto* b : { &previous, &next, &slotA, &slotB, &bypass, &analyse, &voice[0], &voice[1], &voice[2] }) addAndMakeVisible (b);

    for (int s = 0; s < chain::slotCount; ++s) addAndMakeVisible (cards.add (new SlotCard (*this, processor, s)));

    modulePreset.onChange = [this]
    {
        if (modulePreset.getSelectedId() > 0 && modulePreset.getSelectedId() - 1 != processor.slot (selectedSlot).preset)
            processor.setModulePreset (selectedSlot, modulePreset.getSelectedId() - 1);
    };
    removeModule.onClick = [this] { processor.removeSlot (selectedSlot); };
    addChildComponent (modulePreset);
    addChildComponent (removeModule);

    autoGain.setClickingTogglesState (true);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.state, ids::autoGain, autoGain);
    for (auto* s : { &mix, &gain })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->onValueChange = [this] { repaint (960, 404, 292, 300); };
        addAndMakeVisible (s);
    }
    mix.setDoubleClickReturnValue (true, 100);
    gain.setDoubleClickReturnValue (true, 0);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::mix, mix);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.state, ids::output, gain);
    addAndMakeVisible (autoGain);

    addChildComponent (presets);
    addChildComponent (modules);

    setSize (1280, 800);
    rebuildDetail();
    startTimerHz (30);
}

ChainEditor::~ChainEditor() { setLookAndFeel (nullptr); }

void ChainEditor::resized()
{
    previous.setBounds (788, 18, 40, 40);
    next.setBounds (834, 18, 40, 40);
    slotA.setBounds (1060, 18, 44, 40);
    slotB.setBounds (1110, 18, 44, 40);
    bypass.setBounds (1162, 18, 90, 40);
    analyse.setBounds (28, 90, 180, 40);
    for (int i = 0; i < 3; ++i) voice[i].setBounds (220 + i * 86, 90, 82, 40);
    const int w = (1224 - 5 * 22) / 6;
    for (int s = 0; s < chain::slotCount; ++s) cards[s]->setBounds (28 + s * (w + 22), 148, w, 238);
    modulePreset.setBounds (400, 418, 260, 34);
    removeModule.setBounds (894, 418, 34, 34);
    for (int i = 0; i < knobs.size(); ++i)
    {
        const int n = knobs.size(), area = 916 - 44, kw = std::min (150, area / n);
        const int x0 = 28 + 22 + (area - kw * n) / 2;
        knobs[i]->setBounds (x0 + i * kw, 476, kw, 150);
    }
    autoGain.setBounds (1112, 452, 120, 32);
    mix.setBounds (984, 616, 248, 24);
    gain.setBounds (984, 668, 248, 24);
    presets.setBounds (0, 77, 1280, 683);
    modules.setBounds (0, 77, 1280, 683);
}

void ChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    // Header
    g.drawImageWithin (logo, 28, 22, 34, 31, juce::RectanglePlacement::centred);
    g.setColour (colours::text);
    g.setFont (font (22, true));
    g.drawText ("swell", 72, 20, 80, 24, juce::Justification::left);
    g.setColour (colours::dim);
    g.setFont (font (9));
    g.drawText ("A U D I O", 73, 44, 80, 12, juce::Justification::left);
    g.setColour (juce::Colour (0xff2c313b));
    g.fillRect (156, 22, 1, 32);
    g.setColour (colours::text);
    g.setFont (font (24, true));
    g.drawText ("CHAIN", 172, 20, 120, 34, juce::Justification::left);

    const auto& preset = chain::chainPresets()[(size_t) processor.currentChainIndex()];
    auto box = presetBox().toFloat();
    g.setColour (colours::field);
    g.fillRoundedRectangle (box, 8);
    g.setColour (juce::Colour (0xff343a46));
    g.drawRoundedRectangle (box.reduced (0.5f), 8, 1);
    const auto style = styleUpper (preset.style);
    const int chipW = (int) juce::GlyphArrangement::getStringWidth (font (11, true), style) + 18;
    g.setColour (juce::Colour (0xff2a2f3a));
    g.fillRoundedRectangle (box.getX() + 12, box.getCentreY() - 11, (float) chipW, 22, 11);
    g.setColour (styleColour (fr (preset.style)));
    g.setFont (font (11, true));
    g.drawText (style, (int) box.getX() + 12, (int) box.getCentreY() - 11, chipW, 22, juce::Justification::centred);
    g.setColour (colours::text);
    g.setFont (font (15, true));
    g.drawText (fr (preset.name), (int) box.getX() + chipW + 24, (int) box.getY(), 240, (int) box.getHeight(), juce::Justification::centredLeft);
    juce::Path chevron;
    chevron.startNewSubPath (box.getRight() - 24, box.getCentreY() - 3);
    chevron.lineTo (box.getRight() - 19, box.getCentreY() + 2);
    chevron.lineTo (box.getRight() - 14, box.getCentreY() - 3);
    g.setColour (colours::dim);
    g.strokePath (chevron, juce::PathStrokeType (2, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::line);
    g.drawHorizontalLine (76, 0, 1280);

    // Analysis row: a progress bar while listening, then what was found, as small tags.
    if (processor.isAnalysing())
    {
        auto bar = juce::Rectangle<float> (488, 107, 360, 6);
        g.setColour (juce::Colour (0xff23272f));
        g.fillRoundedRectangle (bar, 3);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3);
    }
    else if (processor.isAnalysed())
    {
        float x = 488;
        g.setFont (font (12, true));
        for (const auto& tag : findings)
        {
            const float w = juce::GlyphArrangement::getStringWidth (font (12, true), tag) + 22;
            if (x + w > 1240) break;
            g.setColour (juce::Colour (0xff1f232b));
            g.fillRoundedRectangle (x, 96, w, 28, 14);
            g.setColour (juce::Colour (0xff343a46));
            g.drawRoundedRectangle (x + 0.5f, 96.5f, w - 1, 27, 14, 1);
            g.setColour (juce::Colour (0xffd5dae3));
            g.drawText (tag, (int) x, 96, (int) w, 28, juce::Justification::centred);
            x += w + 8;
        }
    }

    // Arrows between cards
    const int w = (1224 - 5 * 22) / 6;
    for (int s = 0; s < chain::slotCount - 1; ++s)
    {
        const float x = 28.0f + (float) (s + 1) * (float) (w + 22) - 17, y = 267;
        juce::Path a;
        a.startNewSubPath (x, y); a.lineTo (x + 12, y);
        a.startNewSubPath (x + 7, y - 5); a.lineTo (x + 12, y); a.lineTo (x + 7, y + 5);
        g.setColour (juce::Colour (0xff4a5161));
        g.strokePath (a, juce::PathStrokeType (2, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Detail panel
    const auto s = processor.slot (selectedSlot);
    auto detail = juce::Rectangle<float> (28, 404, 916, 300);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (detail, 12);
    g.setColour (s.type == chain::Type::empty ? colours::border : moduleColour (s.type));
    g.drawRoundedRectangle (detail.reduced (0.5f), 12, 1);
    if (s.type != chain::Type::empty)
    {
        const auto colour = moduleColour (s.type);
        auto iconBox = juce::Rectangle<float> (48, 416, 38, 38);
        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (iconBox, 9);
        drawModuleIcon (g, s.type, iconBox.reduced (8), colour, 2.0f);
        g.setColour (colours::text);
        g.setFont (font (20, true));
        g.drawText (fr (chain::info (s.type).name), 98, 418, 290, 34, juce::Justification::centredLeft);
    }

    // Output panel
    auto out = juce::Rectangle<float> (960, 404, 292, 300);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (out, 12);
    g.setColour (colours::border);
    g.drawRoundedRectangle (out.reduced (0.5f), 12, 1);
    g.setColour (colours::dim);
    g.setFont (font (12, true));
    g.drawText ("SORTIE", 984, 422, 100, 20, juce::Justification::left);
    auto meter = [&g] (float x, float level, const char* label)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float h = 120 * juce::jlimit (0.0f, 1.0f, (db + 60) / 60);
        g.setColour (juce::Colour (0xff23272f));
        g.fillRoundedRectangle (x, 452, 14, 120, 4);
        g.setColour (db > -1 ? juce::Colour (0xfff18177) : juce::Colour (0xffaebeff));
        g.fillRoundedRectangle (x, 572 - h, 14, h, 4);
        g.setColour (colours::faint);
        g.setFont (font (11));
        g.drawText (label, (int) x - 10, 576, 34, 16, juce::Justification::centred);
    };
    meter (990, inLevel, "IN");
    meter (1024, outLevel, "OUT");
    g.setColour (colours::dim);
    g.setFont (font (11, true));
    g.drawText ("MIX", 984, 596, 100, 18, juce::Justification::left);
    g.drawText ("GAIN", 984, 648, 100, 18, juce::Justification::left);
    g.setColour (colours::text);
    g.setFont (font (12, true));
    g.drawText (juce::String (juce::roundToInt (mix.getValue())) + " %", 1132, 596, 100, 18, juce::Justification::right);
    g.drawText ((gain.getValue() > 0.05 ? "+" : "") + juce::String (gain.getValue(), 1).replaceCharacter ('.', ',') + " dB", 1132, 648, 100, 18, juce::Justification::right);
    if (autoGain.getToggleState())
    {
        g.setColour (colours::dim);
        g.setFont (font (12, true));
        g.drawText ((processor.autoGainDb() > 0.05f ? "+" : "") + juce::String (processor.autoGainDb(), 1).replaceCharacter ('.', ',') + " dB",
                    1112, 490, 120, 18, juce::Justification::centred);
    }

    // Footer
    g.setColour (colours::line);
    g.drawHorizontalLine (760, 0, 1280);
    g.setColour (colours::faint);
    g.setFont (font (12));
    g.drawText ("Swell-audio  /  Chain v" JucePlugin_VersionString, 28, 760, 400, 40, juce::Justification::centredLeft);
    g.drawText ("by @sevyondatracks", 900, 760, 352, 40, juce::Justification::centredRight);
}

void ChainEditor::mouseUp (const juce::MouseEvent& e)
{
    if (presetBox().contains (e.getPosition())) openPresets();
}

void ChainEditor::select (int slot)
{
    selectedSlot = juce::jlimit (0, chain::slotCount - 1, slot);
    rebuildDetail();
    for (auto* c : cards) c->repaint();
}

void ChainEditor::rebuildDetail()
{
    knobs.clear();
    const auto s = processor.slot (selectedSlot);
    detailType = s.type;
    detailFor = selectedSlot;
    const auto& info = chain::info (s.type);
    for (int k = 0; k < (int) info.knobs.size(); ++k)
        addAndMakeVisible (knobs.add (new KnobView (processor, selectedSlot, k, info.knobs[(size_t) k], moduleColour (s.type))));
    modulePreset.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) info.presets.size(); ++i) modulePreset.addItem (fr (info.presets[(size_t) i].c_str()), i + 1);
    modulePreset.setSelectedId (s.preset + 1, juce::dontSendNotification);
    modulePreset.setVisible (! info.presets.empty());
    removeModule.setVisible (s.type != chain::Type::empty);
    resized();
    repaint();
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
    for (auto* c : cards) c->repaint();
}

void ChainEditor::timerCallback()
{
    processor.pollAnalysis();
    inLevel = std::max (processor.takeInputPeak(), inLevel * 0.85f);
    outLevel = std::max (processor.takeOutputPeak(), outLevel * 0.85f);
    if (processor.isAnalysed())
    {
        findings.clear();
        for (const auto& line : vm::describe (processor.analysisResult()))
            findings.add (fr (line.c_str()).replace (fr ("Résonance vers "), fr ("Résonance ")));
    }
    progress = processor.analysisProgress();
    analyse.setButtonText (processor.isRecording() ? juce::String (juce::roundToInt (progress * 100)) + " %"
                           : processor.isAnalysing() ? fr ("…")
                           : processor.isAnalysed() ? fr ("Relancer") : fr ("Analyser ma voix"));
    const int voiceType = juce::roundToInt (processor.state.getRawParameterValue (ids::voiceType)->load());
    for (int i = 0; i < 3; ++i) styleButton (voice[i], i == voiceType);
    styleButton (slotA, processor.activeSlotAB() == 0);
    styleButton (slotB, processor.activeSlotAB() == 1);

    const auto s = processor.slot (selectedSlot);
    if (s.type != detailType || detailFor != selectedSlot) rebuildDetail();
    else if (modulePreset.isVisible() && modulePreset.getSelectedId() != s.preset + 1)
        modulePreset.setSelectedId (s.preset + 1, juce::dontSendNotification);
    for (auto* c : cards) c->refresh();
    for (auto* k : knobs) k->repaint();
    repaint (0, 0, 1280, 140);
    repaint (960, 404, 292, 300);
}
