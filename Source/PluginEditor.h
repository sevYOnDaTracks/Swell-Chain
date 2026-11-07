#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
namespace colours
{
const juce::Colour background { 0xff121419 }, panel { 0xff181b21 }, field { 0xff1b1e25 }, border { 0xff2c313b },
    line { 0xff262a33 }, text { 0xffeef1f6 }, dim { 0xff9aa2b1 }, faint { 0xff6f7786 }, accent { 0xffff9a4a }, good { 0xff8fe3b0 };
}

juce::Font font (float size, bool bold = false);

class Look final : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13, true); }
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13, true); }
    juce::Font getPopupMenuFont() override { return font (14); }
};

/** Formats a knob value in its unit, in French. */
juce::String formatValue (const chain::KnobSpec&, float real);
}

class ChainEditor;

/** One slot of the chain: a module card, or an empty place to add one. */
class SlotCard final : public juce::Component
{
public:
    SlotCard (ChainEditor&, ChainProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    const int index;

private:
    ChainEditor& editor;
    ChainProcessor& processor;
    juce::TextButton onButton, soloButton, removeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    bool dragging = false;
};

/** A labelled knob bound to one knob parameter of the selected slot. */
class KnobView final : public juce::Component
{
public:
    KnobView (ChainProcessor&, int slot, int knob, const chain::KnobSpec&, juce::Colour);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    ChainProcessor& processor;
    chain::KnobSpec spec;
    int slot, knob;
    juce::Slider slider;
    juce::TextButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PresetsPage final : public juce::Component
{
public:
    PresetsPage (ChainEditor&, ChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
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
    std::vector<int> visible() const;

    ChainEditor& editor;
    ChainProcessor& processor;
    juce::TextButton close { juce::String::fromUTF8 ("×") };
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

private:
    void timerCallback() override;
    void rebuildDetail();
    juce::Rectangle<int> presetBox() const { return { 400, 18, 380, 40 }; }

    ChainProcessor& processor;
    ui::Look look;
    juce::Image logo;
    juce::TextButton previous { juce::String::fromUTF8 ("‹") }, next { juce::String::fromUTF8 ("›") };
    juce::TextButton slotA { "A" }, slotB { "B" }, bypass { "Bypass" };
    juce::TextButton analyse { "Analyser ma voix" };
    juce::TextButton voice[3];
    juce::OwnedArray<SlotCard> cards;
    juce::OwnedArray<KnobView> knobs;
    juce::ComboBox modulePreset;
    juce::TextButton removeModule { "Retirer" };
    juce::TextButton autoGain { "Volume auto" };
    juce::Slider mix, gain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment, gainAttachment;
    PresetsPage presets;
    ModulesPage modules;

    int selectedSlot = 0, dragSlot = -1, dropSlot = -1;
    chain::Type detailType = chain::Type::empty;
    int detailFor = -1;
    float inLevel = 0, outLevel = 0, progress = 0;
    juce::String findings;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainEditor)
};
