#include "TestFramework.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "dsp/ExpressionParser.h"
#include "state/PresetManager.h"

#include <cmath>
#include <limits>
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

        const auto withSidechain = [] (juce::AudioChannelSet main,
                                       juce::AudioChannelSet sc1)
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (main);
            layout.inputBuses.add (sc1);
            layout.outputBuses.add (main);
            return layout;
        };

        CHECK (processor->isBusesLayoutSupported (
            withSidechain (juce::AudioChannelSet::stereo(),
                           juce::AudioChannelSet::stereo())));

        CHECK (processor->isBusesLayoutSupported (
            withSidechain (juce::AudioChannelSet::mono(),
                           juce::AudioChannelSet::mono())));

        CHECK (! processor->isBusesLayoutSupported (
            withSidechain (juce::AudioChannelSet::stereo(),
                           juce::AudioChannelSet::mono())));

        CHECK (! processor->isBusesLayoutSupported (
            withSidechain (juce::AudioChannelSet::stereo(),
                           juce::AudioChannelSet::quadraphonic())));
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

    // New dynamics controls round trip and state version is 5.
    {
        CHECK (FormulaCompressorAudioProcessor::getStateVersion() == 5);
        processor->setParameterValue ("scEnabled", 1.0f);
        processor->setParameterValue ("scHpf", 120.0f);
        processor->setParameterValue ("stereoLink", 0.3f);
        processor->setParameterValue ("monitor", 2.0f);

        juce::MemoryBlock state;
        processor->getStateInformation (state);

        auto restored = std::make_unique<FormulaCompressorAudioProcessor>();
        prepare (*restored);
        restored->setStateInformation (state.getData(),
                                       static_cast<int> (state.getSize()));

        const auto& tree = restored->getValueTreeState();
        CHECK_CLOSE (tree.getRawParameterValue ("scEnabled")->load(), 1.0, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("scHpf")->load(), 120.0, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("stereoLink")->load(), 0.3, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("monitor")->load(), 2.0, 1.0e-3);

        processor->setParameterValue ("scEnabled", 0.0f);
        processor->setParameterValue ("scHpf", 0.0f);
        processor->setParameterValue ("stereoLink", 1.0f);
        processor->setParameterValue ("monitor", 0.0f);
    }

    // A v3 state must reset the new controls to their defaults explicitly.
    {
        processor->setParameterValue ("scEnabled", 1.0f);
        processor->setParameterValue ("scHpf", 250.0f);
        processor->setParameterValue ("stereoLink", 0.2f);
        processor->setParameterValue ("monitor", 1.0f);

        auto oldState = processor->getValueTreeState().copyState();

        for (const auto* id : { "scEnabled", "scHpf", "stereoLink", "monitor" })
        {
            const auto child = oldState.getChildWithProperty ("id", id);

            if (child.isValid())
                oldState.removeChild (child, nullptr);
        }

        oldState.setProperty ("stateVersion", 3, nullptr);

        auto xml = oldState.createXml();
        CHECK (xml != nullptr);
        juce::MemoryBlock data;

        if (xml != nullptr)
            juce::AudioProcessor::copyXmlToBinary (*xml, data);

        processor->setStateInformation (data.getData(),
                                        static_cast<int> (data.getSize()));

        const auto& tree = processor->getValueTreeState();
        CHECK_CLOSE (tree.getRawParameterValue ("scEnabled")->load(), 0.0, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("scHpf")->load(), 0.0, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("stereoLink")->load(), 1.0, 1.0e-3);
        CHECK_CLOSE (tree.getRawParameterValue ("monitor")->load(), 0.0, 1.0e-3);
    }

    // A v4 state with the old scInput selector maps to OFF/ON and drops the old child.
    {
        auto oldState = processor->getValueTreeState().copyState();

        const auto newChild = oldState.getChildWithProperty ("id", "scEnabled");
        if (newChild.isValid())
            oldState.removeChild (newChild, nullptr);

        juce::ValueTree oldSc ("PARAM");
        oldSc.setProperty ("id", "scInput", nullptr);
        oldSc.setProperty ("value", 3.0f, nullptr);
        oldState.appendChild (oldSc, nullptr);
        oldState.setProperty ("stateVersion", 4, nullptr);

        auto xml = oldState.createXml();
        CHECK (xml != nullptr);
        juce::MemoryBlock data;

        if (xml != nullptr)
            juce::AudioProcessor::copyXmlToBinary (*xml, data);

        processor->setStateInformation (data.getData(),
                                        static_cast<int> (data.getSize()));
        CHECK (processor->isSidechainEnabled());
        CHECK (processor->getValueTreeState().copyState()
                   .getChildWithProperty ("id", "scInput").isValid() == false);

        processor->setParameterValue ("scEnabled", 0.0f);
    }

    // Pre-1.2 states without xDomain load as AMP.
    {
        processor->setFormulaDomain (1);
        CHECK (processor->setFormula ("min(x, 0.2)").ok);

        auto state = processor->getValueTreeState().copyState();
        state.removeProperty ("xDomain", nullptr);
        state.removeProperty ("stateVersion", nullptr);

        auto xml = state.createXml();
        CHECK (xml != nullptr);
        juce::MemoryBlock data;

        if (xml != nullptr)
            juce::AudioProcessor::copyXmlToBinary (*xml, data);

        processor->setStateInformation (data.getData(),
                                        static_cast<int> (data.getSize()));
        CHECK (processor->getFormulaDomain() == 0);
        CHECK (processor->getFormula() == "min(x, 0.2)");

        processor->setFormulaDomain (0);
        CHECK (processor->setFormula (ampClassic).ok);
    }

    // Unknown future state versions load best-effort.
    {
        CHECK (processor->setFormula ("2 * x").ok);

        auto state = processor->getValueTreeState().copyState();
        state.setProperty ("stateVersion", 99, nullptr);

        auto xml = state.createXml();
        CHECK (xml != nullptr);
        juce::MemoryBlock data;

        if (xml != nullptr)
            juce::AudioProcessor::copyXmlToBinary (*xml, data);

        processor->setStateInformation (data.getData(),
                                        static_cast<int> (data.getSize()));
        CHECK (processor->getFormula() == "2 * x");
        CHECK (processor->isFormulaValid());

        CHECK (processor->setFormula (ampClassic).ok);
    }

    // A state restore requests a reset that the audio thread consumes at the
    // start of the next block, never on the message thread.
    {
        processor->setFormulaDomain (1);
        CHECK (processor->setFormula ("x").ok);
        processor->setEditorActive (true);

        processConstant (*processor, 0.5f, 60);
        CHECK (processor->getDetectorLevel() > 0.1f);

        juce::MemoryBlock state;
        processor->getStateInformation (state);

        const float before = processor->getDetectorLevel();
        processor->setStateInformation (state.getData(),
                                        static_cast<int> (state.getSize()));
        CHECK_CLOSE (processor->getDetectorLevel(), before, 1.0e-6);

        processConstant (*processor, 0.0f, 1);
        CHECK_CLOSE (processor->getDetectorLevel(), 0.0, 1.0e-6);
        CHECK (processor->getInputLevelDb() <= -99.0f);

        processor->setEditorActive (false);
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

    // Non-finite input is sanitised for the scope and the output.
    {
        CHECK (processor->setFormula (ampClassic).ok);
        processor->setEditorActive (true);

        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < testBlockSize; ++i)
                buffer.setSample (ch, i,
                                  i % 3 == 0
                                      ? std::numeric_limits<float>::quiet_NaN()
                                      : (i % 3 == 1
                                             ? std::numeric_limits<float>::infinity()
                                             : -std::numeric_limits<float>::infinity()));

        processor->processBlock (buffer, midi);
        CHECK (allFinite (buffer));

        const int capacity = FormulaCompressorAudioProcessor::getScopePointCount();
        const int newest = (processor->getScopeWriteIndex() - 1 + capacity)
                           % capacity;

        CHECK (std::isfinite (processor->getScopeInputMin (newest)));
        CHECK (std::isfinite (processor->getScopeInputMax (newest)));
        CHECK (std::isfinite (processor->getScopeOutputMin (newest)));
        CHECK (std::isfinite (processor->getScopeOutputMax (newest)));
        CHECK (std::isfinite (processor->getScopeGainDb (newest)));

        processor->setEditorActive (false);
    }

    // Re-preparing at a different sample rate is safe.
    {
        processor->prepareToPlay (44100.0, testBlockSize);
        processConstant (*processor, 0.5f, 20);
        processor->prepareToPlay (96000.0, testBlockSize);

        const float output = processConstant (*processor, 0.5f, 20);
        CHECK (std::isfinite (output));

        processor->prepareToPlay (testSampleRate, testBlockSize);
    }

    // Mono processing uses the same engine path.
    {
        auto monoProcessor = std::make_unique<FormulaCompressorAudioProcessor>();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::mono());

        for (int i = 0; i < 4; ++i)
            layout.inputBuses.add (juce::AudioChannelSet::disabled());

        layout.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (monoProcessor->setBusesLayout (layout));
        monoProcessor->prepareToPlay (testSampleRate, testBlockSize);

        juce::AudioBuffer<float> buffer (1, testBlockSize);
        juce::FloatVectorOperations::fill (buffer.getWritePointer (0), 0.5f,
                                           testBlockSize);
        juce::MidiBuffer midi;
        monoProcessor->processBlock (buffer, midi);

        CHECK (allFinite (buffer));
        CHECK (buffer.getSample (0, testBlockSize - 1) > 0.0f);
    }

    // Four aux sidechains feed s1..s4 and total s; aux channels are never written.
    {
        auto scProcessor = std::make_unique<FormulaCompressorAudioProcessor>();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());

        for (int i = 0; i < 4; ++i)
            layout.inputBuses.add (juce::AudioChannelSet::stereo());

        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        CHECK (scProcessor->setBusesLayout (layout));
        scProcessor->prepareToPlay (testSampleRate, testBlockSize);
        scProcessor->setEditorActive (true);
        scProcessor->setFormulaDomain (1);
        CHECK (scProcessor->setFormula ("x - max(0, s + 60) / 4").ok);
        scProcessor->setParameterValue ("attack", 0.5f);
        scProcessor->setParameterValue ("scEnabled", 1.0f);

        juce::AudioBuffer<float> buffer (10, testBlockSize);
        juce::MidiBuffer midi;

        const auto fillInputs = [&] (bool enabled)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.5f,
                                                   testBlockSize);

            for (int bus = 0; bus < 4; ++bus)
            {
                const float level = enabled && bus < 2 ? 0.5f : 0.0f;

                for (int ch = 0; ch < 2; ++ch)
                    juce::FloatVectorOperations::fill (
                        buffer.getWritePointer (2 + bus * 2 + ch), level,
                        testBlockSize);
            }
        };

        for (int block = 0; block < 200; ++block)
        {
            fillInputs (true);
            scProcessor->processBlock (buffer, midi);
        }

        CHECK (buffer.getSample (0, testBlockSize - 1) < 0.3f);
        CHECK_CLOSE (buffer.getSample (2, 0), 0.5f, 1.0e-6);
        CHECK_CLOSE (buffer.getSample (4, 0), 0.5f, 1.0e-6);
        CHECK_CLOSE (buffer.getSample (6, 0), 0.0f, 1.0e-6);
        CHECK_CLOSE (buffer.getSample (8, 0), 0.0f, 1.0e-6);

        CHECK (scProcessor->isSidechainEnabled());
        CHECK (scProcessor->isSidechainAvailable (1));
        CHECK (scProcessor->isSidechainAvailable (2));
        CHECK (scProcessor->isSidechainActiveForDisplay());
        CHECK_CLOSE (scProcessor->getSidechainValueForFormula (1), -6.0206, 1.5);
        CHECK_CLOSE (scProcessor->getSidechainValueForFormula (2), -6.0206, 1.5);
        CHECK (scProcessor->getSidechainValueForFormula (3) < -100.0f);
        CHECK_CLOSE (scProcessor->getSidechainTotalForFormula(), 0.0, 1.5);

        // Muting one aux bus only changes that bus's sN and the total.
        for (int block = 0; block < 200; ++block)
        {
            fillInputs (true);
            juce::FloatVectorOperations::clear (buffer.getWritePointer (4),
                                                testBlockSize);
            juce::FloatVectorOperations::clear (buffer.getWritePointer (5),
                                                testBlockSize);
            scProcessor->processBlock (buffer, midi);
        }

        CHECK_CLOSE (scProcessor->getSidechainValueForFormula (1), -6.0206, 1.0);
        CHECK (scProcessor->getSidechainValueForFormula (2) < -100.0f);
        CHECK_CLOSE (scProcessor->getSidechainTotalForFormula(), -6.0206, 1.0);

        // OFF ignores the aux buses and clears the detector immediately.
        scProcessor->setParameterValue ("scEnabled", 0.0f);

        for (int block = 0; block < 100; ++block)
        {
            fillInputs (true);
            scProcessor->processBlock (buffer, midi);
        }

        CHECK_CLOSE (buffer.getSample (0, testBlockSize - 1), 0.5f, 0.03f);
        CHECK (! scProcessor->isSidechainEnabled());
        CHECK (scProcessor->getSidechainTotalForFormula() < -100.0f);
        CHECK_CLOSE (buffer.getSample (2, 0), 0.5f, 1.0e-6);
        scProcessor->setEditorActive (false);
    }

    // ON with no aux bus enabled: no readout, s is silence, audio stays finite.
    {
        auto plain = std::make_unique<FormulaCompressorAudioProcessor>();
        plain->prepareToPlay (testSampleRate, testBlockSize);
        plain->setEditorActive (true);
        plain->setParameterValue ("scEnabled", 1.0f);

        CHECK (plain->isSidechainEnabled());
        CHECK (! plain->isSidechainAvailable (1));
        CHECK (! plain->isSidechainActiveForDisplay());
        CHECK (plain->getSidechainTotalForFormula() < -100.0f);

        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < 20; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 0.5f,
                                                   testBlockSize);

            plain->processBlock (buffer, midi);
            CHECK (allFinite (buffer));
        }

        CHECK (plain->getSidechainTotalForFormula() < -100.0f);
        plain->setEditorActive (false);
    }

    // Editor layout: OFF collapses the header, ON expands it, at both sizes.
    {
        auto uiProcessor = std::make_unique<FormulaCompressorAudioProcessor>();
        uiProcessor->prepareToPlay (testSampleRate, testBlockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (uiProcessor->createEditor());
        CHECK (editor != nullptr);

        if (editor != nullptr)
        {
            const auto labelWithId = [&] (const char* id)
            {
                return dynamic_cast<juce::Label*> (editor->findChildWithID (id));
            };

            const int widths[] { 1000, 900 };
            const int heights[] { 720, 680 };

            for (int sizeIndex = 0; sizeIndex < 2; ++sizeIndex)
            {
                editor->setSize (widths[sizeIndex], heights[sizeIndex]);

                auto* prefix = labelWithId ("formulaLabel");
                auto* equals = labelWithId ("formulaEqualsLabel");
                auto* readout = labelWithId ("sidechainReadoutLabel");
                auto* formulaEditor = dynamic_cast<juce::TextEditor*> (
                    editor->findChildWithID ("formulaEditor"));

                CHECK (prefix != nullptr);
                CHECK (equals != nullptr);
                CHECK (readout != nullptr);
                CHECK (formulaEditor != nullptr);

                if (prefix == nullptr || equals == nullptr || readout == nullptr
                    || formulaEditor == nullptr)
                    continue;

                // OFF: short prefix, no s readout, wider editor.
                CHECK_MESSAGE (prefix->getText() == "f(x:",
                               "OFF prefix was " + prefix->getText());
                CHECK (! readout->isVisible());
                CHECK (! prefix->getText().contains ("s1"));

                const int offEditorWidth = formulaEditor->getWidth();
                const int offPrefixWidth = prefix->getWidth();

                // The "f(x:" label must not be elided: measured text plus margin.
                CHECK (prefix->getWidth()
                       >= juce::GlyphArrangement::getStringWidthInt (prefix->getFont(),
                                                                     prefix->getText()));
                CHECK (prefix->getHeight() <= 24);
                CHECK (equals->getWidth()
                       >= juce::GlyphArrangement::getStringWidthInt (equals->getFont(),
                                                                     equals->getText()));
                CHECK (equals->getHeight() <= 24);

                // ON: s1..s4 in the prefix, readout visible, editor narrower.
                if (auto* onButton = dynamic_cast<juce::Button*> (
                        editor->findChildWithID ("scOnButton")))
                {
                    if (onButton->onClick)
                        onButton->onClick();
                }

                CHECK (uiProcessor->isSidechainEnabled());
                CHECK_MESSAGE (prefix->getText().contains ("s1..4"),
                               "ON prefix was " + prefix->getText());
                CHECK (readout->isVisible());
                CHECK_MESSAGE (readout->getText().contains ("S1 ")
                                   && readout->getText().contains ("S "),
                               "readout was " + readout->getText());

                // The widest dBFS readout (all four buses at -120) must fit.
                const juce::String worstCase (
                    "S1 -120.0 S2 -120.0 S3 -120.0 S4 -120.0 S -120.0");
                CHECK_MESSAGE (juce::GlyphArrangement::getStringWidthInt (
                                   readout->getFont(), worstCase) <= readout->getWidth(),
                               "readout " + juce::String (readout->getWidth())
                                   + " px is too narrow for " + worstCase);
                CHECK (prefix->getWidth() > offPrefixWidth);
                CHECK (formulaEditor->getWidth() < offEditorWidth);
                CHECK (prefix->getWidth()
                       >= juce::GlyphArrangement::getStringWidthInt (prefix->getFont(),
                                                                     prefix->getText()));
                CHECK (prefix->getHeight() <= 24);
                CHECK (formulaEditor->getWidth() >= 380);

                // Back to OFF for the next size.
                if (auto* offButton = dynamic_cast<juce::Button*> (
                        editor->findChildWithID ("scOffButton")))
                {
                    if (offButton->onClick)
                        offButton->onClick();
                }

                CHECK (! uiProcessor->isSidechainEnabled());
                CHECK (! readout->isVisible());
            }
        }

        editor.reset();
        uiProcessor->setEditorActive (false);
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
