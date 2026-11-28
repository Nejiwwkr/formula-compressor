#include "PluginEditor.h"

#include "PluginProcessor.h"
#include "ui/Theme.h"

namespace
{
using namespace formula::theme;

juce::Font uiFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

} // namespace

FormulaCompressorAudioProcessorEditor::FormulaCompressorAudioProcessorEditor (
    FormulaCompressorAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      processorRef (processor),
      bypassAttachment (processor.getValueTreeState(), "bypass", bypassToggle),
      curveEditor (processor),
      levelMeter (processor),
      scopePanel (processor),
      presetManager (processor)
{
    processorRef.setEditorActive (true);
    setLookAndFeel (&lookAndFeel);

    setResizable (true, true);
    setResizeLimits (900, 680, 1600, 1100);

    titleLabel.setText ("FORMULA COMPRESSOR", juce::dontSendNotification);
    titleLabel.setFont (uiFont (formula::type::display));
    titleLabel.setColour (juce::Label::textColourId, ink);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("compression as a function f(x)",
                           juce::dontSendNotification);
    subtitleLabel.setFont (uiFont (formula::type::primary));
    subtitleLabel.setColour (juce::Label::textColourId, secondary);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    presetBox.setTextWhenNothingSelected ("presets");
    presetBox.setTooltip ("Built-in formulas and your saved presets.");
    presetBox.onChange = [this] { presetChanged(); };
    addAndMakeVisible (presetBox);
    refreshPresetList();

    styleButton (savePresetButton);
    savePresetButton.setColour (juce::TextButton::buttonColourId, vermilion);
    savePresetButton.setColour (juce::TextButton::textColourOffId, paperBright);
    savePresetButton.setTooltip ("Save the current formula and all parameters as a preset.");
    savePresetButton.onClick = [this] { savePresetDialog(); };
    addAndMakeVisible (savePresetButton);

    bypassToggle.setButtonText ("BYPASS");
    bypassToggle.setColour (juce::ToggleButton::textColourId, ink);
    bypassToggle.setColour (juce::ToggleButton::tickColourId, ink);
    addAndMakeVisible (bypassToggle);

    formulaLabel.setText ("f(x:", juce::dontSendNotification);
    formulaLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain)));
    formulaLabel.setColour (juce::Label::textColourId, cobalt);
    formulaLabel.setJustificationType (juce::Justification::centredLeft);
    formulaLabel.setMinimumHorizontalScale (1.0f);
    formulaLabel.setComponentID ("formulaLabel");
    addAndMakeVisible (formulaLabel);

    formulaEditor.setMultiLine (true);
    formulaEditor.setReturnKeyStartsNewLine (false);
    formulaEditor.setScrollbarsShown (false);
    formulaEditor.setIndents (8, 6);
    formulaEditor.setInputRestrictions (1024);
    formulaEditor.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), formula::type::mono, juce::Font::plain)));
    formulaEditor.setColour (juce::TextEditor::backgroundColourId, paperBright);
    formulaEditor.setColour (juce::TextEditor::textColourId, ink);
    formulaEditor.setColour (juce::TextEditor::outlineColourId, border);
    formulaEditor.setColour (juce::TextEditor::focusedOutlineColourId, cobalt);
    formulaEditor.setColour (juce::TextEditor::highlightColourId, cobalt.withAlpha (0.18f));
    formulaEditor.setColour (juce::TextEditor::highlightedTextColourId, ink);
    formulaEditor.setColour (juce::CaretComponent::caretColourId, ink);
    formulaEditor.setText (processorRef.getFormula(), false);
    formulaEditor.setTooltip ("Use x, s1..s4, s and P1..P4. Press Enter or pause typing to compile.");
    formulaEditor.setComponentID ("formulaEditor");
    formulaEditor.onTextChange = [this] { scheduleFormulaApply(); };
    formulaEditor.onReturnKey = [this]
    {
        stopTimer();
        applyFormula();
    };
    addAndMakeVisible (formulaEditor);

    statusLabel.setFont (uiFont (formula::type::annotation));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    hintLabel.setText ("x | P1-P4 | min max abs pow exp tanh | db() lin() | cond ? a : b",
                       juce::dontSendNotification);
    hintLabel.setFont (uiFont (formula::type::annotation));
    hintLabel.setColour (juce::Label::textColourId, muted);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    hintLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (hintLabel);

    sidechainReadoutLabel.setFont (uiFont (10.0f));
    sidechainReadoutLabel.setColour (juce::Label::textColourId, muted);
    sidechainReadoutLabel.setJustificationType (juce::Justification::centredRight);
    sidechainReadoutLabel.setTooltip ("S1..S4 and total S sidechain detector levels.");
    sidechainReadoutLabel.setComponentID ("sidechainReadoutLabel");
    addAndMakeVisible (sidechainReadoutLabel);

    styleButton (helpButton);
    helpButton.setTooltip ("Show every variable, operator and function.");
    helpButton.onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Formula reference",
            formula::functionReference(),
            "Close");
    };
    addAndMakeVisible (helpButton);

    formulaEqualsLabel.setText (") =", juce::dontSendNotification);
    formulaEqualsLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain)));
    formulaEqualsLabel.setColour (juce::Label::textColourId, cobalt);
    formulaEqualsLabel.setJustificationType (juce::Justification::centredLeft);
    formulaEqualsLabel.setMinimumHorizontalScale (1.0f);
    formulaEqualsLabel.setComponentID ("formulaEqualsLabel");
    addAndMakeVisible (formulaEqualsLabel);

    const auto setupSegmentedButton = [this] (juce::TextButton& button,
                                              int radioGroup,
                                              const juce::String& tooltip)
    {
        styleButton (button);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (radioGroup);
        button.setColour (juce::TextButton::buttonColourId, panelAlt);
        button.setColour (juce::TextButton::buttonOnColourId, cobalt);
        button.setColour (juce::TextButton::textColourOffId, secondary);
        button.setColour (juce::TextButton::textColourOnId, paperBright);
        button.setTooltip (tooltip);
        addAndMakeVisible (button);
    };

    setupSegmentedButton (domainAmpButton, 0x58444f4d,
                          "x is the linear input amplitude, f(x) returns linear amplitude.");
    setupSegmentedButton (domainDbButton, 0x58444f4d,
                          "x is the input level in dBFS, f(x) returns dBFS; gain dB = f(x) - x.");

    domainAmpButton.onClick = [this]
    {
        processorRef.setFormulaDomain (0);
        refreshDomainButtons();
    };

    domainDbButton.onClick = [this]
    {
        processorRef.setFormulaDomain (1);
        refreshDomainButtons();
    };

    refreshDomainButtons();

    addAndMakeVisible (curveEditor);
    addAndMakeVisible (levelMeter);
    addAndMakeVisible (scopePanel);

    setupKnob (0, "ATTACK", "attack",
               "Attack time: how quickly the detector follows rising level.");
    setupKnob (1, "RELEASE", "release",
               "Release time: how quickly the detector falls when the signal drops.");
    setupKnob (2, "HOLD", "hold",
               "Hold: keep the gain reduction for this long before releasing.");
    setupKnob (3, "INPUT", "input", "Input gain, before the detector.");
    setupKnob (4, "MIX", "mix", "Dry / wet blend.");
    setupKnob (5, "MAKEUP", "makeup", "Output make-up gain.");
    setupKnob (6, "P1", "p1", "P1 knob, referenced in the formula as P1.");
    setupKnob (7, "P2", "p2", "P2 knob, referenced in the formula as P2.");
    setupKnob (8, "P3", "p3", "P3 knob, referenced in the formula as P3.");
    setupKnob (9, "P4", "p4", "P4 knob, referenced in the formula as P4.");
    setupKnob (10, "HPF", "scHpf",
               "High-pass shared by all four sidechain detectors (12 dB/oct).",
               56);
    setupKnob (11, "LINK", "stereoLink",
               "Stereo link: 100% = one linked detector, 0% = dual mono.",
               56);

    setupSegmentedButton (detectorPeakButton, 0x44455431,
                          "Peak detection follows transients.");
    setupSegmentedButton (detectorRmsButton, 0x44455431,
                          "RMS detection follows average energy (10 ms time constant).");

    detectorPeakButton.onClick = [this]
    {
        processorRef.setParameterValue ("detector", 0.0f);
        refreshDetectorButtons();
    };

    detectorRmsButton.onClick = [this]
    {
        processorRef.setParameterValue ("detector", 1.0f);
        refreshDetectorButtons();
    };

    if (auto* detectorParameter = processorRef.getValueTreeState().getParameter ("detector"))
    {
        detectorAttachment = std::make_unique<juce::ParameterAttachment> (
            *detectorParameter,
            [this] (float) { refreshDetectorButtons(); });
        detectorAttachment->sendInitialUpdate();
    }

    refreshDetectorButtons();

    setupSegmentedButton (scOffButton, 0x53434d31,
                          "Sidechain detector off. s1..s4 and s use their silent values.");
    setupSegmentedButton (scOnButton, 0x53434d31,
                          "Use all enabled sidechain aux buses simultaneously. "
                          "In FL Studio map the four slots to plugin inputs 3/4, 5/6, 7/8 and 9/10.");

    scOffButton.setComponentID ("scOffButton");
    scOnButton.setComponentID ("scOnButton");

    scOffButton.onClick = [this] { processorRef.setParameterValue ("scEnabled", 0.0f); refreshSidechainState(); };
    scOnButton.onClick = [this] { processorRef.setParameterValue ("scEnabled", 1.0f); refreshSidechainState(); };

    if (auto* scEnabledParameter = processorRef.getValueTreeState().getParameter ("scEnabled"))
    {
        scEnabledAttachment = std::make_unique<juce::ParameterAttachment> (
            *scEnabledParameter,
            [this] (float) { refreshSidechainState(); });
        scEnabledAttachment->sendInitialUpdate();
    }

    setupSegmentedButton (monitorOffButton, 0x4d4f4e31,
                          "Normal compressor output.");
    setupSegmentedButton (monitorDeltaButton, 0x4d4f4e31,
                          "Listen to what the compressor removes (no makeup gain).");
    setupSegmentedButton (monitorSidechainButton, 0x4d4f4e31,
                          "Listen to the sum of the sidechain buses after the sidechain HPF.");

    monitorOffButton.onClick = [this] { processorRef.setParameterValue ("monitor", 0.0f); refreshMonitorButtons(); };
    monitorDeltaButton.onClick = [this] { processorRef.setParameterValue ("monitor", 1.0f); refreshMonitorButtons(); };
    monitorSidechainButton.onClick = [this] { processorRef.setParameterValue ("monitor", 2.0f); refreshMonitorButtons(); };

    if (auto* monitorParameter = processorRef.getValueTreeState().getParameter ("monitor"))
    {
        monitorAttachment = std::make_unique<juce::ParameterAttachment> (
            *monitorParameter,
            [this] (float) { refreshMonitorButtons(); });
        monitorAttachment->sendInitialUpdate();
    }

    refreshMonitorButtons();
    refreshSidechainState();

    sidechainPollTimer.onTick = [this] { refreshSidechainState(); };
    sidechainPollTimer.startTimerHz (10);

    processorRef.getFormulaBroadcaster().addChangeListener (this);
    refreshFormulaStatus();

    // Size last: every child exists now, so the first resized() lays out the
    // OFF header correctly (in particular hides the sidechain readout).
    setSize (1000, 720);
}

