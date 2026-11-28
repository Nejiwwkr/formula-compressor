#include "TestFramework.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
int writeScreenshot (const juce::String& path, const juce::String& modeName,
                     const juce::String& formulaText, const juce::String& domainName,
                     int width, int height, bool sidechain)
{
    auto processor = std::make_unique<FormulaCompressorAudioProcessor>();

    if (sidechain)
    {
        // Enable two aux buses so the readout shows live S1/S2 values and
        // "--" for the two buses the host left disabled.
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.inputBuses.add (juce::AudioChannelSet::disabled());
        layout.inputBuses.add (juce::AudioChannelSet::disabled());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());

        if (! processor->setBusesLayout (layout))
            std::printf ("Could not enable sidechain aux buses\n");

        processor->setParameterValue ("scEnabled", 1.0f);
    }

    processor->prepareToPlay (48000.0, 512);

    processor->setFormulaDomain (domainName.equalsIgnoreCase ("db") ? 1 : 0);

    if (formulaText.isNotEmpty())
    {
        const auto result = processor->setFormula (formulaText);

        if (! result.ok)
        {
            std::printf ("Formula error at column %d: %s\n",
                         result.errorColumn, result.error.toRawUTF8());
            return 1;
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

    if (editor == nullptr)
        return 1;

    if (auto* curve = dynamic_cast<FormulaCurveEditor*> (
            editor->findChildWithID ("formulaCurveEditor")))
    {
        if (modeName.equalsIgnoreCase ("gain"))
            curve->setMode (FormulaCurveEditor::ViewMode::Gain);
        else if (modeName.equalsIgnoreCase ("linear"))
            curve->setMode (FormulaCurveEditor::ViewMode::Linear);
        else
            curve->setMode (FormulaCurveEditor::ViewMode::Level);
    }

    editor->setSize (width, height);

    // Feed a short program-dependent signal so the scope has something to show.
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr int blocks = 225; // 2.4 s

        juce::AudioBuffer<float> buffer (processor->getTotalNumInputChannels(),
                                         blockSize);
        juce::MidiBuffer midi;

        auto main = processor->getBusBuffer (buffer, true, 0);
        // Two different aux sources so S1 and S2 differ.
        auto sc1 = sidechain ? processor->getBusBuffer (buffer, true, 1)
                             : juce::AudioBuffer<float>();
        auto sc2 = sidechain ? processor->getBusBuffer (buffer, true, 2)
                             : juce::AudioBuffer<float>();

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const double t = (block * blockSize + i) / sampleRate;
                const float envelope = 0.12f + 0.78f * static_cast<float> (
                    0.5 + 0.5 * std::sin (2.0 * 3.14159265358979 * 0.9 * t));
                const float value = envelope * static_cast<float> (
                    std::sin (2.0 * 3.14159265358979 * 220.0 * t)) * 0.9f;

                main.setSample (0, i, value);
                main.setSample (1, i, value * 0.9f);

                if (sidechain)
                {
                    const float sc1Value = envelope * 0.5f * static_cast<float> (
                        std::sin (2.0 * 3.14159265358979 * 110.0 * t));
                    const float sc2Value = envelope * 0.25f * static_cast<float> (
                        std::sin (2.0 * 3.14159265358979 * 330.0 * t));

                    sc1.setSample (0, i, sc1Value);
                    sc1.setSample (1, i, sc1Value);
                    sc2.setSample (0, i, sc2Value);
                    sc2.setSample (1, i, sc2Value);
                }
            }

            processor->processBlock (buffer, midi);
        }
    }

    // No message loop runs here, so the 10 Hz timers never tick. Re-enter the
    // ON handler for the readout, and broadcast synchronously so the curve
    // editor picks up the sidechain levels we just fed in.
    if (sidechain)
        if (auto* on = dynamic_cast<juce::Button*> (
                editor->findChildWithID ("scOnButton")))
            if (on->onClick)
                on->onClick();

    processor->getFormulaBroadcaster().sendSynchronousChangeMessage();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());

    auto file = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    file.deleteFile();

    auto stream = file.createOutputStream();

    if (stream == nullptr)
    {
        std::printf ("Could not write %s\n", file.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::PNGImageFormat png;
    png.writeImageToStream (image, *stream);

    std::printf ("Wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(),
                 image.getWidth(), image.getHeight());
    return 0;
}
} // namespace

namespace tests
{
int checks = 0;
int failures = 0;

void report (bool condition, const char* expression, const char* file, int line,
             const juce::String& detail)
{
    ++checks;

    if (condition)
        return;

    ++failures;
    std::printf ("FAIL  %s:%d\n      %s", file, line, expression);

    if (detail.isNotEmpty())
        std::printf ("\n      %s", detail.toRawUTF8());

    std::printf ("\n");
}

void reportClose (double actual, double expected, double tolerance,
                  const char* expression, const char* file, int line)
{
    const bool ok = std::abs (actual - expected) <= tolerance;

    report (ok, expression, file, line,
            "actual " + juce::String (actual, 6)
                + "  expected " + juce::String (expected, 6)
                + "  tolerance " + juce::String (tolerance, 6));
}
} // namespace tests

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::StringArray arguments;

    for (int i = 0; i < argc; ++i)
        arguments.add (argv[i]);

    int screenshotWidth = 1000;
    int screenshotHeight = 720;
    const int sizeIndex = arguments.indexOf ("--size");

    if (sizeIndex >= 0 && sizeIndex + 1 < arguments.size())
    {
        const auto size = arguments[sizeIndex + 1];
        const int separator = size.indexOfChar ('x');

        if (separator > 0)
        {
            screenshotWidth = juce::jlimit (600, 2400,
                                            size.substring (0, separator).getIntValue());
            screenshotHeight = juce::jlimit (500, 1600,
                                             size.substring (separator + 1).getIntValue());
            arguments.removeRange (sizeIndex, 2);
        }
    }

    const bool screenshotSidechain = arguments.contains ("--sidechain");
    arguments.removeString ("--sidechain");

    const int screenshotIndex = arguments.indexOf ("--screenshot");

    if (screenshotIndex >= 0 && screenshotIndex + 1 < arguments.size())
    {
        const auto path = arguments[screenshotIndex + 1];
        juce::String modeName = "level";
        juce::String formulaText;
        juce::String domainName = "db";

        int argument = screenshotIndex + 2;

        if (argument < arguments.size())
        {
            const auto candidate = arguments[argument];

            if (candidate.equalsIgnoreCase ("level")
                || candidate.equalsIgnoreCase ("gain")
                || candidate.equalsIgnoreCase ("linear"))
            {
                modeName = candidate;
                ++argument;
            }
        }

        if (argument < arguments.size())
        {
            formulaText = arguments[argument];
            ++argument;
        }

        if (argument < arguments.size())
        {
            const auto candidate = arguments[argument];

            if (candidate.equalsIgnoreCase ("db")
                || candidate.equalsIgnoreCase ("amp"))
                domainName = candidate;
        }

        return writeScreenshot (path, modeName, formulaText, domainName,
                                screenshotWidth, screenshotHeight,
                                screenshotSidechain);
    }

    std::printf ("Formula Compressor tests\n");

    tests::testExpression();
    tests::testEngine();
    tests::testProcessor();

    std::printf ("\n%d checks, %d failure%s\n",
                 tests::checks,
                 tests::failures,
                 tests::failures == 1 ? "" : "s");

    return tests::failures == 0 ? 0 : 1;
}
