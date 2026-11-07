#pragma once
#include "ChainPresets.h"
#include "dsp/Analysis.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace ids
{
inline juce::String type (int s) { return "s" + juce::String (s) + "type"; }
inline juce::String on (int s) { return "s" + juce::String (s) + "on"; }
inline juce::String preset (int s) { return "s" + juce::String (s) + "preset"; }
inline juce::String knob (int s, int k) { return "s" + juce::String (s) + "k" + juce::String (k); }
inline const juce::String mix = "mix", output = "output", autoGain = "autoGain", bypass = "bypass", voiceType = "voiceType", solo = "solo";
}

class ChainProcessor final : public juce::AudioProcessor
{
public:
    ChainProcessor();
    ~ChainProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return state.getParameter (ids::bypass); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Swell Chain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8; }
    int getNumPrograms() override { return (int) chain::chainPresets().size(); }
    int getCurrentProgram() override { return currentChain; }
    void setCurrentProgram (int index) override { loadChain (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Reading the chain (any thread).
    chain::SlotState slot (int s) const;
    int currentChainIndex() const { return currentChain; }

    // Editing the chain (message thread).
    void loadChain (int index);
    void setSlotType (int s, chain::Type type);
    void setModulePreset (int s, int preset);
    void setKnob (int s, int k, float realValue);
    void moveSlot (int from, int to);
    void removeSlot (int s);
    void setSolo (int s);   // -1: none
    int soloSlot() const;

    // A/B: two complete chains.
    int activeSlotAB() const { return ab; }
    void switchAB (int which);

    // Analysis: records the dry voice at the chain input, analysed on a background thread.
    void startAnalysis();
    bool isRecording() const { return captureState.load() == 1; }
    bool isAnalysing() const { return captureState.load() == 1 || captureState.load() == 2; }
    float analysisProgress() const;
    void pollAnalysis();     // message thread
    bool isAnalysed() const { return analyzed.load(); }
    vm::AnalysisResult analysisResult() const;

    float takeInputPeak() { return engine.inputPeak.exchange (0); }
    float takeOutputPeak() { return engine.outputPeak.exchange (0); }
    float autoGainDb() const { return engine.autoGainDb(); }
    int latencySamples() const { return engine.latency(); }
    double currentRate() const { return rate.load(); }

    juce::AudioProcessorValueTreeState state;

private:
    void render (juce::AudioBuffer<float>&, bool forceBypass);
    chain::ChainSettings currentSettings() const;
    void writeSlot (int s, const chain::SlotState&);
    void setParam (const juce::String& id, float value);
    juce::ValueTree captureChain() const;
    void applyChain (const juce::ValueTree&);
    void applyResult (const vm::AnalysisResult&);

    chain::Engine engine;
    std::vector<float> monoRight = std::vector<float> (512, 0.0f);
    std::atomic<double> rate { 48000 };
    int currentChain = 0;

    std::vector<float> capture;
    std::atomic<int> captureState { 0 };   // 0 idle, 1 recording, 2 analysing, 3 done
    std::atomic<int> capturePosition { 0 };
    std::atomic<int> captureVoiceType { 1 };
    juce::ThreadPool pool { 1 };
    juce::SpinLock resultLock;
    vm::AnalysisResult pendingResult;

    std::atomic<bool> analyzed { false };
    std::array<std::atomic<float>, vm::numBands> analysisEq {};
    std::array<std::atomic<float>, vm::maxMarkers> markerHz {}, markerDepth {};
    std::atomic<int> markerCount { 0 };

    juce::ValueTree slotsAB[2];
    int ab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainProcessor)
};