FormulaCompressorAudioProcessorEditor::~FormulaCompressorAudioProcessorEditor()
{
    sidechainPollTimer.stopTimer();
    processorRef.getFormulaBroadcaster().removeChangeListener (this);
    processorRef.setEditorActive (false);
    setLookAndFeel (nullptr);
}

void FormulaCompressorAudioProcessorEditor::styleButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, panel);
    button.setColour (juce::TextButton::buttonOnColourId, ink);
    button.setColour (juce::TextButton::textColourOffId, ink);
    button.setColour (juce::TextButton::textColourOnId, paper);
}

void FormulaCompressorAudioProcessorEditor::setupKnob (int index,
                                                       const juce::String& caption,
                                                       const juce::String& parameterID,
                                                       const juce::String& tooltip,
                                                       int textBoxWidth)
{
    auto& slider = knobs[static_cast<std::size_t> (index)];
    auto& label = knobCaptions[static_cast<std::size_t> (index)];

    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, 16);
    slider.setColour (juce::Slider::textBoxTextColourId, ink);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, paperBright);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setPopupDisplayEnabled (false, false, this);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    label.setText (caption, juce::dontSendNotification);
    label.setFont (uiFont (formula::type::annotation));
    label.setColour (juce::Label::textColourId, secondary);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    knobAttachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.getValueTreeState(), parameterID, slider));
}

void FormulaCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    g.setColour (cobalt);
    g.fillRect (0, 0, 2, getHeight());

    // Formula panel.
    g.setColour (panel);
    if (! formulaPanelBounds.isEmpty())
        g.fillRoundedRectangle (formulaPanelBounds.toFloat(), 7.0f);

    // Header preset cluster, separated from the bypass switch.
    {
        const auto presetArea = presetBox.getBounds()
                                    .getUnion (savePresetButton.getBounds());
        g.setColour (panel);
        g.fillRoundedRectangle (presetArea.toFloat().expanded (5.0f, 4.0f), 6.0f);

        g.setColour (border);
        g.drawVerticalLine (bypassToggle.getX() - 9,
                            static_cast<float> (bypassToggle.getY() + 2),
                            static_cast<float> (bypassToggle.getBottom() - 2));
    }

    // Control cards: TIME / GAIN / FORMULA / SIDECHAIN.
    static const char* cardTitles[cardCount] = { "TIME", "GAIN", "FORMULA", "SIDECHAIN" };

    g.setFont (uiFont (9.5f));

    for (int i = 0; i < cardCount; ++i)
    {
        const auto card = cardBounds[static_cast<std::size_t> (i)];

        if (card.isEmpty())
            continue;

        g.setColour (panel);
        g.fillRoundedRectangle (card.toFloat(), 7.0f);

        const bool leftAligned = i == 1 || i == 3;
        g.setColour (secondary);
        g.drawText (cardTitles[i],
                    card.toFloat().reduced (8.0f, 6.0f).withHeight (16.0f),
                    leftAligned ? juce::Justification::centredLeft
                                : juce::Justification::centred,
                    false);
    }

    // Segmented button groups get their own inset track.
    const auto drawGroupTrack = [&] (juce::Rectangle<int> group)
    {
        if (group.isEmpty())
            return;

        g.setColour (panelAlt);
        g.fillRoundedRectangle (group.toFloat().expanded (2.0f, 0.0f), 4.0f);
    };

    drawGroupTrack (domainAmpButton.getBounds().getUnion (domainDbButton.getBounds()));
    drawGroupTrack (monitorOffButton.getBounds()
                        .getUnion (monitorDeltaButton.getBounds())
                        .getUnion (monitorSidechainButton.getBounds()));
}

void FormulaCompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto header = area.removeFromTop (38);
    auto headerRight = header;

    bypassToggle.setBounds (headerRight.removeFromRight (84).reduced (0, 6));
    headerRight.removeFromRight (14);
    savePresetButton.setBounds (headerRight.removeFromRight (62).reduced (0, 6));
    headerRight.removeFromRight (6);
    presetBox.setBounds (headerRight.removeFromRight (190).reduced (0, 6));

    auto titleArea = header.withRight (headerRight.getRight() - 12);
    titleLabel.setBounds (titleArea.removeFromLeft (330));
    subtitleLabel.setBounds (titleArea);

    area.removeFromTop (10);

    auto formulaPanel = area.removeFromTop (100);
    formulaPanelBounds = formulaPanel;

    {
        auto inner = formulaPanel.reduced (8);
        auto row = inner.removeFromTop (46);

        const juce::String prefixText = sidechainShown ? "f(x, s1..4, s:" : "f(x:";
        formulaLabel.setText (prefixText, juce::dontSendNotification);

        // Measure with the same font the labels are drawn with. Label draws
        // with a fitted-text layout whose required width is a little larger
        // than the glyph bounding box; too little and ") =" becomes ")" + ellipsis.
        const auto headerWidth = [this] (const juce::String& labelText)
        {
            const auto& font = formulaLabel.getFont();
            return juce::GlyphArrangement::getStringWidthInt (font, labelText) + 20;
        };

        const int prefixWidth = std::max (headerWidth (prefixText), 52);
        const int equalsWidth = std::max (headerWidth (") ="), 38);
        const int domainWidth = 92;
        const int gap = 4;
        const int clusterWidth = prefixWidth + gap + domainWidth + gap + equalsWidth;

        auto labelCluster = row.removeFromLeft (clusterWidth);
        formulaLabel.setBounds (labelCluster.removeFromLeft (prefixWidth)
                                    .withSizeKeepingCentre (prefixWidth, 24));
        labelCluster.removeFromLeft (gap);

        auto domainGroup = labelCluster.removeFromLeft (domainWidth)
                               .withSizeKeepingCentre (domainWidth, 24);
        const int domainButtonWidth = (domainGroup.getWidth() - 4) / 2;
        domainDbButton.setBounds (domainGroup.removeFromLeft (domainButtonWidth));
        domainGroup.removeFromLeft (4);
        domainAmpButton.setBounds (domainGroup);

        labelCluster.removeFromLeft (gap);
        formulaEqualsLabel.setBounds (labelCluster.removeFromLeft (equalsWidth)
                                          .withSizeKeepingCentre (equalsWidth, 24));
        formulaEditor.setBounds (row);

        auto actionRow = inner.removeFromTop (24);
        auto helpArea = actionRow.removeFromRight (70);
        helpButton.setBounds (helpArea.withSizeKeepingCentre (66, 24));
        statusLabel.setBounds (actionRow);

        auto hintRow = inner.removeFromTop (14);

        sidechainReadoutLabel.setVisible (sidechainShown);

        if (sidechainShown)
        {
            sidechainReadoutLabel.setBounds (hintRow.removeFromRight (300));
            hintLabel.setText ("x, s1..s4, s | P1-P4 | min max abs pow exp tanh | "
                               "db() lin() | cond ? a : b",
                               juce::dontSendNotification);
        }
        else
        {
            hintLabel.setText ("x | P1-P4 | min max abs pow exp tanh | db() lin() | "
                               "cond ? a : b",
                               juce::dontSendNotification);
        }

        hintLabel.setBounds (hintRow);
    }

    area.removeFromTop (10);

    auto cardsArea = area.removeFromBottom (140);
    const int cardGap = 10;

    const int totalUnits = 12;
    const int unit = (cardsArea.getWidth() - cardGap * 3) / totalUnits;

    cardBounds[0] = cardsArea.removeFromLeft (unit * 3);
    cardsArea.removeFromLeft (cardGap);
    cardBounds[1] = cardsArea.removeFromLeft (unit * 3);
    cardsArea.removeFromLeft (cardGap);
    cardBounds[2] = cardsArea.removeFromLeft (unit * 4);
    cardsArea.removeFromLeft (cardGap);
    cardBounds[3] = cardsArea;

    const auto layoutCard = [this] (const juce::Rectangle<int>& card,
                                    int firstKnob, int count)
    {
        auto inner = card.reduced (8, 6);
        inner.removeFromTop (18); // title row

        const int cellWidth = inner.getWidth() / count;

        for (int k = 0; k < count; ++k)
        {
            auto cell = inner.removeFromLeft (k == count - 1 ? inner.getWidth()
                                                             : cellWidth);

            knobCaptions[static_cast<std::size_t> (firstKnob + k)]
                .setBounds (cell.removeFromTop (14));
            knobs[static_cast<std::size_t> (firstKnob + k)].setBounds (cell);
        }
    };

    layoutCard (cardBounds[0], 0, 3);  // TIME
    layoutCard (cardBounds[1], 3, 3);  // GAIN: INPUT / MIX / MAKEUP
    layoutCard (cardBounds[2], 6, 4);  // FORMULA
    layoutCard (cardBounds[3], 10, 2); // SIDECHAIN: HPF / LINK

    // GAIN header: monitor mode.
    {
        auto headerArea = cardBounds[1].reduced (8, 6).removeFromTop (18);
        auto group = headerArea.removeFromRight (84);

        monitorOffButton.setBounds (group.removeFromLeft (26));
        group.removeFromLeft (2);
        monitorDeltaButton.setBounds (group.removeFromLeft (34));
        group.removeFromLeft (2);
        monitorSidechainButton.setBounds (group);
    }

    // SIDECHAIN header: OFF / ON.
    {
        auto headerArea = cardBounds[3].reduced (8, 6).removeFromTop (18);
        auto group = headerArea.removeFromRight (60);

        scOffButton.setBounds (group.removeFromLeft (29));
        group.removeFromLeft (2);
        scOnButton.setBounds (group);
    }

    area.removeFromBottom (10);

    auto meterArea = area.removeFromRight (200);
    area.removeFromRight (10);

    levelMeter.setBounds (meterArea);

    // PEAK / RMS live in the LEVELS header.
    {
        auto meterContent = meterArea.reduced (10, 8);
        auto meterHeader = meterContent.removeFromTop (32);
        const int plotRight = meterContent.getRight() - 8;
        auto buttons = juce::Rectangle<int> (plotRight - 96,
                                             meterHeader.getCentreY() - 12,
                                             96, 24);

        detectorPeakButton.setBounds (buttons.removeFromLeft (47));
        buttons.removeFromLeft (2);
        detectorRmsButton.setBounds (buttons);
        detectorPeakButton.toFront (false);
        detectorRmsButton.toFront (false);
    }

    // Give the transfer plot exactly enough width to stay square while sharing
    // its vertical scale with the time plot and the meters.
    const int gridSide = juce::jmax (180, area.getHeight() - 88);
    const int curveWidth = juce::jlimit (300,
                                         juce::jmax (300, area.getWidth() - 320),
                                         gridSide + 66);

    auto curveArea = area.removeFromLeft (curveWidth);
    area.removeFromLeft (10);

    scopePanel.setBounds (area);
    curveEditor.setBounds (curveArea);
}

