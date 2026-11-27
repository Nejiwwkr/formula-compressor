#include "TestFramework.h"

#include "PluginProcessor.h"
#include "dsp/ExpressionParser.h"
#include "state/PresetManager.h"

#include <cmath>
#include <memory>

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 512;
const juce::String ampClassic = "min(x, P1 + (x - P1) * P2)";

void prepare (FormulaCompressorAudioProcessor& processor)
{
    processor.prepareToPlay (testSampleRate, testBlockSize);
}

float processConstant (FormulaCompressorAudioProcessor& processor,
                       float value, int blocks, int channel = 0)
{
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    juce::MidiBuffer midi;

    for (int block = 0; block < blocks; ++block)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), value,
                                               testBlockSize);

        processor.processBlock (buffer, midi);
    }

    return buffer.getSample (channel, testBlockSize - 1);
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;

    return true;
}
} // namespace

void tests::testProcessor()
{
    std::printf ("Processor tests\n");

    auto processor = std::make_unique<FormulaCompressorAudioProcessor>();
    prepare (*processor);

    CHECK (processor->getName().isNotEmpty());
    CHECK (! processor->acceptsMidi());
    CHECK_CLOSE (processor->getTailLengthSeconds(), 0.0, 1.0e-9);
    CHECK (processor->getBypassParameter() != nullptr);

    // Bus layouts.
    {
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo());
        stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        CHECK (processor->isBusesLayoutSupported (stereo));

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (processor->isBusesLayoutSupported (mono));

        juce::AudioProcessor::BusesLayout quad;
        quad.inputBuses.add (juce::AudioChannelSet::quadraphonic());
        quad.outputBuses.add (juce::AudioChannelSet::quadraphonic());
        CHECK (! processor->isBusesLayoutSupported (quad));
    }

    // The default is dBFS, compressing a steady 0.5 input to about 0.298.
    {
        CHECK (processor->getFormulaDomain() == 1);
        const float output = processConstant (*processor, 0.5f, 100);
        CHECK_CLOSE (output, 0.2983, 3.0e-3);
    }

    // Bypass.
    {
        processor->setParameterValue ("bypass", 1.0f);
        const float output = processConstant (*processor, 0.5f, 20);
        CHECK_CLOSE (output, 0.5, 1.0e-5);
        processor->setParameterValue ("bypass", 0.0f);
    }

    // Mix at zero is transparent.
    {
        processor->setParameterValue ("mix", 0.0f);
        const float output = processConstant (*processor, 0.5f, 20);
        CHECK_CLOSE (output, 0.5, 1.0e-6);
        processor->setParameterValue ("mix", 1.0f);
    }

    // Input gain drives both the detector path and the dry path.
    {
        processor->setParameterValue ("mix", 0.0f);
        processor->setParameterValue ("input", 6.0206f);

        const float output = processConstant (*processor, 0.25f, 80);
        CHECK_CLOSE (output, 0.5, 0.01);

        processor->setParameterValue ("bypass", 1.0f);
        const float bypassed = processConstant (*processor, 0.25f, 80);
        CHECK_CLOSE (bypassed, 0.25, 0.01);

        processor->setParameterValue ("bypass", 0.0f);
        processor->setParameterValue ("input", 0.0f);
        processor->setParameterValue ("mix", 1.0f);
    }

    // Invalid formulas keep the last valid program and report a column.
    {
        CHECK (processor->setFormula ("min(x, 0.2)").ok);
        const auto before = processor->getActiveProgram();
        CHECK (before != nullptr);

        const auto result = processor->setFormula ("min(");
        CHECK (! result.ok);
        CHECK (! processor->isFormulaValid());
        CHECK (processor->getFormulaErrorColumn() >= 1);
        CHECK (processor->getFormulaError().isNotEmpty());

        const auto after = processor->getActiveProgram();
        CHECK (after.get() == before.get());

        if (after != nullptr)
            CHECK_CLOSE (after->eval (0.5f, 0, 0, 0, 0), 0.2, 1.0e-5);

        CHECK (processor->setFormula (formula::defaultFormula()).ok);
    }

    // Clearing the editor is not replaced by the default text.
    {
        CHECK (processor->setFormula ("min(x, 0.5)").ok);
        const auto before = processor->getActiveProgram();

        const auto result = processor->setFormula ("");
        CHECK (! result.ok);
        CHECK (processor->getFormula().isEmpty());
        CHECK (processor->getActiveProgram().get() == before.get());

        CHECK (processor->setFormula (formula::defaultFormula()).ok);
    }

    // State round trip, including formula text and parameters.
    {
        CHECK (processor->setFormula ("min(x, 0.2)").ok);
        processor->setParameterValue ("p1", 0.7f);
        processor->setParameterValue ("detector", 1.0f);
        processor->setParameterValue ("attack", 25.0f);

        juce::MemoryBlock state;
        processor->getStateInformation (state);
        CHECK (state.getSize() > 0);

        auto restored = std::make_unique<FormulaCompressorAudioProcessor>();
        prepare (*restored);
        restored->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        CHECK (restored->getFormula() == "min(x, 0.2)");
        CHECK_CLOSE (restored->getPValue (1), 0.7, 1.0e-3);

        const auto* detector = restored->getValueTreeState()
                                   .getRawParameterValue ("detector");
        const auto* attack = restored->getValueTreeState()
                                 .getRawParameterValue ("attack");
        CHECK (detector != nullptr && juce::roundToInt (detector->load()) == 1);
        CHECK (attack != nullptr && std::abs (attack->load() - 25.0f) < 0.05f);
        CHECK (restored->isFormulaValid());
    }

    // Saved preset round trip.
    {
        const auto directory = juce::File::getSpecialLocation (
                                   juce::File::tempDirectory)
                                   .getChildFile ("FormulaCompressorTestsPresets");

        directory.deleteRecursively();
        PresetManager manager (*processor, directory);

        CHECK (processor->setFormula ("2 * x").ok);
        processor->setParameterValue ("p2", 0.5f);
        CHECK (manager.savePreset ("Round Trip"));

        CHECK (processor->setFormula ("x").ok);
        processor->setParameterValue ("p2", -0.5f);

        CHECK (manager.loadPreset ("Round Trip"));
        CHECK (processor->getFormula() == "2 * x");
        CHECK_CLOSE (processor->getPValue (2), 0.5, 1.0e-3);

        const auto names = manager.getUserPresetNames();
        CHECK (names.contains ("Round Trip"));

        directory.deleteRecursively();
    }

    // Every built-in preset is a valid, finite expression.
    {
        const auto& presets = PresetManager::getBuiltInPresets();
        CHECK (presets.size() >= 6);

        for (const auto& preset : presets)
        {
            const auto result = formula::compileExpression (preset.formula);
            CHECK_MESSAGE (result.ok, preset.name + ": " + result.error);

            if (result.ok)
            {
                const float value = result.program.eval (0.5f, preset.p[0],
                                                         preset.p[1], preset.p[2],
                                                         preset.p[3]);
                CHECK_MESSAGE (std::isfinite (value), preset.name + " is not finite");
            }
        }

        // Loading a built-in applies both its formula and its P values.
        const auto directory = juce::File::getSpecialLocation (
                                   juce::File::tempDirectory)
                                   .getChildFile ("FormulaCompressorTestsPresets");
        PresetManager manager (*processor, directory);

        manager.loadBuiltIn (2); // Hard Limit
        CHECK (processor->getFormula() == presets[2].formula);
        CHECK_CLOSE (processor->getPValue (1), presets[2].p[0], 1.0e-3);
        CHECK_CLOSE (processor->getPValue (2), presets[2].p[1], 1.0e-3);

        manager.loadBuiltIn (0);
        CHECK (processor->getFormula() == presets[0].formula);
        CHECK_CLOSE (processor->getPValue (1), presets[0].p[0], 1.0e-3);
        CHECK (processor->getFormulaDomain() == 0);

        CHECK (presets.size() >= 12);
        manager.loadBuiltIn (8); // Classic 4:1 (dB)
        CHECK (processor->getFormulaDomain() == 1);
        CHECK (processor->getFormula() == presets[8].formula);

        manager.loadBuiltIn (0);
    }

    // Negative formulas invert polarity and are exposed to the UI.
    {
        CHECK (processor->setFormula ("-x").ok);
        const float output = processConstant (*processor, 0.5f, 40);
        CHECK_CLOSE (output, -0.5, 2.0e-3);
        CHECK (processor->isGainInverted());

        CHECK (processor->setFormula ("0 * x").ok);
        const float silent = processConstant (*processor, 0.5f, 20);
        CHECK_CLOSE (silent, 0.0, 1.0e-6);
        CHECK (! processor->isGainInverted());

        CHECK (processor->setFormula (ampClassic).ok);
    }

    // x domain round trip.
    {
        processor->setFormulaDomain (1);
        CHECK (processor->getFormulaDomain() == 1);
        CHECK (processor->setFormula ("min(x, -12 + (x + 12) / 4)").ok);

        juce::MemoryBlock state;
        processor->getStateInformation (state);

        auto restored = std::make_unique<FormulaCompressorAudioProcessor>();
        prepare (*restored);
        restored->setStateInformation (state.getData(),
                                       static_cast<int> (state.getSize()));

        CHECK (restored->getFormulaDomain() == 1);
        CHECK (restored->getFormula() == "min(x, -12 + (x + 12) / 4)");

        processor->setFormulaDomain (0);
        CHECK (processor->setFormula (ampClassic).ok);
    }

    // Scope captures the formula-gain waveform, not Mix or Makeup.
    {
        CHECK (processor->setFormula (ampClassic).ok);
        processor->setEditorActive (true);

        processConstant (*processor, 0.5f, 80);

        const int capacity = FormulaCompressorAudioProcessor::getScopePointCount();
        CHECK (processor->getScopeCount() > 0);

        const int newest = (processor->getScopeWriteIndex() - 1 + capacity)
                           % capacity;
        CHECK_CLOSE (processor->getScopeInputMax (newest), 0.5, 0.01);
        CHECK_CLOSE (processor->getScopeOutputMax (newest), 0.3125, 0.02);
        CHECK (processor->getScopeGainDb (newest) < -3.0f);

        processor->setParameterValue ("makeup", 12.0f);
        processConstant (*processor, 0.5f, 40);

        const int newestAfterMakeup = (processor->getScopeWriteIndex() - 1 + capacity)
                                      % capacity;
        CHECK_CLOSE (processor->getScopeOutputMax (newestAfterMakeup), 0.3125, 0.03);

        processor->setParameterValue ("makeup", 0.0f);
        processor->setEditorActive (false);
    }

    // Metering only runs while an editor is open.
    {
        processor->setEditorActive (true);
        processConstant (*processor, 0.5f, 20);
        CHECK (processor->getInputLevelDb() > -20.0f);
        CHECK (processor->getDetectorLevel() > 0.0f);
        processor->setEditorActive (false);
    }

    // Stress: noise-like input across a few formulas stays finite.
    {
        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;
        juce::Random random (1234);

        const char* formulas[] = {
            "min(x, P1 + (x - P1) * P2)",
            "tanh(x * (1 + P1 * 4)) / tanh(1 + P1 * 4)",
            "1 / x",
            "exp(x * 100)",
            "x < P1 ? x * P2 : x"
        };

        for (const auto* text : formulas)
        {
            CHECK (processor->setFormula (text).ok);

            for (int block = 0; block < 60; ++block)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < testBlockSize; ++i)
                        buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

                processor->processBlock (buffer, midi);
                CHECK (allFinite (buffer));
            }
        }

        CHECK (processor->setFormula (ampClassic).ok);
    }
}
