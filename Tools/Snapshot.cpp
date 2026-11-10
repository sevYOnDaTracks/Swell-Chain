// Rendu hors ligne de l'interface en PNG : ChainSnapshot <sortie.png> [échelle] [emplacement] [locked|spectre|presets|modules]
// « locked » montre l'écran d'activation au lieu de débloquer la licence ; « spectre » montre l'écran Avant / Après ;
// « presets » et « modules » ouvrent ces pages.
// Le processeur reçoit d'abord une voix synthétique pour que les aiguilles indiquent une vraie mesure.
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool locked = argc > 4 && juce::String (argv[4]) == "locked";
    if (! locked) swell::Licence::unlockForTesting();   // rendu hors ligne, sans fichier de licence
    ChainProcessor p;
    p.setCurrentProgram (0);   // « Trap Lead »
    p.prepareToPlay (48000, 256);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> b (2, 256);
    juce::Random rng (3);
    double phase = 0;
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditorIfNeeded());
    ChainEditor* chainEditor = nullptr;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        if (auto* ce = dynamic_cast<ChainEditor*> (&c)) chainEditor = ce;
        for (auto* child : c.getChildren()) find (*child);
    };
    find (*e);
    if (chainEditor != nullptr && argc > 3) chainEditor->select (juce::String (argv[3]).getIntValue());
    if (chainEditor != nullptr && argc > 4 && juce::String (argv[4]) == "spectre") chainEditor->showBeforeAfter (true);
    if (chainEditor != nullptr && argc > 4 && juce::String (argv[4]) == "presets") chainEditor->openPresets();
    if (chainEditor != nullptr && argc > 4 && juce::String (argv[4]) == "modules") chainEditor->openModules (4);
    for (int blockIndex = 0; blockIndex < 600; ++blockIndex)
    {
        for (int n = 0; n < 256; ++n)
        {
            const double t = (blockIndex * 256 + n) / 48000.0;
            phase += 2 * juce::MathConstants<double>::pi * (190 + 25 * std::sin (t * 4.0)) / 48000.0;
            double s = 0;
            for (int h = 1; h <= 8; ++h) s += std::sin (phase * h) / h;
            const double syllable = std::max (0.0, std::sin (2 * juce::MathConstants<double>::pi * 2.5 * t));
            const float v = (float) (0.22 * s * syllable) + 0.01f * (rng.nextFloat() - 0.5f);
            b.setSample (0, n, v);
            b.setSample (1, n, v);
        }
        p.processBlock (b, midi);
        // Read the meters in the middle of a syllable (the test voice has gaps between syllables).
        const double phrase = std::fmod ((blockIndex + 1) * 256 / 48000.0, 0.4);
        if (chainEditor != nullptr && blockIndex % 6 == 0 && phrase > 0.06 && phrase < 0.18) chainEditor->settleMeters();
    }
    // Capture propre : sans le badge de licence (vide hors ligne) ni le coin de redimensionnement.
    std::function<void (juce::Component&)> tidy = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! locked && (dynamic_cast<swell::LicenceBadge*> (child) != nullptr || dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr))
                child->setVisible (false);
            tidy (*child);
        }
    };
    tidy (*e);
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;
    auto image = e->createComponentSnapshot (e->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (image, s);
    std::printf ("%s\n", out.getFullPathName().toRawUTF8());
    p.editorBeingDeleted (e.get());
    e.reset();
    return 0;
}
