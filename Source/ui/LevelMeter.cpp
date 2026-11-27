#include "LevelMeter.h"

#include "../PluginProcessor.h"
#include "Theme.h"

#include <cmath>

namespace
{
using namespace formula::theme;

juce::Font uiFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

constexpr float meterMinDb = -60.0f;
constexpr float meterMaxDb = 12.0f;
} // namespace

LevelMeter::LevelMeter (FormulaCompressorAudioProcessor& processorIn)
    : processorRef (processorIn)
{
    setTooltip ("Input and output peak levels and the gain curve, on the same "
                "dB axis as the transfer plot.");
    startTimerHz (30);
}

void LevelMeter::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    const auto card = getLocalBounds().toFloat();

    g.setColour (panel);
    g.fillRoundedRectangle (card, 8.0f);

    auto content = card.reduced (10.0f, 8.0f);
    auto header = content.removeFromTop (32.0f);
    auto footer = content.removeFromBottom (24.0f);

    const auto plot = juce::Rectangle<float> (content.getX() + 22.0f,
                                              content.getY(),
                                              content.getWidth() - 30.0f,
                                              content.getHeight());

    const float inputTarget = processorRef.getInputLevelDb();
    const float outputTarget = processorRef.getOutputLevelDb();
    const float reductionTarget = juce::jlimit (meterMinDb, meterMaxDb,
                                                processorRef.getGainReductionDb());

    if (! primed)
    {
        input.db = input.holdDb = inputTarget;
        output.db = output.holdDb = outputTarget;
        reduction.db = reduction.holdDb = reductionTarget;
        primed = true;
    }
    else if (! processorRef.isUiFrozen())
    {
        update (input, inputTarget, false);
        update (output, outputTarget, false);
        update (reduction, reductionTarget, true);
    }

    g.setColour (secondary);
    g.setFont (uiFont (formula::type::annotation));
    g.drawText ("LEVELS",
                juce::Rectangle<float> (header.getX(), header.getY(),
                                        header.getRight() - header.getX(),
                                        header.getHeight()),
                juce::Justification::centredLeft, false);

    g.setColour (paperBright);
    g.fillRoundedRectangle (plot, 4.0f);

    // dB ticks share the vertical positions of the transfer and time plots.
    g.setFont (uiFont (9.0f));

    for (int db = static_cast<int> (meterMinDb);
         db <= static_cast<int> (meterMaxDb);
         db += 12)
    {
        const float t = (static_cast<float> (db) - meterMinDb)
                        / (meterMaxDb - meterMinDb);
        const float y = plot.getBottom() - t * plot.getHeight();

        g.setColour (db == 0 ? gridStrong : grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (secondary);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (plot.getX() - 22.0f, y - 6.0f, 17.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    struct Column
    {
        juce::String label;
        float db;
        float holdDb;
        juce::Colour colour;
        bool fromUnity;
    };

    const Column columns[3] = {
        { "IN",  input.db,     input.holdDb,     secondary, false },
        { "OUT", output.db,    output.holdDb,    ink,       false },
        { "GR",  reduction.db, reduction.holdDb, cobalt,    true }
    };

    auto labelRow = footer.removeFromTop (11.0f);
    auto valueRow = footer;

    const float columnWidth = plot.getWidth() / 3.0f;
    const float barWidth = juce::jmin (18.0f, columnWidth * 0.55f);

    for (int i = 0; i < 3; ++i)
    {
        const auto& column = columns[i];

        auto cell = plot.withX (plot.getX() + columnWidth * static_cast<float> (i))
                        .withWidth (columnWidth);

        auto bar = cell.withSizeKeepingCentre (barWidth, plot.getHeight());

        drawBar (g, bar, column.db, column.holdDb, column.fromUnity, column.colour);

        auto labelCell = labelRow.withX (cell.getX()).withWidth (columnWidth);
        auto valueCell = valueRow.withX (cell.getX()).withWidth (columnWidth);

        g.setColour (column.colour);
        g.setFont (uiFont (9.0f));
        g.drawText (column.label, labelCell, juce::Justification::centred, false);

        juce::String value;

        if (column.label == "GR")
            value = column.db <= -99.0f ? juce::String ("-inf")
                                        : juce::String (column.db, 1);
        else
            value = column.db <= -99.0f ? juce::String ("-inf")
                                        : juce::String (juce::roundToInt (column.db));

        g.setFont (uiFont (9.0f));
        g.drawText (value, valueCell, juce::Justification::centred, false);
    }

    g.setColour (border);
    g.drawRoundedRectangle (plot, 4.0f, 1.0f);
}

void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> bounds,
                          float db, float holdDb, bool fromUnity, juce::Colour fill)
{
    if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
        return;

    g.setColour (paper);
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    const auto yFor = [] (float value, juce::Rectangle<float> area)
    {
        const float t = (juce::jlimit (meterMinDb, meterMaxDb, value) - meterMinDb)
                        / (meterMaxDb - meterMinDb);
        return area.getBottom() - t * area.getHeight();
    };

    auto inner = bounds.reduced (2.5f, 1.0f);

    float top = inner.getY();
    float bottom = inner.getBottom();

    if (fromUnity)
    {
        const float unityY = yFor (0.0f, inner);
        const float valueY = yFor (db, inner);
        top = juce::jmin (unityY, valueY);
        bottom = juce::jmax (unityY, valueY);
    }
    else
    {
        top = yFor (db, inner);
        bottom = inner.getBottom();
    }

    top = juce::jmax (top, inner.getY());
    bottom = juce::jmin (bottom, inner.getBottom());

    if (bottom > top + 0.5f)
    {
        g.setColour (fill.withAlpha (0.82f));
        g.fillRect (juce::Rectangle<float> (inner.getX(), top,
                                            inner.getWidth(), bottom - top));
    }

    if (holdDb > meterMinDb + 0.5f)
    {
        const float y = juce::jlimit (inner.getY(), inner.getBottom(),
                                      yFor (holdDb, inner));
        g.setColour (fill.brighter (0.25f));
        g.fillRect (juce::Rectangle<float> (inner.getX(), y - 1.0f,
                                            inner.getWidth(), 1.5f));
    }
}

void LevelMeter::update (Ballistics& ballistics, float targetDb, bool isReduction)
{
    const bool rising = isReduction ? targetDb < ballistics.db : targetDb > ballistics.db;
    const float coefficient = rising ? 0.65f : 0.10f;

    ballistics.db += (targetDb - ballistics.db) * coefficient;

    const bool newPeak = isReduction ? targetDb < ballistics.holdDb
                                     : targetDb > ballistics.holdDb;

    if (newPeak)
    {
        ballistics.holdDb = targetDb;
        ballistics.holdFrames = 30;
    }
    else if (ballistics.holdFrames > 0)
    {
        --ballistics.holdFrames;
    }
    else
    {
        ballistics.holdDb += (targetDb - ballistics.holdDb) * 0.06f;
    }
}

void LevelMeter::timerCallback()
{
    if (processorRef.isUiFrozen())
        return;

    repaint();
}
