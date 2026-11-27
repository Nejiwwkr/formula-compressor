#include "FormulaCurveEditor.h"

#include "../PluginProcessor.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
using namespace formula::theme;

juce::Font uiFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

float gainToDb (float value)
{
    return 20.0f * std::log10 (std::max (1.0e-6f, std::fabs (value)));
}

float dbToGain (float db)
{
    return std::pow (10.0f, db / 20.0f);
}

juce::String tickText (float value, bool linear)
{
    if (linear)
    {
        if (value == 0.0f)
            return "0";

        if (std::abs (value - 1.5f) < 0.01f)
            return "1.5";

        return juce::String (value, 2);
    }

    return juce::String (juce::roundToInt (value));
}
} // namespace

FormulaCurveEditor::FormulaCurveEditor (FormulaCompressorAudioProcessor& processorIn)
    : processorRef (processorIn)
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setComponentID ("formulaCurveEditor");
    setTooltip ("Teal = boost, vermilion = reduction; a dashed curve means inverted polarity.");

    processorRef.getFormulaBroadcaster().addChangeListener (this);

    program = processorRef.getActiveProgram();
    formulaDomain = processorRef.getFormulaDomain();
    liveInputLinear = processorRef.getDetectorLevel();
    sidechainValue = processorRef.getSidechainValueForFormula();
    sidechainActive = processorRef.isSidechainActiveForDisplay();

    for (int i = 0; i < 4; ++i)
        pValues[static_cast<std::size_t> (i)] = processorRef.getPValue (i + 1);

    const char* names[3] = { "LEVEL", "GAIN", "LINEAR" };
    const char* tips[3] = {
        "Input and output level in dB. Equal scale: unity is the 45 degree diagonal.",
        "Gain = output / input in dB. A constant gain is a flat line.",
        "Linear amplitude with ordinary axes. The slope is the gain factor."
    };

    for (int i = 0; i < 3; ++i)
    {
        auto& button = modeButtons[static_cast<std::size_t> (i)];

        button.setButtonText (names[i]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (0x46435631);
        button.setColour (juce::TextButton::buttonColourId, panelAlt);
        button.setColour (juce::TextButton::buttonOnColourId, cobalt);
        button.setColour (juce::TextButton::textColourOffId, secondary);
        button.setColour (juce::TextButton::textColourOnId, paperBright);
        button.setTooltip (tips[i]);
        button.onClick = [this, i]
        {
            setMode (static_cast<ViewMode> (i));
        };

        addAndMakeVisible (button);
    }

    modeButtons[0].setToggleState (true, juce::dontSendNotification);
    startTimerHz (30);
}

FormulaCurveEditor::~FormulaCurveEditor()
{
    processorRef.getFormulaBroadcaster().removeChangeListener (this);
}

void FormulaCurveEditor::setMode (ViewMode newMode)
{
    mode = newMode;

    for (int i = 0; i < 3; ++i)
        modeButtons[static_cast<std::size_t> (i)].setToggleState (
            i == static_cast<int> (mode), juce::dontSendNotification);

    rebuildCurve();
    repaint();
}

void FormulaCurveEditor::resized()
{
    const auto layout = computeLayout();

    const auto header = layout.header;
    const float groupWidth = 3 * 54.0f + 2 * 2.0f;

    auto buttonArea = juce::Rectangle<float> (layout.plot.getRight() - groupWidth,
                                              header.getY(), groupWidth,
                                              header.getHeight());

    for (int i = 0; i < 3; ++i)
    {
        auto cell = buttonArea.removeFromLeft (54.0f)
                        .withSizeKeepingCentre (54.0f, 24.0f);
        modeButtons[static_cast<std::size_t> (i)].setBounds (cell.toNearestInt());
        buttonArea.removeFromLeft (2.0f);
    }

    rebuildCurve();
}

FormulaCurveEditor::Layout FormulaCurveEditor::computeLayout() const
{
    Layout layout;

    auto card = getLocalBounds().toFloat();
    auto content = card.reduced (10.0f, 8.0f);
    layout.header = content.removeFromTop (32.0f);

    const float marginLeft = 22.0f;
    const float marginRight = 8.0f;
    const float bottom = 24.0f;
    const float availableHeight = content.getHeight() - bottom;
    const float side = std::max (60.0f,
                                 std::min (availableHeight,
                                           content.getWidth() - marginLeft - marginRight));

    layout.plot = juce::Rectangle<float> (content.getX() + marginLeft,
                                          content.getY(), side, side);

    return layout;
}

