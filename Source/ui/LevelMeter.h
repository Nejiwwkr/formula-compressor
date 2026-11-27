#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FormulaCompressorAudioProcessor;

/** Three vertical meters: input, output and gain reduction. */
class LevelMeter final : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    explicit LevelMeter (FormulaCompressorAudioProcessor&);
    ~LevelMeter() override = default;

    void paint (juce::Graphics&) override;

private:
    struct Ballistics
    {
        float db = -100.0f;
        float holdDb = -100.0f;
        int holdFrames = 0;
    };

    void timerCallback() override;
    void update (Ballistics&, float targetDb, bool isReduction);

    static void drawBar (juce::Graphics&, juce::Rectangle<float>, float db,
                         float holdDb, bool fromUnity, juce::Colour fill);

    FormulaCompressorAudioProcessor& processorRef;
    Ballistics input;
    Ballistics output;
    Ballistics reduction;
    bool primed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};