void FormulaCompressorAudioProcessorEditor::scheduleFormulaApply()
{
    if (suppressFormulaChange)
        return;

    startTimer (300);
}

void FormulaCompressorAudioProcessorEditor::timerCallback()
{
    stopTimer();
    applyFormula();
}

void FormulaCompressorAudioProcessorEditor::applyFormula()
{
    if (suppressFormulaChange)
        return;

    processorRef.setFormula (formulaEditor.getText(), true);
    refreshFormulaStatus();
}

void FormulaCompressorAudioProcessorEditor::refreshFormulaStatus()
{
    if (processorRef.isFormulaValid())
    {
        statusLabel.setColour (juce::Label::textColourId, teal);
        statusLabel.setText ("OK - compiled, curve updated", juce::dontSendNotification);
        formulaEditor.setTooltip ("Use x and P1..P4. Press Enter or pause typing to compile.");
    }
    else
    {
        const auto message = "Column " + juce::String (processorRef.getFormulaErrorColumn())
                             + " - " + processorRef.getFormulaError();

        statusLabel.setColour (juce::Label::textColourId, vermilion);
        statusLabel.setText (message, juce::dontSendNotification);
        formulaEditor.setTooltip (message);
    }
}

void FormulaCompressorAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const auto newText = processorRef.getFormula();

    if (formulaEditor.getText() != newText)
    {
        suppressFormulaChange = true;
        formulaEditor.setText (newText, false);
        suppressFormulaChange = false;
    }

    refreshFormulaStatus();
    refreshDomainButtons();
    refreshSidechainState();
}