juce::Point<float> FormulaCurveEditor::toScreen (float inputValue, float outputValue,
                                                 juce::Rectangle<float> plot) const
{
    const bool linear = mode == ViewMode::Linear;
    const float minimum = linear ? linearMin : levelMinDb;
    const float maximum = linear ? linearMax : levelMaxDb;

    const float nx = (juce::jlimit (minimum, maximum, inputValue) - minimum)
                     / (maximum - minimum);
    const float ny = (juce::jlimit (minimum, maximum, outputValue) - minimum)
                     / (maximum - minimum);

    return { plot.getX() + nx * plot.getWidth(),
             plot.getBottom() - ny * plot.getHeight() };
}

float FormulaCurveEditor::screenToInputValue (float x, juce::Rectangle<float> plot) const
{
    const bool linear = mode == ViewMode::Linear;
    const float minimum = linear ? linearMin : levelMinDb;
    const float maximum = linear ? linearMax : levelMaxDb;

    const float nx = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return minimum + nx * (maximum - minimum);
}

FormulaCurveEditor::CurveValue FormulaCurveEditor::makeValue (float inputLinear,
                                                              float inputDb,
                                                              float sValue) const
{
    CurveValue value;

    value.inputLinear = std::max (0.0f, inputLinear);
    value.inputDb = juce::jlimit (-120.0f, 60.0f, inputDb);

    if (formulaDomain == 1)
    {
        const float outputDb = program != nullptr && program->valid
                                   ? program->eval (value.inputDb, sValue,
                                                    pValues[0], pValues[1],
                                                    pValues[2], pValues[3])
                                   : value.inputDb;

        if (! std::isnan (outputDb))
        {
            const bool silent = outputDb == -std::numeric_limits<float>::infinity();
            const float effectiveDb = silent ? -240.0f
                                             : (std::isinf (outputDb) ? 24.0f : outputDb);

            value.valid = true;
            value.outputDb = silent ? levelMinDb
                                    : juce::jlimit (levelMinDb, levelMaxDb, effectiveDb);
            value.outputLinear = silent ? 0.0f
                                        : dbToGain (std::clamp (effectiveDb, -240.0f, 24.0f));

            const float gainDb = std::clamp (effectiveDb - value.inputDb,
                                             -240.0f, 24.0f);
            value.gainDb = juce::jlimit (levelMinDb, levelMaxDb, gainDb);
            value.gainLinear = dbToGain (gainDb);
            value.boost = value.outputLinear > value.inputLinear + 1.0e-9f;
        }
    }
    else
    {
        const float output = program != nullptr && program->valid
                                 ? program->eval (value.inputLinear, sValue,
                                                  pValues[0], pValues[1],
                                                  pValues[2], pValues[3])
                                 : value.inputLinear;

        if (std::isfinite (output))
        {
            value.valid = true;
            value.outputLinear = output;
            value.outputDb = juce::jlimit (levelMinDb, levelMaxDb,
                                           gainToDb (std::abs (output)));

            value.gainLinear = value.inputLinear > 1.0e-6f
                                   ? output / value.inputLinear
                                   : (std::abs (output) < 1.0e-9f ? 0.0f : output / 1.0e-6f);

            if (! std::isfinite (value.gainLinear))
                value.gainLinear = 0.0f;

            value.gainDb = juce::jlimit (levelMinDb, levelMaxDb,
                                         gainToDb (std::abs (value.gainLinear)));
            value.inverted = output < 0.0f;
            value.boost = std::abs (output) > value.inputLinear + 1.0e-9f;
        }
    }

    return value;
}

float FormulaCurveEditor::inputValueFor (const Sample& sample) const
{
    return mode == ViewMode::Linear ? sample.inputLinear : sample.inputDb;
}

float FormulaCurveEditor::outputValueFor (const Sample& sample) const
{
    switch (mode)
    {
        case ViewMode::Level:  return sample.outputDb;
        case ViewMode::Gain:   return sample.gainDb;
        case ViewMode::Linear: return std::min (linearMax, std::abs (sample.outputLinear));
    }

    return sample.outputDb;
}

