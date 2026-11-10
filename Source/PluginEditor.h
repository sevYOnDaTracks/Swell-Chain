#pragma once
#include "PluginProcessor.h"
#include "UserLibrary.h"
#include "Analog/AnalogComponents.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

// Façade « matériel de studio » : alu anthracite brossé, joues en noyer, potards, VU-mètres, afficheurs ambrés.
// Performance : tout ce qui ne bouge pas est rendu une fois dans une image (LayerCache, à la résolution de l'écran) ;
// seules les pièces mobiles (aiguilles, voyants, potards touchés, textes qui changent) sont redessinées, sur leur zone.
namespace ui
{
namespace colours
{
const juce::Colour background { 0xff17171a }, panel { 0xff1f1f22 }, field { 0xff121214 }, border { 0xff3a3936 },
    line { 0xff2c2b29 }, text { 0xfff2ead6 }, dim { 0xffa59e8c }, faint { 0xff6f6a5e }, accent { 0xffffb347 }, good { 0xff7dff9a },
    vfd { 0xffffc266 };
}

juce::Font font (float size, bool bold = false);
juce::Font vfdFont (float size);
/** An amber display window, recessed in the panel. */
void drawVfdWindow (juce::Graphics&, juce::Rectangle<float>);
/** Text in an amber display, with its glow. */
void drawVfdText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Justification, float alpha = 1.0f);

class Look final : public swell::analog::AnalogLookAndFeel
{
public:
    Look();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13, true); }
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return vfdFont (15); }
    juce::Font getPopupMenuFont() override { return font (14); }
};

/** Formats a knob value in its unit, in French. */
juce::String formatValue (const chain::KnobSpec&, float real);

/** The icon of a module, drawn as strokes on a 24 x 24 grid scaled into r. */
void drawModuleIcon (juce::Graphics&, chain::Type, juce::Rectangle<float> r, juce::Colour, float stroke = 2.0f);

enum class Glyph { power, solo, close, analyse };
void drawGlyph (juce::Graphics&, Glyph, juce::Rectangle<float> r, juce::Colour, float stroke = 2.0f);

/** A small amber triangle, as on the preset displays. */
class ArrowButton final : public juce::Button
{
public:
    ArrowButton (const juce::String& name, bool right) : juce::Button (name), pointsRight (right) { setTooltip (name); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    bool pointsRight;
};

/** A cream push key with a letter on its cap (A / B). */
class LetterKey final : public swell::analog::PushKey
{
public:
    explicit LetterKey (const juce::String& l) : letter (l) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::String letter;
};

/** A dark push button with a glass LED window (On, Solo). */
class LedKey final : public juce::Button
{
public:
    LedKey (const juce::String& name, juce::Colour led) : juce::Button (name), colour (led) { setTooltip (name); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour colour;
};

/** A small round machined button with a cross (remove). */
class RoundX final : public juce::Button
{
public:
    explicit RoundX (const juce::String& name) : juce::Button (name) { setTooltip (name); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

/** The big illuminated push button that starts the voice analysis. */
class LitButton final : public juce::Button
{
public:
    LitButton() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setLit (bool on) { if (on != lit) { lit = on; repaint(); } }
private:
    bool lit = true;
};
}

class ChainEditor;

/** One slot of the rack: a module plate, or a blank vented plate where a module can go. */
class SlotCard final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SlotCard (ChainEditor&, ChainProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    /** Called by the editor's timer: repaints only when something shown on the plate changed. */
    void refresh();
    const int index;

private:
    ChainEditor& editor;
    ChainProcessor& processor;
    ui::LedKey onButton { "On / Off", juce::Colour (0xff7dff9a) }, soloButton { juce::String::fromUTF8 ("Écouter seul"), juce::Colour (0xffffb347) };
    ui::RoundX removeButton { "Retirer" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    swell::analog::LayerCache plate, blank;
    bool dragging = false;
    juce::int64 shown = -1;
};

/** A module knob with its printed scale, its name and its value window, bound to one knob of the selected slot. */
class KnobView final : public juce::Component
{
public:
    KnobView (ChainProcessor&, int slot, int knob, const chain::KnobSpec&, juce::Colour, int knobSize);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<float> valueBox() const;
    ChainProcessor& processor;
    chain::KnobSpec spec;
    int slot, knob, size;
    juce::Colour colour;
    swell::analog::Knob slider;
    swell::analog::BatToggle toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachment;
    swell::analog::LayerCache scale;
};

/** The voice before the chain (grey) and after it (amber), on an amber phosphor screen. */
class SpectrumView final : public juce::Component
{
public:
    void update (ChainProcessor&);
    void paint (juce::Graphics&) override;

private:
    static constexpr int order = 11, size = 1 << order, bins = size / 2;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann };
    std::array<std::vector<float>, 2> history { std::vector<float> (size, 0.0f), std::vector<float> (size, 0.0f) };
    std::array<std::vector<float>, 2> levels { std::vector<float> (bins, -100.0f), std::vector<float> (bins, -100.0f) };
    std::vector<float> work = std::vector<float> (size * 2, 0.0f), incoming = std::vector<float> (16384, 0.0f);
    double rate = 48000;
    swell::analog::LayerCache screen, glass;
};

class PresetsPage final : public juce::Component
{
public:
    PresetsPage (ChainEditor&, ChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    struct Row { bool user; int index; };
    class List final : public juce::Component
    {
    public:
        explicit List (PresetsPage& p) : page (p) {}
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        PresetsPage& page;
        int hover = -1;
    };
    void setStyle (const juce::String&);
    std::vector<Row> visible() const;
    void saveCurrent();

    ChainEditor& editor;
    ChainProcessor& processor;
    UserLibrary library;
    juce::TextButton close { juce::String::fromUTF8 ("×") };
    juce::TextEditor nameField;
    juce::TextButton saveButton { juce::String::fromUTF8 ("Enregistrer ma chaîne") };
    juce::OwnedArray<juce::TextButton> styleButtons;
    juce::String style = "Tous";
    List list { *this };
    juce::Viewport viewport;
    friend class List;
};

class ModulesPage final : public juce::Component
{
public:
    ModulesPage (ChainEditor&, ChainProcessor&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void resized() override;
    int target = 0;

private:
    juce::Rectangle<int> tile (int i) const;
    ChainEditor& editor;
    ChainProcessor& processor;
    juce::TextButton close { juce::String::fromUTF8 ("×") };
    int hover = -1;
};

class ChainEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChainEditor (ChainProcessor&);
    ~ChainEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    void select (int slot);
    int selected() const { return selectedSlot; }
    void openPresets();
    void openModules (int slot);
    void closePages();
    void dragOver (int from, juce::Point<int> screenPos, bool finished);
    int dropTarget() const { return dropSlot; }
    int dragSource() const { return dragSlot; }
    /** Offline captures: needles straight to their reading, no animation. */
    void settleMeters();
    /** Offline captures: the Before / After screen instead of the knobs. */
    void showBeforeAfter (bool);
    /** Offline films: one timer tick, then the needles move by dt seconds. */
    void advanceMeters (double dt);

