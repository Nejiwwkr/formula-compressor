#include "ScopePanel.h"

#include "../PluginProcessor.h"
#include "Theme.h"

#include <algorithm>
#include <cmath>

namespace
{
using namespace formula::theme;

juce::Font uiFont (float height)
{
    return juce::Font (juce::FontOptions (height));
}

constexpr float levelMinDb = -60.0f;
constexpr float levelMaxDb = 12.0f;

float peakToDb (float minimum, float maximum)
{
    const float peak = std::max (std::abs (minimum), std::abs (maximum));
    return 20.0f * std::log10 (std::max (1.0e-6f, peak));
}
} // namespace

ScopePanel::ScopePanel (FormulaCompressorAudioProcessor& processorIn)
    : processorRef (processorIn)
{
    setOpaque (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to freeze the transfer, time and level views. "
                "Input/output levels and gain reduction share the transfer plot's dB axis; "
                "Mix and Makeup are not included.");
    startTimerHz (30);
}

void ScopePanel::collectFrames()
{
    frames.clear();

    const int capacity = FormulaCompressorAudioProcessor::getScopePointCount();
    const int count = juce::jlimit (0, capacity, processorRef.getScopeCount());

    if (count <= 0)
        return;

    const int write = processorRef.getScopeWriteIndex();
    const int start = (write - count + capacity) % capacity;

    frames.reserve (static_cast<std::size_t> (count));

    for (int i = 0; i < count; ++i)
    {
        const int index = (start + i) % capacity;

        Frame frame;
        frame.inputMin = processorRef.getScopeInputMin (index);
        frame.inputMax = processorRef.getScopeInputMax (index);
        frame.outputMin = processorRef.getScopeOutputMin (index);
        frame.outputMax = processorRef.getScopeOutputMax (index);
        frame.gainDb = processorRef.getScopeGainDb (index);
        frames.push_back (frame);
    }
}

void ScopePanel::timerCallback()
{
    if (processorRef.isUiFrozen())
        return;

    repaint();
}

void ScopePanel::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isLeftButtonDown())
        return;

    const bool freeze = ! processorRef.isUiFrozen();

    if (freeze)
    {
        collectFrames();
        frozenFrames = frames;
        setTooltip ("Frozen. Click to resume.");
    }
    else
    {
        frozenFrames.clear();
        setTooltip ("Click to freeze the transfer, time and level views.");
    }

    processorRef.setUiFrozen (freeze);
    repaint();
}

void ScopePanel::paint (juce::Graphics& g)
{
    g.fillAll (paper);

    const auto card = getLocalBounds().toFloat();

    g.setColour (panel);
    g.fillRoundedRectangle (card, 8.0f);

    auto content = card.reduced (10.0f, 8.0f);
    auto header = content.removeFromTop (32.0f);
    content.removeFromBottom (24.0f);

    const auto plot = juce::Rectangle<float> (content.getX() + 22.0f,
                                              content.getY(),
                                              content.getWidth() - 30.0f,
                                              content.getHeight());

    g.setFont (uiFont (formula::type::annotation));
    g.setColour (secondary);
    g.drawText ("TIME",
                juce::Rectangle<float> (header.getX(), header.getY(),
                                        header.getRight() - header.getX(),
                                        header.getHeight()),
                juce::Justification::centredLeft, false);

    if (processorRef.isUiFrozen())
    {
        g.setFont (uiFont (9.5f));
        g.setColour (vermilion);
        g.drawText ("FROZEN",
                    juce::Rectangle<float> (header.getX() + 45.0f, header.getY(),
                                            60.0f, header.getHeight()),
                    juce::Justification::centredLeft, false);
        g.setFont (uiFont (formula::type::annotation));
    }

    auto headerRight = header.withRight (plot.getRight());
    g.setFont (uiFont (9.5f));
    g.setColour (muted);
    g.drawText ("3 s", headerRight.removeFromRight (26.0f),
                juce::Justification::centredRight, false);
    headerRight.removeFromRight (8.0f);
    g.setColour (cobalt);
    g.drawText ("GR", headerRight.removeFromRight (20.0f),
                juce::Justification::centredRight, false);

    g.setColour (paperBright);
    g.fillRoundedRectangle (plot, 4.0f);

    // Same dB axis and vertical margins as the transfer plot.
    g.setFont (uiFont (9.0f));

    for (int db = static_cast<int> (levelMinDb);
         db <= static_cast<int> (levelMaxDb);
         db += 12)
    {
        const float t = (static_cast<float> (db) - levelMinDb)
                        / (levelMaxDb - levelMinDb);
        const float y = plot.getBottom() - t * plot.getHeight();

        g.setColour (grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (secondary);
        g.drawText (juce::String (db),
                    juce::Rectangle<float> (plot.getX() - 22.0f, y - 7.0f, 17.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }

    const bool frozen = processorRef.isUiFrozen();

    if (! frozen)
        collectFrames();

    const auto& display = frozen ? frozenFrames : frames;
    const int capacity = FormulaCompressorAudioProcessor::getScopePointCount();

    if (display.size() < 2)
    {
        g.setFont (uiFont (10.5f));
        g.setColour (muted);
        g.drawText ("waiting for audio", plot, juce::Justification::centred, false);

        g.setColour (border);
        g.drawRoundedRectangle (plot, 4.0f, 1.0f);
        return;
    }

    const float spacing = plot.getWidth() / static_cast<float> (juce::jmax (1, capacity - 1));

    const auto toY = [] (float db, juce::Rectangle<float> area)
    {
        const float t = (juce::jlimit (levelMinDb, levelMaxDb, db) - levelMinDb)
                        / (levelMaxDb - levelMinDb);
        return area.getBottom() - t * area.getHeight();
    };

    const auto fillBand = [&] (float x, float width, float lowDb, float highDb,
                               juce::Colour colour)
    {
        if (highDb <= lowDb + 0.01f)
            return;

        const float top = toY (highDb, plot);
        const float bottom = toY (lowDb, plot);

        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, top, width, bottom - top));
    };

    juce::Path gainPath;
    bool gainStarted = false;

    const float columnWidth = juce::jmax (1.0f, spacing + 0.5f);

    for (std::size_t i = 0; i < display.size(); ++i)
    {
        const auto& frame = display[i];
        const float x = plot.getRight() - spacing
                        * static_cast<float> (display.size() - 1 - i);

        const float inputDb = juce::jlimit (levelMinDb, levelMaxDb,
                                            peakToDb (frame.inputMin, frame.inputMax));
        const float outputDb = juce::jlimit (levelMinDb, levelMaxDb,
                                             peakToDb (frame.outputMin, frame.outputMax));

        // Three regions: common body, removed input, added output.
        fillBand (x, columnWidth, levelMinDb, std::min (inputDb, outputDb),
                  ink.withAlpha (0.07f));

        if (inputDb > outputDb)
            fillBand (x, columnWidth, outputDb, inputDb, vermilion.withAlpha (0.30f));
        else if (outputDb > inputDb)
            fillBand (x, columnWidth, inputDb, outputDb, teal.withAlpha (0.30f));

        const float gainY = toY (frame.gainDb, plot);

        if (! gainStarted)
        {
            gainPath.startNewSubPath (x, gainY);
            gainStarted = true;
        }
        else
        {
            gainPath.lineTo (x, gainY);
        }
    }

    g.setColour (cobalt);
    g.strokePath (gainPath, juce::PathStrokeType (1.6f));

    g.setColour (border);
    g.drawRoundedRectangle (plot, 4.0f, 1.0f);
}