float FormulaCurveEditor::unityValueFor (float inputValue) const
{
    if (mode == ViewMode::Gain)
        return 0.0f;

    return inputValue;
}

void FormulaCurveEditor::rebuildCurve()
{
    const auto plot = computeLayout().plot;

    samples.clear();
    curvePath.clear();
    invertedPath.clear();
    boostFill.clear();
    reduceFill.clear();
    invalidInputs.clear();
    hasCurve = false;
    hasInverted = false;

    if (plot.getWidth() < 20.0f || plot.getHeight() < 20.0f)
        return;

    const bool linear = mode == ViewMode::Linear;
    const float minimum = linear ? linearMin : levelMinDb;
    const float maximum = linear ? linearMax : levelMaxDb;
    const int count = juce::jlimit (128, 512, juce::roundToInt (plot.getWidth()));

    samples.reserve (static_cast<std::size_t> (count));

    for (int i = 0; i < count; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (count - 1);
        const float viewValue = minimum + t * (maximum - minimum);

        float inputLinear;
        float inputDb;

        if (linear)
        {
            inputLinear = viewValue;
            inputDb = inputLinear > 1.0e-6f ? gainToDb (inputLinear) : -120.0f;
        }
        else
        {
            inputDb = viewValue;
            inputLinear = dbToGain (viewValue);
        }

        const auto value = makeValue (inputLinear, inputDb, sidechainValue);

        Sample sample;
        sample.inputLinear = value.inputLinear;
        sample.inputDb = value.inputDb;
        sample.outputLinear = value.outputLinear;
        sample.outputDb = value.outputDb;
        sample.gainLinear = value.gainLinear;
        sample.gainDb = value.gainDb;
        sample.valid = value.valid;
        sample.inverted = value.inverted;
        sample.boost = value.boost;
        samples.push_back (sample);

        if (! value.valid)
            invalidInputs.push_back (viewValue);
    }

    // Shaded areas between the curve and unity, split by boost / reduction.
    for (std::size_t i = 0; i < samples.size();)
    {
        const auto& first = samples[i];

        if (! first.valid)
        {
            ++i;
            continue;
        }

        const bool boost = first.boost;
        const std::size_t begin = i;

        while (i < samples.size() && samples[i].valid && samples[i].boost == boost)
            ++i;

        const std::size_t end = i;

        if (end - begin < 2)
            continue;

        juce::Path polygon;
        bool started = false;

        for (std::size_t k = begin; k < end; ++k)
        {
            const auto point = toScreen (inputValueFor (samples[k]),
                                         outputValueFor (samples[k]), plot);

            if (! started)
            {
                polygon.startNewSubPath (point);
                started = true;
            }
            else
            {
                polygon.lineTo (point);
            }
        }

        for (std::size_t k = end; k-- > begin;)
        {
            const auto inputValue = inputValueFor (samples[k]);
            polygon.lineTo (toScreen (inputValue, unityValueFor (inputValue), plot));
        }

        polygon.closeSubPath();
        (boost ? boostFill : reduceFill).addPath (polygon);
    }

    // Line work: solid cobalt for positive output, dashed cobalt for inversion.
    bool penDown = false;
    bool invertedPenDown = false;

    for (const auto& sample : samples)
    {
        if (! sample.valid)
        {
            penDown = false;
            invertedPenDown = false;
            continue;
        }

        const auto point = toScreen (inputValueFor (sample),
                                     outputValueFor (sample), plot);

        if (sample.inverted)
        {
            if (! invertedPenDown)
            {
                invertedPath.startNewSubPath (point);
                invertedPenDown = true;
            }
            else
            {
                invertedPath.lineTo (point);
            }

            penDown = false;
            hasInverted = true;
        }
        else
        {
            if (! penDown)
            {
                curvePath.startNewSubPath (point);
                penDown = true;
            }
            else
            {
                curvePath.lineTo (point);
            }

            invertedPenDown = false;
            hasCurve = true;
        }
    }

    repaint();
}

