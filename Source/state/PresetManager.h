#pragma once

#include <juce_core/juce_core.h>

#include <vector>

class FormulaCompressorAudioProcessor;

/** File based preset bank plus a small library of built-in formulas. */
class PresetManager
{
public:
    struct BuiltInPreset
    {
        juce::String name;
        juce::String formula;
        float p[4] = { 0.25f, 0.25f, 0.0f, 0.0f };
        int xDomain = 0; // 0 = linear amplitude, 1 = dBFS
    };

    explicit PresetManager (FormulaCompressorAudioProcessor&, juce::File directory = {});

    juce::StringArray getUserPresetNames() const;
    juce::File getDirectory() const { return directory; }

    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::String& name);
    void loadBuiltIn (int index);

    static const std::vector<BuiltInPreset>& getBuiltInPresets();
    static juce::String sanitiseName (const juce::String& name);

private:
    FormulaCompressorAudioProcessor& processor;
    juce::File directory;
};