    // Layout of the façade (1280 x 800).
    static constexpr int W = 1280, H = 800;
    static juce::Rectangle<int> presetBox() { return { 436, 14, 412, 42 }; }
    static juce::Rectangle<int> findingsBox() { return { 504, 86, 722, 40 }; }
    static juce::Rectangle<int> rackWell() { return { 50, 148, 1158, 232 }; }
    static juce::Rectangle<int> cardBounds (int s) { return { 62 + s * 191, 156, 179, 216 }; }
    static juce::Rectangle<int> modulePanel() { return { 50, 410, 880, 334 }; }
    static juce::Rectangle<int> masterPanel() { return { 944, 410, 284, 334 }; }

private:
    void timerCallback() override;
    void rebuildDetail();
    void drawFacade (juce::Graphics&);
    void drawWalnut (juce::Graphics&, juce::Rectangle<float>, bool flip);
    juce::Rectangle<int> moduleHeader() const { return { 60, 416, 340, 48 }; }
    juce::Rectangle<int> masterValues() const { return { 952, 660, 270, 30 }; }
    juce::Rectangle<int> autoValueBox() const { return { 1140, 703, 76, 28 }; }

    ChainProcessor& processor;
    ui::Look look;
    juce::Image logo;
    swell::analog::LayerCache facade;

    ui::ArrowButton previous { juce::String::fromUTF8 ("Preset précédent"), false }, next { "Preset suivant", true };
    ui::LetterKey slotA { "A" }, slotB { "B" };
    swell::analog::BatToggle bypass;
    swell::analog::PilotLamp bypassLamp { juce::Colour (0xffe8321e) };
    ui::LitButton analyse;
    swell::analog::Knob voiceKnob { swell::analog::Knob::Style::ChickenHead };
    juce::OwnedArray<SlotCard> cards;
    juce::OwnedArray<KnobView> knobs;
    juce::ComboBox modulePreset;
    ui::ArrowButton presetPrev { juce::String::fromUTF8 ("Preset du module précédent"), false }, presetNext { "Preset du module suivant", true };
    ui::RoundX removeModule { "Retirer le module" };
    swell::analog::BatToggle viewToggle;
    SpectrumView spectrum;
    bool showSpectrum = false;
    swell::analog::VuMeter vuIn, vuOut;
    swell::analog::PilotLamp peakIn { juce::Colour (0xffe8321e) }, peakOut { juce::Colour (0xffe8321e) };
    swell::analog::Knob inputKnob { swell::analog::Knob::Style::FetSmall }, mixKnob { swell::analog::Knob::Style::FetSmall },
        gainKnob { swell::analog::Knob::Style::FetSmall };
    swell::analog::BatToggle autoGain;
    swell::analog::PilotLamp autoLamp { juce::Colour (0xff58f07d) };
    swell::LicenceBadge badge { juce::Colour (0xb0ece5d3), juce::Colour (0xffffc266), juce::Justification::centredRight };
    juce::TooltipWindow tooltips { this, 500 };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> voiceAttachment, inputAttachment, mixAttachment, gainAttachment;
    PresetsPage presets;
    ModulesPage modules;

    int selectedSlot = 0, dragSlot = -1, dropSlot = -1;
    chain::Type detailType = chain::Type::empty;
    int detailFor = -1;
    float progress = 0;
    int peakHoldIn = 0, peakHoldOut = 0;
    juce::String shownHeader, shownFindings, shownAuto;
    juce::StringArray findings;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainEditor)
};