void FormulaCurveEditor::drawGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const bool linear = mode == ViewMode::Linear;
    const float minimum = linear ? linearMin : levelMinDb;
    const float maximum = linear ? linearMax : levelMaxDb;

    g.setFont (uiFont (9.0f));

    const int steps = 6;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (steps);
        const float value = minimum + t * (maximum - minimum);

        const float x = plot.getX() + t * plot.getWidth();
        const float y = plot.getBottom() - t * plot.getHeight();

        g.setColour (grid);
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);

        g.setColour (secondary);
        g.drawText (tickText (value, linear),
                    juce::Rectangle<float> (x - 24.0f, plot.getBottom() + 3.0f, 48.0f, 14.0f),
                    juce::Justification::centred, false);

        g.drawText (tickText (value, linear),
                    juce::Rectangle<float> (plot.getX() - 22.0f, y - 7.0f, 17.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }
}

juce::String FormulaCurveEditor::getModeTitle() const
{
    switch (mode)
    {
        case ViewMode::Level:  return "TRANSFER  |  LEVEL";
        case ViewMode::Gain:   return "GAIN  |  f(x,s)/x";
        case ViewMode::Linear: return "TRANSFER  |  LINEAR";
    }

    return {};
}

void FormulaCurveEditor::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    const auto panelArea = getLocalBounds().toFloat();

    g.setColour (panel);
    g.fillRoundedRectangle (panelArea, 8.0f);

    const auto layout = computeLayout();
    const auto plot = layout.plot;
    const bool linear = mode == ViewMode::Linear;
    const float minimum = linear ? linearMin : levelMinDb;
    const float maximum = linear ? linearMax : levelMaxDb;

    const auto viewGroup = modeButtons[0].getBounds().getUnion (
        modeButtons[2].getBounds());

    if (! viewGroup.isEmpty())
    {
        g.setColour (panelAlt);
        g.fillRoundedRectangle (viewGroup.toFloat().expanded (2.0f, 0.0f), 4.0f);
    }

    g.setColour (ink);
    g.setFont (uiFont (formula::type::annotation));
    g.drawFittedText (getModeTitle(),
                      juce::Rectangle<float> (layout.header.getX(), layout.header.getY(),
                                              juce::jmax (60.0f,
                                                          layout.header.getRight()
                                                              - 178.0f
                                                              - layout.header.getX()),
                                              layout.header.getHeight())
                          .toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.75f);

    g.setColour (paperBright);
    g.fillRoundedRectangle (plot, 4.0f);

    g.setColour (teal.withAlpha (0.14f));
    g.fillPath (boostFill);

    g.setColour (vermilion.withAlpha (0.13f));
    g.fillPath (reduceFill);

    drawGrid (g, plot);

    // Unity: diagonal in LEVEL / LINEAR, horizontal 0 dB line in GAIN.
    g.setColour (gridStrong);
    g.drawLine ({ toScreen (minimum, unityValueFor (minimum), plot),
                  toScreen (maximum, unityValueFor (maximum), plot) }, 1.3f);

    g.setFont (uiFont (9.5f));
    g.setColour (secondary);

    const float unityY = mode == ViewMode::Gain
                             ? toScreen (minimum, 0.0f, plot).y - 14.0f
                             : plot.getY() + 4.0f;

    g.drawText ("UNITY",
                juce::Rectangle<float> (plot.getRight() - 46.0f,
                                        juce::jlimit (plot.getY(), plot.getBottom() - 12.0f, unityY),
                                        42.0f, 12.0f),
                juce::Justification::centredRight, false);

    if (hasCurve)
    {
        g.setColour (cobalt);
        g.strokePath (curvePath, juce::PathStrokeType (2.0f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    if (hasInverted)
    {
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType stroke (2.0f, juce::PathStrokeType::curved,
                                     juce::PathStrokeType::rounded);
        juce::Path dashed;
        stroke.createDashedStroke (dashed, invertedPath, dashes, 2);

        g.setColour (cobalt);
        g.fillPath (dashed);
    }

    for (const auto inputValue : invalidInputs)
    {
        const auto point = toScreen (inputValue, minimum, plot);
        g.setColour (vermilion.withAlpha (0.3f));
        g.drawLine (point.x, plot.getY(), point.x, plot.getBottom(), 1.0f);
    }

    // Live operating point.
    {
        const float inputLinear = liveInputLinear;
        const float inputDb = inputLinear > 1.0e-6f ? gainToDb (inputLinear) : -120.0f;
        const auto value = makeValue (inputLinear, inputDb, sidechainValue);

        if (value.valid && inputLinear > 1.0e-6f)
        {
            const float inputValue = mode == ViewMode::Linear ? inputLinear : inputDb;
            const float outputValue = mode == ViewMode::Level
                                          ? value.outputDb
                                          : (mode == ViewMode::Gain
                                                 ? value.gainDb
                                                 : std::min (linearMax,
                                                             std::abs (value.outputLinear)));

            if (inputValue >= minimum - 0.01f)
            {
                const auto point = toScreen (inputValue, outputValue, plot);
                const auto colour = value.boost ? teal : vermilion;

                g.setColour (paperBright);
                g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (point));
                g.setColour (colour);
                g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (point), 2.0f);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (point));
            }
        }
    }

    // Hover crosshair.
    if (hovering && plot.contains (hoverPosition))
    {
        const float viewValue = screenToInputValue (hoverPosition.x, plot);
        float inputLinear;
        float inputDb;

        if (linear)
        {
            inputLinear = viewValue;
            inputDb = inputLinear > 1.0e-6f ? gainToDb (inputLinear) : -120.0f;
        }
        else
        {
            inputDb = viewValue;
            inputLinear = dbToGain (viewValue);
        }

        const auto value = makeValue (inputLinear, inputDb, sidechainValue);

        if (value.valid)
        {
            const float outputValue = mode == ViewMode::Level
                                          ? value.outputDb
                                          : (mode == ViewMode::Gain
                                                 ? value.gainDb
                                                 : std::min (linearMax,
                                                             std::abs (value.outputLinear)));

            const auto point = toScreen (viewValue, outputValue, plot);

            g.setColour (ink.withAlpha (0.25f));
            g.drawLine (hoverPosition.x, plot.getY(), hoverPosition.x, plot.getBottom(), 1.0f);
            g.drawLine (plot.getX(), point.y, plot.getRight(), point.y, 1.0f);

            g.setColour (value.boost ? teal : vermilion);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (point));
        }
    }

    g.setColour (border);
    g.drawRoundedRectangle (plot, 4.0f, 1.0f);
}

