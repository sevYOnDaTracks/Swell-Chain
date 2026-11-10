// Film de l'interface, image par image, pour la vidéo de la boutique (projet Chain-Motion, assets/ui.mp4).
// Usage : ChainFilm <voix.wav> [secondes] | ffmpeg -f rawvideo -pix_fmt bgra -s 2560x1600 -r 30 -i - ...
// Les images (BGRA, 2560 x 1600, 30 i/s) sortent sur stdout. Le plugin traite la voix en continu,
// et les actions suivent le montage de la vidéo : analyse à 7,5 s (accélérée x4 pendant 3 s),
// Avant / Après de 14 à 17,3 s, chaînes R&B Silk, Rage Scream, Afro Bounce, Trap Ad-libs, puis la bibliothèque à 24 s.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

namespace
{
int chainIndex (const char* name)
{
    const auto& all = chain::chainPresets();
    for (int i = 0; i < (int) all.size(); ++i) if (juce::String (all[(size_t) i].name) == name) return i;
    return 0;
}

int slotOf (ChainProcessor& p, chain::Type t)
{
    for (int s = 0; s < chain::slotCount; ++s) if (p.slot (s).type == t) return s;
    return 0;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::fprintf (stderr, "usage: ChainFilm voix.wav [secondes]\n"); return 1; }
    const double seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 30.0;
    swell::Licence::unlockForTesting();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (juce::File (juce::String::fromUTF8 (argv[1]))), true));
    if (reader == nullptr) { std::fprintf (stderr, "voix illisible\n"); return 1; }
    juce::AudioBuffer<float> voice (2, (int) reader->lengthInSamples);
    reader->read (&voice, 0, voice.getNumSamples(), 0, true, true);
    const double rate = reader->sampleRate;

    ChainProcessor p;
    p.setCurrentProgram (0);   // Trap Lead
    constexpr int fps = 30, block = 400;
    const int perFrame = (int) std::lround (rate / fps);
    p.prepareToPlay (rate, block);
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditorIfNeeded());
    ChainEditor* ed = nullptr;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        if (auto* ce = dynamic_cast<ChainEditor*> (&c)) ed = ce;
        for (auto* child : c.getChildren())
        {
            if (dynamic_cast<swell::LicenceBadge*> (child) != nullptr || dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr)
                child->setVisible (false);
            walk (*child);
        }
    };
    walk (*e);
    if (ed == nullptr) return 1;
    ed->select (slotOf (p, chain::Type::space));

    juce::AudioBuffer<float> b (2, block);
    juce::MidiBuffer midi;
    int readPos = 0;
    auto feed = [&] (int samples)
    {
        for (int done = 0; done < samples; done += block)
        {
            const int n = std::min (block, samples - done);
            b.setSize (2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    b.setSample (ch, i, voice.getSample (std::min (ch, voice.getNumChannels() - 1), (readPos + i) % voice.getNumSamples()));
            readPos = (readPos + n) % voice.getNumSamples();
            p.processBlock (b, midi);
        }
    };

    struct Cue { double t; std::function<void()> act; bool done = false; };
    std::vector<Cue> cues {
        { 7.5, [&] { p.startAnalysis(); } },
        { 14.0, [&] { ed->showBeforeAfter (true); } },
        { 17.3, [&] { ed->showBeforeAfter (false); } },
        { 17.6, [&] { p.loadChain (chainIndex ("R&B Silk")); ed->select (slotOf (p, chain::Type::adlib)); } },
        { 19.2, [&] { p.loadChain (chainIndex ("Rage Scream")); ed->select (slotOf (p, chain::Type::knock)); } },
        { 20.8, [&] { p.loadChain (chainIndex ("Afro Bounce")); ed->select (slotOf (p, chain::Type::adlib)); } },
        { 22.4, [&] { p.loadChain (chainIndex ("Trap Ad-libs")); ed->select (slotOf (p, chain::Type::adlib)); } },
        { 24.0, [&] { ed->openPresets(); } },
    };

    const int frames = (int) std::lround (seconds * fps);
    for (int f = 0; f < frames; ++f)
    {
        const double t = (double) f / fps;
        for (auto& c : cues) if (! c.done && t >= c.t) { c.act(); c.done = true; }
        // While the voice is recorded for the analysis, time runs four times faster (12 s shown in 3 s).
        feed (p.isRecording() ? perFrame * 4 : perFrame);
        if (! p.isRecording() && p.isAnalysing())
            for (int wait = 0; wait < 200 && p.isAnalysing(); ++wait) { p.pollAnalysis(); juce::Thread::sleep (5); }
        ed->advanceMeters (1.0 / fps);
        auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 2.0f).convertedToFormat (juce::Image::ARGB);
        juce::Image::BitmapData d (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < img.getHeight(); ++y) std::fwrite (d.getLinePointer (y), 4, (size_t) img.getWidth(), stdout);
        if (f % 60 == 0) std::fprintf (stderr, "%.0f s\n", t);
    }
    std::fflush (stdout);
    p.editorBeingDeleted (e.get());
    e.reset();
    return 0;
}
