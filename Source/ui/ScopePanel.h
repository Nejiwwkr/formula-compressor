#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

class FormulaCompressorAudioProcessor;

/**
    Rolling time view on the same dBFS axis as the transfer plot.

    Input and output peak levels (formula gain only, no Mix / Makeup) and the
    gain curve share one plot: graphite is the common body, vermilion the
    removed input, teal the added output, and cobalt the gain reduction curve.
*/
class ScopePanel final : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    explicit ScopePanel (FormulaCompressorAudioProcessor&);
    ~ScopePanel() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Frame
    {
        float inputMin = 0.0f;
        float inputMax = 0.0f;
        float outputMin = 0.0f;
        float outputMax = 0.0f;
        float gainDb = 0.0f;
    };

    void timerCallback() override;
    void collectFrames();

    FormulaCompressorAudioProcessor& processorRef;
    std::vector<Frame> frames;
    std::vector<Frame> frozenFrames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopePanel)
};
