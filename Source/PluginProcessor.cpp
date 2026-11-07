#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ResizableEditor.h"

namespace
{
constexpr double captureSeconds = 12.0;

float toNormal (chain::Type t, int k, float real)
{
    const auto& specs = chain::info (t).knobs;
    if ((size_t) k >= specs.size()) return 0;
    const auto& s = specs[(size_t) k];
    return juce::jlimit (0.0f, 1.0f, (real - s.min) / (s.max - s.min));
}

float toReal (chain::Type t, int k, float normal)
{
    const auto& specs = chain::info (t).knobs;
    if ((size_t) k >= specs.size()) return 0;
    const auto& s = specs[(size_t) k];
    return s.min + normal * (s.max - s.min);
}
}

ChainProcessor::ChainProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SwellChain", layout())
{
    for (auto& v : analysisEq) v = 0;
    for (auto& v : markerHz) v = 1000;
    for (auto& v : markerDepth) v = 0;
    // Factory chain: built from the first preset so a new instance plays something useful.
    const auto slots = chain::slotsFor (chain::chainPresets()[0]);
    for (int s = 0; s < chain::slotCount; ++s) writeSlot (s, slots[(size_t) s]);
    slotsAB[0] = captureChain();
    slotsAB[1] = captureChain();
}

ChainProcessor::~ChainProcessor() { pool.removeAllJobs (true, 4000); }

juce::AudioProcessorValueTreeState::ParameterLayout ChainProcessor::layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray types;
    for (int t = 0; t < chain::typeCount; ++t) types.add (String::fromUTF8 (chain::info ((chain::Type) t).name));
    for (int s = 0; s < chain::slotCount; ++s)
    {
        const String n = "Slot " + String (s + 1) + " ";
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::type (s), 1 }, n + "Module", types, 0));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::on (s), 1 }, n + "On", true));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { ids::preset (s), 1 }, n + "Preset", 0, 31, 0));
        for (int k = 0; k < chain::maxKnobs; ++k)
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::knob (s, k), 1 }, n + "Knob " + String (k + 1),
                                                          NormalisableRange<float> (0, 1), 0.5f));
    }
    StringArray voices;
    for (const auto& v : vm::voiceTypes()) voices.add (String::fromUTF8 (v.name));
    StringArray solos { "Aucun" };
    for (int s = 0; s < chain::slotCount; ++s) solos.add (String (s + 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, 1 }, String::fromUTF8 ("Entrée"), NormalisableRange<float> (-12, 12, 0.5f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix", NormalisableRange<float> (0, 100, 1), 100.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 }, "Gain", NormalisableRange<float> (-12, 12, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::autoGain, 1 }, "Volume auto", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::voiceType, 1 }, "Type de voix", voices, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::solo, 1 }, "Solo", solos, 0));
    return l;
}

bool ChainProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

void ChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    monoRight.assign ((size_t) std::max (512, samplesPerBlock), 0.0f);
    setLatencySamples (engine.latency());
    // Allocated here, never on the audio thread, and never while the analysis reads it.
    if (captureState.load() == 1) captureState = 0;
    if (captureState.load() == 0 && capture.size() != size_t (sampleRate * captureSeconds))
        capture.assign (size_t (sampleRate * captureSeconds), 0.0f);
}

chain::SlotState ChainProcessor::slot (int s) const
{
    chain::SlotState st;
    auto raw = [this] (const juce::String& id) { return state.getRawParameterValue (id)->load(); };
    st.type = (chain::Type) juce::jlimit (0, chain::typeCount - 1, juce::roundToInt (raw (ids::type (s))));
    st.on = raw (ids::on (s)) > 0.5f;
    st.preset = juce::roundToInt (raw (ids::preset (s)));
    for (int k = 0; k < chain::maxKnobs; ++k) st.knobs[(size_t) k] = toReal (st.type, k, raw (ids::knob (s, k)));
    return st;
}

