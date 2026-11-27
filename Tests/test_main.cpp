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
                     const juce::String& formulaText, const juce::String& domainName)
{
    auto processor = std::make_unique<FormulaCompressorAudioProcessor>();
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

    editor->setSize (1000, 720);

    // Feed a short program-dependent signal so the scope has something to show.
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr int blocks = 225; // 2.4 s

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const double t = (block * blockSize + i) / sampleRate;
                const float envelope = 0.12f + 0.78f * static_cast<float> (
                    0.5 + 0.5 * std::sin (2.0 * 3.14159265358979 * 0.9 * t));
                const float value = envelope * static_cast<float> (
                    std::sin (2.0 * 3.14159265358979 * 220.0 * t)) * 0.9f;

                buffer.setSample (0, i, value);
                buffer.setSample (1, i, value * 0.9f);
            }

            processor->processBlock (buffer, midi);
        }
    }

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

        return writeScreenshot (path, modeName, formulaText, domainName);
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
