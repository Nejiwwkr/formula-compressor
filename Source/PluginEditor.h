#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "state/PresetManager.h"
#include "ui/FormulaCurveEditor.h"
#include "ui/FormulaLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/ScopePanel.h"

#include <array>
#include <memory>
#include <vector>

class FormulaCompressorAudioProcessor;

class FormulaCompressorAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer,
                                                    private juce::ChangeListener
{
public:
    explicit FormulaCompressorAudioProcessorEditor (FormulaCompressorAudioProcessor&);
    ~FormulaCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void scheduleFormulaApply();
    void applyFormula();
    void refreshFormulaStatus();
    void refreshDomainButtons();
    void refreshDetectorButtons();
    void refreshPresetList();
    void presetChanged();
    void savePresetDialog();
    void setupKnob (int index, const juce::String& caption,
                    const juce::String& parameterID, const juce::String& tooltip);
    static void styleButton (juce::TextButton&);

    FormulaCompressorAudioProcessor& processorRef;
    FormulaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "SAVE" };
    juce::ToggleButton bypassToggle;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    juce::Label formulaLabel;
    juce::TextEditor formulaEditor;
    juce::Label statusLabel;
    juce::Label hintLabel;
    juce::TextButton helpButton { "HELP" };

    juce::Label formulaEqualsLabel;
    juce::TextButton domainDbButton { "dBFS" };
    juce::TextButton domainAmpButton { "AMP" };

    FormulaCurveEditor curveEditor;
    LevelMeter levelMeter;
    ScopePanel scopePanel;

    static constexpr int knobCount = 10;
    std::array<juce::Slider, knobCount> knobs;
    std::array<juce::Label, knobCount> knobCaptions;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>
        knobAttachments;

    static constexpr int cardCount = 3;
    std::array<juce::Rectangle<int>, cardCount> cardBounds;
    juce::Rectangle<int> formulaPanelBounds;

    juce::TextButton detectorPeakButton { "PEAK" };
    juce::TextButton detectorRmsButton { "RMS" };
    std::unique_ptr<juce::ParameterAttachment> detectorAttachment;

    PresetManager presetManager;
    bool suppressFormulaChange = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormulaCompressorAudioProcessorEditor)
};
