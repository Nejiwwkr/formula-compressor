#include "FormulaLookAndFeel.h"

#include "Theme.h"

#include <cmath>

namespace
{
using namespace formula::theme;

juce::Font uiFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}
} // namespace

FormulaLookAndFeel::FormulaLookAndFeel()
{
    setColour (juce::Label::textColourId, ink);

    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, ink);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, paper);

    setColour (juce::ComboBox::backgroundColourId, paperBright);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, secondary);

    setColour (juce::PopupMenu::backgroundColourId, paperBright);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelAlt);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, secondary);

    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxBackgroundColourId, paperBright);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, cobalt);
    setColour (juce::Slider::rotarySliderOutlineColourId, gridStrong);
    setColour (juce::Slider::thumbColourId, cobalt);

    setColour (juce::ToggleButton::textColourId, ink);
    setColour (juce::ToggleButton::tickColourId, ink);
    setColour (juce::ToggleButton::tickDisabledColourId, border);

    setColour (juce::TextEditor::backgroundColourId, paperBright);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, cobalt);
    setColour (juce::TextEditor::highlightColourId, cobalt.withAlpha (0.22f));
    setColour (juce::TextEditor::highlightedTextColourId, ink);

    setColour (juce::ScrollBar::thumbColourId, gridStrong);
    setColour (juce::ScrollBar::trackColourId, panelAlt);
}

juce::Font FormulaLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return uiFont (formula::type::control);
}

juce::Font FormulaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return uiFont (formula::type::control);
}

juce::Font FormulaLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

juce::Label* FormulaLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);

    if (label != nullptr)
        label->setFont (uiFont (formula::type::annotation));

    return label;
}

void FormulaLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                               juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
{
    if (! button.isEnabled())
        return;

    auto fill = button.getToggleState()
                    ? button.findColour (juce::TextButton::buttonOnColourId)
                    : backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.darker (0.08f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.04f);

    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);

    if (! button.getToggleState())
    {
        g.setColour (border.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }
}

void FormulaLookAndFeel::drawButtonText (juce::Graphics& g,
                                         juce::TextButton& button,
                                         bool,
                                         bool)
{
    auto colour = button.getToggleState()
                      ? button.findColour (juce::TextButton::textColourOnId)
                      : button.findColour (juce::TextButton::textColourOffId);

    if (! button.isEnabled())
        colour = muted;

    g.setColour (colour);
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().reduced (4),
                      juce::Justification::centred,
                      1);
}

void FormulaLookAndFeel::drawToggleButton (juce::Graphics& g,
                                           juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float box = juce::jmin (14.0f, bounds.getHeight() - 2.0f);
    auto boxArea = juce::Rectangle<float> (box, box)
                       .withY (bounds.getCentreY() - box * 0.5f);

    g.setColour (paperBright);
    g.fillRoundedRectangle (boxArea, 3.0f);

    g.setColour (button.getToggleState() ? ink : border);
    g.drawRoundedRectangle (boxArea.reduced (0.5f), 3.0f, 1.2f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath (boxArea.getX() + box * 0.22f, boxArea.getCentreY());
        tick.lineTo (boxArea.getCentreX() - box * 0.05f, boxArea.getBottom() - box * 0.28f);
        tick.lineTo (boxArea.getRight() - box * 0.2f, boxArea.getY() + box * 0.28f);

        g.setColour (cobalt);
        g.strokePath (tick, juce::PathStrokeType (1.8f));
    }

    g.setColour (button.isEnabled() ? ink : muted);
    g.setFont (uiFont (formula::type::control));
    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().withTrimmedLeft (juce::roundToInt (box) + 8),
                      juce::Justification::centredLeft,
                      1);
}

void FormulaLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                       bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                          static_cast<float> (width),
                                          static_cast<float> (height)).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (box.isEnabled() ? border : border.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const float arrowWidth = 9.0f;
    juce::Path arrow;
    arrow.addTriangle (static_cast<float> (width) - 17.0f,
                       static_cast<float> (height) * 0.5f - 2.0f,
                       static_cast<float> (width) - 17.0f + arrowWidth,
                       static_cast<float> (height) * 0.5f - 2.0f,
                       static_cast<float> (width) - 17.0f + arrowWidth * 0.5f,
                       static_cast<float> (height) * 0.5f + 4.0f);

    g.setColour (box.isEnabled() ? secondary : muted);
    g.fillPath (arrow);
}

void FormulaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                           int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const float size = static_cast<float> (juce::jmin (width, height)) - 6.0f;

    if (size <= 2.0f)
        return;

    const auto bounds = juce::Rectangle<float> (size, size)
                            .withCentre ({ static_cast<float> (x) + static_cast<float> (width) * 0.5f,
                                           static_cast<float> (y) + static_cast<float> (height) * 0.5f });

    const auto centre = bounds.getCentre();
    const float radius = bounds.getWidth() * 0.5f;
    const float lineWidth = juce::jmax (2.5f, radius * 0.15f);
    const float angle = rotaryStartAngle
                        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - lineWidth * 0.5f,
                         radius - lineWidth * 0.5f, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);

    g.setColour (gridStrong);
    g.strokePath (track, juce::PathStrokeType (lineWidth));

    if (sliderPosProportional > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius - lineWidth * 0.5f,
                                radius - lineWidth * 0.5f, 0.0f,
                                rotaryStartAngle, angle, true);

        g.setColour (slider.isEnabled() ? cobalt : muted);
        g.strokePath (valueArc, juce::PathStrokeType (lineWidth));
    }

    const float bodyRadius = juce::jmax (2.0f, radius - lineWidth * 2.0f);

    g.setColour (paperBright);
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                       .withCentre (centre));

    g.setColour (border);
    g.drawEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                       .withCentre (centre),
                   1.0f);

    const float pointerLength = bodyRadius * 0.78f;
    const juce::Point<float> pointer (centre.x + std::sin (angle) * pointerLength,
                                      centre.y - std::cos (angle) * pointerLength);

    g.setColour (slider.isEnabled() ? ink : muted);
    g.drawLine ({ centre, pointer }, juce::jmax (1.8f, radius * 0.07f));
}

void FormulaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
                                           int width, int height, float sliderPos,
                                           float, float,
                                           const juce::Slider::SliderStyle style,
                                           juce::Slider& slider)
{
    if (style == juce::Slider::LinearVertical)
    {
        const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        juce::Rectangle<float> track (centreX - 2.0f, static_cast<float> (y),
                                      4.0f, static_cast<float> (height));

        g.setColour (panelAlt);
        g.fillRoundedRectangle (track, 2.0f);

        g.setColour (slider.isEnabled() ? cobalt : muted);
        g.fillRoundedRectangle (track.withTop (sliderPos), 2.0f);
        return;
    }

    const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    juce::Rectangle<float> track (static_cast<float> (x), centreY - 2.0f,
                                  static_cast<float> (width), 4.0f);

    g.setColour (panelAlt);
    g.fillRoundedRectangle (track, 2.0f);

    g.setColour (slider.isEnabled() ? cobalt : muted);
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    g.setColour (paperBright);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f)
                       .withCentre ({ sliderPos, centreY }));
    g.setColour (cobalt);
    g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f)
                       .withCentre ({ sliderPos, centreY }),
                   1.5f);
}

void FormulaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (paperBright);
    g.setColour (border);
    g.drawRect (0, 0, width, height, 1);
}

void FormulaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tooltipText,
                                      int width, int height)
{
    juce::Rectangle<float> bounds (0.0f, 0.0f,
                                   static_cast<float> (width),
                                   static_cast<float> (height));

    g.setColour (paperBright);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    juce::AttributedString s;
    s.setJustification (juce::Justification::centredLeft);
    s.append (tooltipText, uiFont (formula::type::annotation), ink);

    juce::TextLayout layout;
    layout.createLayout (s, static_cast<float> (width) - 12.0f);
    layout.draw (g, bounds.reduced (6.0f, 4.0f));
}
