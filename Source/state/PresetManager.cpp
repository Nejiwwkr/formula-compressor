#include "PresetManager.h"

#include "../PluginProcessor.h"

namespace
{
const std::vector<PresetManager::BuiltInPreset>& builtIns()
{
    static const std::vector<PresetManager::BuiltInPreset> presets {
        { "Classic 4:1", "min(x, P1 + (x - P1) * P2)", { 0.25f, 0.25f, 0.0f, 0.0f }, 0 },
        { "Soft Knee", "x < P1 * 0.5 ? x : mix(x, P1 + (x - P1) * P2, "
                       "smoothstep(P1 * 0.5, P1, x))", { 0.25f, 0.25f, 0.0f, 0.0f }, 0 },
        { "Hard Limit", "min(x, P1)", { 0.35f, 0.25f, 0.0f, 0.0f }, 0 },
        { "Upward Compress", "x < P1 ? x / max(P2, 0.05) : x", { 0.10f, 0.5f, 0.0f, 0.0f }, 0 },
        { "Expander / Gate", "x < P1 ? x * max(P2, 0.05) : x", { 0.10f, 0.25f, 0.0f, 0.0f }, 0 },
        { "Tanh Comp", "tanh(x * (1 + P1 * 4)) / tanh(1 + P1 * 4)", { 0.25f, 0.0f, 0.0f, 0.0f }, 0 },
        { "Log Comp", "log(1 + x * P1 * 10) / log(1 + P1 * 10)", { 0.5f, 0.0f, 0.0f, 0.0f }, 0 },
        { "Two Stage", "min(min(x, P1 + (x - P1) * P2), P3 + (x - P3) * 0.1)",
                       { 0.25f, 0.25f, 0.55f, 0.0f }, 0 },
        { "Classic 4:1 (dB)", "min(x, -12 + (x + 12) / 4)",
                               { 0.25f, 0.25f, 0.0f, 0.0f }, 1 },
        { "Soft Knee (dB)", "x < -30 ? x : mix(x, -24 + (x + 24) / 4, "
                            "smoothstep(-30, -24, x))",
                            { 0.25f, 0.25f, 0.0f, 0.0f }, 1 },
        { "Upward Compress (dB)", "x < -36 ? x + 12 : x",
                                   { 0.25f, 0.25f, 0.0f, 0.0f }, 1 },
        { "Gate (dB)", "x < -40 ? -120 : x",
                        { 0.25f, 0.25f, 0.0f, 0.0f }, 1 }
    };

    return presets;
}
} // namespace

PresetManager::PresetManager (FormulaCompressorAudioProcessor& processorIn,
                              juce::File directoryIn)
    : processor (processorIn),
      directory (std::move (directoryIn))
{
    if (directory.getFullPathName().isEmpty())
        directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("FormulaCompressor")
                        .getChildFile ("Presets");
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;

    if (! directory.isDirectory())
        return names;

    for (const auto& entry : directory.findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (entry.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

juce::String PresetManager::sanitiseName (const juce::String& nameIn)
{
    auto name = juce::File::createLegalFileName (nameIn.trim());

    if (name.endsWithIgnoreCase (".xml"))
        name = name.dropLastCharacters (4);

    name = name.replace ("..", "_").trim();

    if (name.isEmpty() || name == "." || name == "_")
        name = "Preset";

    return name;
}

bool PresetManager::savePreset (const juce::String& nameIn)
{
    const auto name = sanitiseName (nameIn);

    if (name.isEmpty())
        return false;

    if (! directory.isDirectory() && directory.createDirectory().failed())
        return false;

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    return directory.getChildFile (name + ".xml")
        .replaceWithData (state.getData(), state.getSize());
}

bool PresetManager::loadPreset (const juce::String& nameIn)
{
    const auto name = sanitiseName (nameIn);
    const auto file = directory.getChildFile (name + ".xml");

    if (! file.existsAsFile())
        return false;

    juce::MemoryBlock state;

    if (! file.loadFileAsData (state))
        return false;

    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    return true;
}

void PresetManager::loadBuiltIn (int index)
{
    const auto& presets = builtIns();

    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    const auto& preset = presets[static_cast<std::size_t> (index)];

    processor.setFormulaDomain (preset.xDomain);
    processor.setFormula (preset.formula);

    for (int i = 0; i < 4; ++i)
        processor.setParameterValue ("p" + juce::String (i + 1), preset.p[i]);
}

const std::vector<PresetManager::BuiltInPreset>& PresetManager::getBuiltInPresets()
{
    return builtIns();
}
