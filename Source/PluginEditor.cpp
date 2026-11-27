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

    setSize (1000, 720);
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

    formulaLabel.setText ("f(x, s:", juce::dontSendNotification);
    formulaLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain)));
    formulaLabel.setColour (juce::Label::textColourId, cobalt);
    formulaLabel.setJustificationType (juce::Justification::centredLeft);
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
    formulaEditor.setTooltip ("Use x and P1..P4. Press Enter or pause typing to compile.");
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

    hintLabel.setText ("x, s | P1-P4 | min max abs pow exp tanh | db() lin() | cond ? a : b",
                       juce::dontSendNotification);
    hintLabel.setFont (uiFont (formula::type::annotation));
    hintLabel.setColour (juce::Label::textColourId, muted);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    sidechainReadoutLabel.setFont (uiFont (formula::type::annotation));
    sidechainReadoutLabel.setColour (juce::Label::textColourId, muted);
    sidechainReadoutLabel.setJustificationType (juce::Justification::centredRight);
    sidechainReadoutLabel.setTooltip ("Sidechain detector level used as s in the formula.");
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
               "High-pass on the selected sidechain detector only (12 dB/oct).",
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

    scInputBox.addItem ("OFF", 1);
    scInputBox.addItem ("SC1", 2);
    scInputBox.addItem ("SC2", 3);
    scInputBox.addItem ("SC3", 4);
    scInputBox.addItem ("SC4", 5);
    scInputBox.setTooltip ("Selected external sidechain bus. In FL Studio map sidechain slots "
                           "to plugin inputs 3/4, 5/6, 7/8 and 9/10 on the VFX page.");
    addAndMakeVisible (scInputBox);

    scInputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getValueTreeState(), "scInput", scInputBox);

    setupSegmentedButton (monitorOffButton, 0x4d4f4e31,
                          "Normal compressor output.");
    setupSegmentedButton (monitorDeltaButton, 0x4d4f4e31,
                          "Listen to what the compressor removes (no makeup gain).");
    setupSegmentedButton (monitorSidechainButton, 0x4d4f4e31,
                          "Listen to the selected sidechain after the sidechain HPF.");

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

        auto labelCluster = row.removeFromLeft (200);
        formulaLabel.setBounds (labelCluster.removeFromLeft (76));
        labelCluster.removeFromLeft (4);

        auto domainGroup = labelCluster.removeFromLeft (92)
                               .withSizeKeepingCentre (92, 24);
        const int domainButtonWidth = (domainGroup.getWidth() - 4) / 2;
        domainDbButton.setBounds (domainGroup.removeFromLeft (domainButtonWidth));
        domainGroup.removeFromLeft (4);
        domainAmpButton.setBounds (domainGroup);

        labelCluster.removeFromLeft (4);
        formulaEqualsLabel.setBounds (labelCluster);
        formulaEditor.setBounds (row);

        auto actionRow = inner.removeFromTop (24);
        auto helpArea = actionRow.removeFromRight (70);
        helpButton.setBounds (helpArea.withSizeKeepingCentre (66, 24));
        statusLabel.setBounds (actionRow);

        auto hintRow = inner.removeFromTop (14);
        sidechainReadoutLabel.setBounds (hintRow.removeFromRight (120));
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

    // SIDECHAIN header: selected aux input.
    {
        auto headerArea = cardBounds[3].reduced (8, 6).removeFromTop (18);
        scInputBox.setBounds (headerArea.removeFromRight (64)
                                  .withSizeKeepingCentre (64, 18));
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
    const auto* parameter = processorRef.getValueTreeState()
                                .getRawParameterValue ("scInput");
    const int scInput = parameter != nullptr
                            ? juce::jlimit (0, 4, juce::roundToInt (parameter->load()))
                            : 0;

    for (int i = 1; i <= 4; ++i)
        scInputBox.setItemEnabled (i + 1, processorRef.isSidechainAvailable (i));

    const bool active = scInput > 0 && processorRef.isSidechainAvailable (scInput);

    if (! processorRef.isUiFrozen())
    {
        if (active)
        {
            const float value = processorRef.getSidechainValueForFormula();
            sidechainReadoutLabel.setText (
                processorRef.getFormulaDomain() == 1
                    ? "S " + juce::String (value, 1) + " dB"
                    : "S " + juce::String (value, 3),
                juce::dontSendNotification);
        }
        else
        {
            sidechainReadoutLabel.setText ("S OFF", juce::dontSendNotification);
        }
    }

    if (scInput <= 0)
    {
        scInputBox.setTooltip ("Sidechain input is off. s = -120 dBFS in dBFS mode "
                               "or 0 in AMP mode.");
    }
    else if (active)
    {
        scInputBox.setTooltip ("Listening to SC" + juce::String (scInput)
                               + ". Formula variable s carries this detector level.");
    }
    else
    {
        scInputBox.setTooltip ("SC" + juce::String (scInput)
                               + " is not enabled by the host or has no audio channels; "
                                 "the sidechain is silent. In standalone, use the internal "
                                 "main detector via x.");
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