chain::ChainSettings ChainProcessor::currentSettings() const
{
    chain::ChainSettings c;
    auto raw = [this] (const juce::String& id) { return state.getRawParameterValue (id)->load(); };
    for (int s = 0; s < chain::slotCount; ++s) c.slots[(size_t) s] = slot (s);
    c.solo = juce::roundToInt (raw (ids::solo)) - 1;
    c.mix = raw (ids::mix) / 100;
    c.outputDb = raw (ids::output);
    c.inputDb = raw (ids::input);
    c.autoGain = raw (ids::autoGain) > 0.5f;
    c.bypass = raw (ids::bypass) > 0.5f;
    c.voiceType = juce::roundToInt (raw (ids::voiceType));
    c.analysis.analyzed = analyzed.load();
    c.analysis.markerCount = c.analysis.analyzed ? markerCount.load() : 0;
    for (int b = 0; b < vm::numBands; ++b) c.analysis.analysis[(size_t) b] = analysisEq[(size_t) b].load();
    for (int m = 0; m < vm::maxMarkers; ++m)
    {
        c.analysis.markerHz[(size_t) m] = markerHz[(size_t) m].load();
        c.analysis.markerDepth[(size_t) m] = markerDepth[(size_t) m].load();
        c.analysis.markerOn[(size_t) m] = true;
    }
    return c;
}

void ChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { render (buffer, false); }
void ChainProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { render (buffer, true); }

void ChainProcessor::render (juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() == 0) return;
    const bool mono = getTotalNumInputChannels() == 1 || buffer.getNumChannels() == 1;
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (mono && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    auto settings = currentSettings();
    settings.bypass = settings.bypass || forceBypass;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto bpm = pos->getBpm())
                if (std::isfinite (*bpm) && *bpm > 0) settings.bpm = *bpm;

    // The dry voice for the analysis, before the chain.
    if (captureState.load() == 1 && ! capture.empty())
    {
        float energy = 0;
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (l[i] + r[i]); energy += m * m; }
        if (energy / float (n) > 1.0e-7f)
        {
            int pos = capturePosition.load();
            for (int i = 0; i < n && pos < int (capture.size()); ++i, ++pos) capture[(size_t) pos] = 0.5f * (l[i] + r[i]);
            capturePosition = pos;
            if (pos >= int (capture.size()))
            {
                captureState = 2;
                const int voice = captureVoiceType.load();
                pool.addJob ([this, voice]
                {
                    const auto result = vm::analyse (capture, rate.load(), voice);
                    const auto stats = chain::measureVoice (capture, rate.load());
                    {
                        const juce::SpinLock::ScopedLockType lock (resultLock);
                        pendingResult = result;
                        pendingStats = stats;
                    }
                    captureState = 3;
                });
            }
        }
    }

    pushScope (0, buffer.getReadPointer (0), buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0), n);
    if (buffer.getNumChannels() > 1)
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, settings);
    else
    {
        // A mono bus: process a stereo copy and fold it back.
        for (int start = 0; start < n; start += (int) monoRight.size())
        {
            const int count = std::min ((int) monoRight.size(), n - start);
            float* left = buffer.getWritePointer (0, start);
            std::copy (left, left + count, monoRight.begin());
            engine.process (left, monoRight.data(), count, settings);
            for (int i = 0; i < count; ++i) left[i] = 0.5f * (left[i] + monoRight[(size_t) i]);
        }
    }
    pushScope (1, buffer.getReadPointer (0), buffer.getReadPointer (buffer.getNumChannels() > 1 ? 1 : 0), n);
}

void ChainProcessor::pushScope (int which, const float* l, const float* r, int n)
{
    auto& fifo = scopeFifo[which];
    auto& data = scopeData[which];
    int pre1, size1, pre2, size2;
    fifo.prepareToWrite (n, pre1, size1, pre2, size2);
    for (int i = 0; i < size1; ++i) data[(size_t) (pre1 + i)] = 0.5f * (l[i] + r[i]);
    for (int i = 0; i < size2; ++i) data[(size_t) (pre2 + i)] = 0.5f * (l[size1 + i] + r[size1 + i]);
    fifo.finishedWrite (size1 + size2);
}

