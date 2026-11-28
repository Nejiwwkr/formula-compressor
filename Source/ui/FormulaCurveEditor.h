#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../dsp/Expression.h"

#include <array>
#include <memory>
#include <vector>

class FormulaCompressorAudioProcessor;

/**
    Draws f(x) on equal-scale axes in three views:

      LEVEL   input / output level in dB
      GAIN    gain = f(x,s)/x in dB, at the current s
      LINEAR  linear amplitude (ordinary axes)

    Area fills are teal for boost and vermilion for reduction. Negative output
    (polarity inversion) is drawn as a dashed cobalt curve. The x domain can be
    linear amplitude (AMP) or dBFS, matching the formula editor's x switch.
*/
class FormulaCurveEditor final : public juce::Component,
                                 public juce::SettableTooltipClient,
                                 private juce::Timer,
                                 private juce::ChangeListener
{
public:
    enum class ViewMode { Level = 0, Gain = 1, Linear = 2 };

    explicit FormulaCurveEditor (FormulaCompressorAudioProcessor&);
    ~FormulaCurveEditor() override;

    void setMode (ViewMode newMode);
    ViewMode getMode() const noexcept { return mode; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr float levelMinDb = -60.0f;
    static constexpr float levelMaxDb = 12.0f;
    static constexpr float linearMin = 0.0f;
    static constexpr float linearMax = 1.5f;

private:
    struct Sample
    {
        float inputLinear = 0.0f;
        float inputDb = 0.0f;
        float outputLinear = 0.0f;
        float outputDb = 0.0f;
        float gainLinear = 0.0f;
        float gainDb = 0.0f;
        bool valid = false;
        bool inverted = false;
        bool boost = false;
    };

    struct CurveValue
    {
        float inputLinear = 0.0f;
        float inputDb = 0.0f;
        float outputLinear = 0.0f;
        float outputDb = 0.0f;
        float gainLinear = 0.0f;
        float gainDb = 0.0f;
        bool valid = false;
        bool inverted = false;
        bool boost = false;
    };

    struct Layout
    {
        juce::Rectangle<float> header;
        juce::Rectangle<float> plot;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    Layout computeLayout() const;
    void rebuildCurve();

    CurveValue makeValue (float inputLinear, float inputDb,
                          const std::array<float, 4>& sidechainValues,
                          float sidechainTotal) const;

    juce::Point<float> toScreen (float inputValue, float outputValue,
                                 juce::Rectangle<float> plot) const;
    float screenToInputValue (float x, juce::Rectangle<float> plot) const;

    float inputValueFor (const Sample&) const;
    float outputValueFor (const Sample&) const;
    float unityValueFor (float inputValue) const;

    void drawGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
    juce::String getModeTitle() const;

    FormulaCompressorAudioProcessor& processorRef;
    ViewMode mode = ViewMode::Level;
    int formulaDomain = 0;

    std::array<juce::TextButton, 3> modeButtons;

    std::shared_ptr<const formula::Program> program;
    std::array<float, 4> pValues { 0.25f, 0.25f, 0.0f, 0.0f };

    std::vector<Sample> samples;
    juce::Path curvePath;
    juce::Path invertedPath;
    juce::Path boostFill;
    juce::Path reduceFill;
    std::vector<float> invalidInputs;
    bool hasCurve = false;
    bool hasInverted = false;

    juce::Point<float> hoverPosition { -1.0f, -1.0f };
    bool hovering = false;
    float liveInputLinear = 0.0f;
    std::array<float, 4> sidechainValues { 0.0f, 0.0f, 0.0f, 0.0f };
    float sidechainTotal = 0.0f;
    bool sidechainActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormulaCurveEditor)
};