void FormulaCurveEditor::timerCallback()
{
    if (processorRef.isUiFrozen())
        return;

    const auto active = processorRef.getActiveProgram();
    bool needsRebuild = active.get() != program.get();

    const int domain = processorRef.getFormulaDomain();

    if (domain != formulaDomain)
    {
        formulaDomain = domain;
        sidechainValue = processorRef.getSidechainValueForFormula();
        needsRebuild = true;
    }

    const float newSidechainValue = processorRef.getSidechainValueForFormula();
    const bool newSidechainActive = processorRef.isSidechainActiveForDisplay();

    if (newSidechainActive != sidechainActive
        || std::abs (newSidechainValue - sidechainValue) > 1.0e-4f)
    {
        sidechainValue = newSidechainValue;
        sidechainActive = newSidechainActive;

        if (active != nullptr && active->usesSidechain)
            needsRebuild = true;
    }

    for (int i = 0; i < 4; ++i)
    {
        const float value = processorRef.getPValue (i + 1);

        if (std::abs (value - pValues[static_cast<std::size_t> (i)]) > 1.0e-5f)
        {
            pValues[static_cast<std::size_t> (i)] = value;
            needsRebuild = true;
        }
    }

    if (needsRebuild)
    {
        program = active;
        rebuildCurve();
    }

    liveInputLinear = processorRef.getDetectorLevel();
    repaint();
}

void FormulaCurveEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (processorRef.isUiFrozen())
        return;

    program = processorRef.getActiveProgram();
    formulaDomain = processorRef.getFormulaDomain();
    liveInputLinear = processorRef.getDetectorLevel();
    sidechainValue = processorRef.getSidechainValueForFormula();
    sidechainActive = processorRef.isSidechainActiveForDisplay();

    for (int i = 0; i < 4; ++i)
        pValues[static_cast<std::size_t> (i)] = processorRef.getPValue (i + 1);

    rebuildCurve();
}

void FormulaCurveEditor::mouseMove (const juce::MouseEvent& event)
{
    hovering = computeLayout().plot.contains (event.position);
    hoverPosition = event.position;
    repaint();
}

void FormulaCurveEditor::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}