void FormulaCompressorAudioProcessorEditor::refreshDomainButtons()
{
    const int domain = processorRef.getFormulaDomain();
    domainAmpButton.setToggleState (domain == 0, juce::dontSendNotification);
    domainDbButton.setToggleState (domain == 1, juce::dontSendNotification);
}

void FormulaCompressorAudioProcessorEditor::refreshDetectorButtons()
{
    const auto* parameter = processorRef.getValueTreeState()
                                .getRawParameterValue ("detector");
    const int mode = parameter != nullptr
                         ? juce::roundToInt (parameter->load())
                         : 0;

    detectorPeakButton.setToggleState (mode == 0, juce::dontSendNotification);
    detectorRmsButton.setToggleState (mode == 1, juce::dontSendNotification);
}

void FormulaCompressorAudioProcessorEditor::refreshMonitorButtons()
{
    const auto* parameter = processorRef.getValueTreeState()
                                .getRawParameterValue ("monitor");
    const int mode = parameter != nullptr
                         ? juce::roundToInt (parameter->load())
                         : 0;

    monitorOffButton.setToggleState (mode == 0, juce::dontSendNotification);
    monitorDeltaButton.setToggleState (mode == 1, juce::dontSendNotification);
    monitorSidechainButton.setToggleState (mode == 2, juce::dontSendNotification);
}