int ChainProcessor::readScope (bool processed, float* dest, int n)
{
    auto& fifo = scopeFifo[processed ? 1 : 0];
    auto& data = scopeData[processed ? 1 : 0];
    const int ready = fifo.getNumReady();
    if (ready > n)
    {
        int a, b, c, d;
        fifo.prepareToRead (ready - n, a, b, c, d);
        fifo.finishedRead (b + d);
    }
    int pre1, size1, pre2, size2;
    fifo.prepareToRead (std::min (n, fifo.getNumReady()), pre1, size1, pre2, size2);
    for (int i = 0; i < size1; ++i) dest[i] = data[(size_t) (pre1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = data[(size_t) (pre2 + i)];
    fifo.finishedRead (size1 + size2);
    return size1 + size2;
}

void ChainProcessor::loadUserChain (const juce::ValueTree& t)
{
    applyChain (t);
    userName = t.getProperty ("name").toString();
    setSolo (-1);
    applyVoice();
}

//==============================================================================
void ChainProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void ChainProcessor::writeSlot (int s, const chain::SlotState& st)
{
    setParam (ids::type (s), (float) (int) st.type);
    setParam (ids::on (s), st.on ? 1.0f : 0.0f);
    setParam (ids::preset (s), (float) st.preset);
    for (int k = 0; k < chain::maxKnobs; ++k)
    {
        auto* p = state.getParameter (ids::knob (s, k));
        p->beginChangeGesture();
        p->setValueNotifyingHost (toNormal (st.type, k, st.knobs[(size_t) k]));
        p->endChangeGesture();
    }
}

void ChainProcessor::loadChain (int index)
{
    const auto& list = chain::chainPresets();
    currentChain = juce::jlimit (0, (int) list.size() - 1, index);
    userName.clear();
    const auto slots = chain::slotsFor (list[(size_t) currentChain]);
    for (int s = 0; s < chain::slotCount; ++s) writeSlot (s, slots[(size_t) s]);
    setSolo (-1);
    applyVoice();
}

const juce::String ChainProcessor::getProgramName (int index)
{
    const auto& list = chain::chainPresets();
    return juce::String::fromUTF8 (list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)].name);
}

void ChainProcessor::setSlotType (int s, chain::Type type)
{
    chain::SlotState st;
    st.type = type;
    st.on = true;
    st.preset = 0;
    st.knobs = chain::presetKnobs (type, 0);
    writeSlot (s, st);
    if (voiceValid.load() && (type == chain::Type::vox || type == chain::Type::match))
    {
        const auto v = voiceSettings();
        if (type == chain::Type::vox) { setKnob (s, 1, v.compress); setKnob (s, 2, v.deEss); }
        else setKnob (s, 2, v.deEss);
    }
}

void ChainProcessor::setModulePreset (int s, int preset)
{
    auto st = slot (s);
    st.preset = preset;
    st.knobs = chain::presetKnobs (st.type, preset);
    writeSlot (s, st);
}

void ChainProcessor::setKnob (int s, int k, float realValue)
{
    const auto st = slot (s);
    auto* p = state.getParameter (ids::knob (s, k));
    p->beginChangeGesture();
    p->setValueNotifyingHost (toNormal (st.type, k, realValue));
    p->endChangeGesture();
}

void ChainProcessor::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= chain::slotCount || to >= chain::slotCount) return;
    std::array<chain::SlotState, chain::slotCount> slots;
    for (int s = 0; s < chain::slotCount; ++s) slots[(size_t) s] = slot (s);
    const auto moved = slots[(size_t) from];
    if (from < to) std::rotate (slots.begin() + from, slots.begin() + from + 1, slots.begin() + to + 1);
    else std::rotate (slots.begin() + to, slots.begin() + from, slots.begin() + from + 1);
    slots[(size_t) to] = moved;
    for (int s = 0; s < chain::slotCount; ++s) writeSlot (s, slots[(size_t) s]);
    setSolo (-1);
}

void ChainProcessor::removeSlot (int s)
{
    std::array<chain::SlotState, chain::slotCount> slots;
    for (int i = 0; i < chain::slotCount; ++i) slots[(size_t) i] = slot (i);
    for (int i = s; i < chain::slotCount - 1; ++i) slots[(size_t) i] = slots[(size_t) i + 1];
    slots[chain::slotCount - 1] = {};
    for (int i = 0; i < chain::slotCount; ++i) writeSlot (i, slots[(size_t) i]);
    setSolo (-1);
}

void ChainProcessor::setSolo (int s) { setParam (ids::solo, (float) (s + 1)); }
int ChainProcessor::soloSlot() const { return juce::roundToInt (state.getRawParameterValue (ids::solo)->load()) - 1; }

