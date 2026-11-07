#pragma once
// The user's own chains and favourite presets, kept for every project in
// Application Support/Swell/Chain (chains.xml, favoris.txt).
#include <juce_data_structures/juce_data_structures.h>

class UserLibrary
{
public:
    UserLibrary() { load(); }

    static juce::File folder()
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile ("Swell").getChildFile ("Chain");
    }

    void load()
    {
        favourites.clear();
        favourites.addLines (folder().getChildFile ("favoris.txt").loadFileAsString());
        favourites.removeEmptyStrings();
        chains = juce::ValueTree ("Chains");
        if (auto xml = juce::XmlDocument::parse (folder().getChildFile ("chains.xml")))
            chains = juce::ValueTree::fromXml (*xml);
        if (! chains.hasType ("Chains")) chains = juce::ValueTree ("Chains");
    }

    bool isFavourite (const juce::String& preset) const { return favourites.contains (preset); }
    void toggleFavourite (const juce::String& preset)
    {
        if (isFavourite (preset)) favourites.removeString (preset); else favourites.add (preset);
        folder().createDirectory();
        folder().getChildFile ("favoris.txt").replaceWithText (favourites.joinIntoString ("\n"));
    }

    int chainCount() const { return chains.getNumChildren(); }
    juce::ValueTree chain (int i) const { return chains.getChild (i); }

    /** Saves a chain under a name; a chain with the same name is replaced. */
    void saveChain (const juce::String& name, const juce::ValueTree& chainTree)
    {
        auto copy = chainTree.createCopy();
        copy.setProperty ("name", name, nullptr);
        for (int i = chains.getNumChildren(); --i >= 0;)
            if (chains.getChild (i).getProperty ("name").toString() == name) chains.removeChild (i, nullptr);
        chains.appendChild (copy, nullptr);
        write();
    }

    void removeChain (int i) { chains.removeChild (i, nullptr); write(); }

private:
    void write()
    {
        folder().createDirectory();
        if (auto xml = chains.createXml()) xml->writeTo (folder().getChildFile ("chains.xml"));
    }

    juce::StringArray favourites;
    juce::ValueTree chains { "Chains" };
};