void FormulaCompressorAudioProcessorEditor::refreshSidechainState()
{
    const bool enabled = processorRef.isSidechainEnabled();

    scOffButton.setToggleState (! enabled, juce::dontSendNotification);
    scOnButton.setToggleState (enabled, juce::dontSendNotification);

    bool anyAvailable = false;

    for (int i = 1; i <= 4; ++i)
        anyAvailable = anyAvailable || processorRef.isSidechainAvailable (i);

    if (! processorRef.isUiFrozen() && enabled)
    {
        const bool db = processorRef.getFormulaDomain() == 1;
        const auto format = [db] (float value)
        {
            return db ? juce::String (value, 1) : juce::String (value, 3);
        };

        juce::StringArray parts;

        for (int i = 1; i <= 4; ++i)
        {
            parts.add ("S" + juce::String (i) + " "
                       + (processorRef.isSidechainAvailable (i)
                              ? format (processorRef.getSidechainValueForFormula (i))
                              : juce::String ("--")));
        }

        parts.add (juce::String ("S ")
                   + (anyAvailable ? format (processorRef.getSidechainTotalForFormula())
                                   : juce::String ("--")));

        sidechainReadoutLabel.setText (parts.joinIntoString ("  "),
                                       juce::dontSendNotification);
    }

    const juce::String tooltip = ! enabled
        ? "Sidechain detector off. s1..s4 = 0 (AMP) or -120 dBFS."
        : (anyAvailable
               ? "All enabled aux buses feed s1..s4; total s is their linear sum."
               : "Sidechain is ON, but no aux bus is enabled by the host. "
                 "In standalone this is normal; S readouts show --.");

    scOffButton.setTooltip (tooltip);
    scOnButton.setTooltip (tooltip);

    if (enabled != sidechainShown)
    {
        sidechainShown = enabled;
        resized();
    }
}

void FormulaCompressorAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Built-in");

    int itemID = 1;

    for (const auto& preset : PresetManager::getBuiltInPresets())
        presetBox.addItem (preset.name, itemID++);

    presetBox.addSectionHeading ("User");

    const auto names = presetManager.getUserPresetNames();

    if (names.isEmpty())
    {
        presetBox.addItem ("(no user presets)", 9999);
        presetBox.setItemEnabled (9999, false);
    }
    else
    {
        int userID = 1000;

        for (const auto& name : names)
            presetBox.addItem (name, userID++);
    }

    presetBox.setSelectedId (0, juce::dontSendNotification);
}

void FormulaCompressorAudioProcessorEditor::presetChanged()
{
    const int id = presetBox.getSelectedId();

    if (id <= 0 || id == 9999)
        return;

    if (id < 1000)
        presetManager.loadBuiltIn (id - 1);
    else
        presetManager.loadPreset (presetBox.getText());
}

void FormulaCompressorAudioProcessorEditor::savePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save preset",
                                          "Name for this preset:",
                                          juce::MessageBoxIconType::NoIcon);

    window->addTextEditor ("name", "My Preset", {});
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<FormulaCompressorAudioProcessorEditor> safeThis (this);

    window->enterModalState (
        true,
        juce::ModalCallbackFunction::create ([safeThis, window] (int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            const auto name = window->getTextEditorContents ("name");

            if (safeThis->presetManager.savePreset (name))
            {
                safeThis->refreshPresetList();

                for (int i = 0; i < safeThis->presetBox.getNumItems(); ++i)
                {
                    if (safeThis->presetBox.getItemText (i)
                        == PresetManager::sanitiseName (name))
                    {
                        safeThis->presetBox.setSelectedItemIndex (
                            i, juce::dontSendNotification);
                        break;
                    }
                }
            }
        }),
        true);
}