juce::ValueTree ChainProcessor::captureChain() const
{
    juce::ValueTree t ("Chain");
    t.setProperty ("preset", currentChain, nullptr);
    for (int s = 0; s < chain::slotCount; ++s)
    {
        const auto st = slot (s);
        juce::ValueTree c ("Slot");
        c.setProperty ("type", (int) st.type, nullptr);
        c.setProperty ("on", st.on, nullptr);
        c.setProperty ("preset", st.preset, nullptr);
        for (int k = 0; k < chain::maxKnobs; ++k) c.setProperty ("k" + juce::String (k), st.knobs[(size_t) k], nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void ChainProcessor::applyChain (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    currentChain = (int) t.getProperty ("preset", currentChain);
    for (int s = 0; s < chain::slotCount && s < t.getNumChildren(); ++s)
    {
        const auto c = t.getChild (s);
        chain::SlotState st;
        st.type = (chain::Type) juce::jlimit (0, chain::typeCount - 1, (int) c.getProperty ("type", 0));
        st.on = (bool) c.getProperty ("on", true);
        st.preset = (int) c.getProperty ("preset", 0);
        for (int k = 0; k < chain::maxKnobs; ++k) st.knobs[(size_t) k] = (float) c.getProperty ("k" + juce::String (k), 0.0f);
        writeSlot (s, st);
    }
}

void ChainProcessor::switchAB (int which)
{
    if (which == ab) return;
    slotsAB[ab] = captureChain();
    ab = which;
    applyChain (slotsAB[ab]);
}

//==============================================================================
void ChainProcessor::startAnalysis()
{
    if (isAnalysing() || capture.empty()) return;
    captureVoiceType = juce::roundToInt (state.getRawParameterValue (ids::voiceType)->load());
    capturePosition = 0;
    captureState = 1;
}

float ChainProcessor::analysisProgress() const
{
    if (capture.empty()) return 0;
    return captureState.load() >= 2 ? 1.0f : float (capturePosition.load()) / float (capture.size());
}

void ChainProcessor::pollAnalysis()
{
    if (captureState.load() != 3) return;
    vm::AnalysisResult result;
    chain::VoiceStats stats;
    {
        const juce::SpinLock::ScopedLockType lock (resultLock);
        result = pendingResult;
        stats = pendingStats;
    }
    captureState = 0;
    if (result.valid) applyResult (result, stats);
}

void ChainProcessor::applyResult (const vm::AnalysisResult& r, const chain::VoiceStats& v)
{
    voiceValid = v.valid;
    voiceLevel = v.levelDb; voiceDynamics = v.dynamicsDb; voiceSibilance = v.sibilanceDb;
    for (int b = 0; b < vm::numBands; ++b) analysisEq[(size_t) b] = r.eq[(size_t) b];
    for (int m = 0; m < vm::maxMarkers; ++m)
    {
        markerHz[(size_t) m] = m < r.markerCount ? r.markerHz[(size_t) m] : 1000.0f;
        markerDepth[(size_t) m] = m < r.markerCount ? r.markerDepth[(size_t) m] : 0.0f;
    }
    markerCount = r.markerCount;
    analyzed = true;
    applyVoice();
}

chain::VoiceStats ChainProcessor::voiceStats() const
{
    chain::VoiceStats v;
    v.valid = voiceValid.load();
    v.levelDb = voiceLevel.load(); v.dynamicsDb = voiceDynamics.load(); v.sibilanceDb = voiceSibilance.load();
    return v;
}

void ChainProcessor::applyVoice()
{
    if (! voiceValid.load()) return;
    const auto v = voiceSettings();
    setParam (ids::input, v.inputDb);
    for (int s = 0; s < chain::slotCount; ++s)
    {
        const auto st = slot (s);
        if (st.type == chain::Type::vox) { setKnob (s, 1, v.compress); setKnob (s, 2, v.deEss); }
        if (st.type == chain::Type::match) setKnob (s, 2, v.deEss);
    }
}

vm::AnalysisResult ChainProcessor::analysisResult() const
{
    vm::AnalysisResult r;
    r.valid = analyzed.load();
    for (int b = 0; b < vm::numBands; ++b) r.eq[(size_t) b] = analysisEq[(size_t) b].load();
    r.markerCount = markerCount.load();
    for (int m = 0; m < vm::maxMarkers; ++m)
    {
        r.markerHz[(size_t) m] = markerHz[(size_t) m].load();
        r.markerDepth[(size_t) m] = markerDepth[(size_t) m].load();
    }
    return r;
}

//==============================================================================
void ChainProcessor::getStateInformation (juce::MemoryBlock& out)
{
    auto t = state.copyState();
    t.setProperty ("chainPreset", currentChain, nullptr);
    t.setProperty ("userChain", userName, nullptr);
    t.setProperty ("ab", ab, nullptr);
    t.setProperty ("analyzed", analyzed.load(), nullptr);
    juce::String eq, markers;
    for (int b = 0; b < vm::numBands; ++b) eq << analysisEq[(size_t) b].load() << " ";
    for (int m = 0; m < markerCount.load(); ++m) markers << markerHz[(size_t) m].load() << ":" << markerDepth[(size_t) m].load() << " ";
    t.setProperty ("analysisEq", eq.trim(), nullptr);
    t.setProperty ("markers", markers.trim(), nullptr);
    t.setProperty ("voice", voiceValid.load() ? juce::String (voiceLevel.load()) + " " + juce::String (voiceDynamics.load()) + " " + juce::String (voiceSibilance.load()) : juce::String(), nullptr);
    for (auto* old : { "SlotA", "SlotB" }) t.removeChild (t.getChildWithName (old), nullptr);
    slotsAB[ab] = captureChain();
    auto a = slotsAB[0].createCopy(); a.setProperty ("name", "A", nullptr);
    auto b = slotsAB[1].createCopy(); b.setProperty ("name", "B", nullptr);
    juce::ValueTree holderA ("SlotA"); holderA.appendChild (a, nullptr);
    juce::ValueTree holderB ("SlotB"); holderB.appendChild (b, nullptr);
    t.appendChild (holderA, nullptr);
    t.appendChild (holderB, nullptr);
    if (auto xml = t.createXml()) copyXmlToBinary (*xml, out);
}

void ChainProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (state.state.getType())) return;
    auto t = juce::ValueTree::fromXml (*xml);
    const auto holderA = t.getChildWithName ("SlotA"), holderB = t.getChildWithName ("SlotB");
    if (holderA.isValid() && holderA.getNumChildren() > 0) slotsAB[0] = holderA.getChild (0).createCopy();
    if (holderB.isValid() && holderB.getNumChildren() > 0) slotsAB[1] = holderB.getChild (0).createCopy();
    currentChain = (int) t.getProperty ("chainPreset", 0);
    userName = t.getProperty ("userChain").toString();
    ab = juce::jlimit (0, 1, (int) t.getProperty ("ab", 0));
    analyzed = (bool) t.getProperty ("analyzed", false);
    auto eq = juce::StringArray::fromTokens (t.getProperty ("analysisEq").toString(), " ", "");
    for (int b = 0; b < vm::numBands && b < eq.size(); ++b) analysisEq[(size_t) b] = eq[b].getFloatValue();
    auto markers = juce::StringArray::fromTokens (t.getProperty ("markers").toString(), " ", "");
    markerCount = std::min (markers.size(), vm::maxMarkers);
    for (int m = 0; m < markerCount.load(); ++m)
    {
        markerHz[(size_t) m] = markers[m].upToFirstOccurrenceOf (":", false, false).getFloatValue();
        markerDepth[(size_t) m] = markers[m].fromFirstOccurrenceOf (":", false, false).getFloatValue();
    }
    auto voice = juce::StringArray::fromTokens (t.getProperty ("voice").toString(), " ", "");
    voiceValid = voice.size() == 3;
    if (voice.size() == 3) { voiceLevel = voice[0].getFloatValue(); voiceDynamics = voice[1].getFloatValue(); voiceSibilance = voice[2].getFloatValue(); }
    t.removeChild (holderA, nullptr);
    t.removeChild (holderB, nullptr);
    state.replaceState (t);
}

juce::AudioProcessorEditor* ChainProcessor::createEditor() { return new ResizableEditor (*this, new ChainEditor (*this)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChainProcessor(); }
