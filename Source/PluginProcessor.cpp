#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::String msToString (float value)
{
    return (value < 10.0f ? juce::String (value, 2) : juce::String (value, 1)) + " ms";
}

juce::String percentToString (float value)
{
    return juce::String (juce::roundToInt (value * 100.0f)) + "%";
}

juce::String dbToString (float value)
{
    return juce::String (value, 1) + " dB";
}

juce::String numberToString (float value)
{
    return juce::String (value, 3);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
FormulaCompressorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 },
        "Attack",
        juce::NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.35f),
        10.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return msToString (value);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 },
        "Release",
        juce::NormalisableRange<float> (5.0f, 2000.0f, 0.1f, 0.35f),
        150.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return msToString (value);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "hold", 1 },
        "Hold",
        juce::NormalisableRange<float> (0.0f, 500.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return msToString (value);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "input", 1 },
        "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return dbToString (value);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return percentToString (value);
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return juce::jlimit (0.0f, 1.0f, text.getFloatValue() * 0.01f);
            })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "makeup", 1 },
        "Makeup",
        juce::NormalisableRange<float> (-12.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
            {
                return dbToString (value);
            })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "detector", 1 },
        "Detector",
        juce::StringArray { "Peak", "RMS" },
        0,
        juce::AudioParameterChoiceAttributes().withStringFromValueFunction (
            [] (int index, int) { return index == 1 ? "RMS" : "Peak"; })));

    const float pDefaults[4] = { 0.25f, 0.25f, 0.0f, 0.0f };

    for (int i = 0; i < 4; ++i)
    {
        const auto number = juce::String (i + 1);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "p" + number, 1 },
            "P" + number,
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
            pDefaults[i],
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float value, int)
                {
                    return numberToString (value);
                })));
    }

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [] (bool value, int) { return value ? "On" : "Off"; })));

    return layout;
}

juce::String FormulaCompressorAudioProcessor::getDefaultFormulaText()
{
    return formula::defaultFormula();
}

FormulaCompressorAudioProcessor::FormulaCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    attackParam = apvts.getRawParameterValue ("attack");
    releaseParam = apvts.getRawParameterValue ("release");
    holdParam = apvts.getRawParameterValue ("hold");
    inputGainParam = apvts.getRawParameterValue ("input");
    mixParam = apvts.getRawParameterValue ("mix");
    makeupParam = apvts.getRawParameterValue ("makeup");
    detectorParam = apvts.getRawParameterValue ("detector");
    bypassParam = apvts.getRawParameterValue ("bypass");

    for (int i = 0; i < 4; ++i)
        pParams[i] = apvts.getRawParameterValue ("p" + juce::String (i + 1));

    apvts.state.setProperty ("formula", getDefaultFormulaText(), nullptr);
    apvts.state.setProperty ("xDomain", 1, nullptr);
    formulaDomain.store (1, std::memory_order_relaxed);

    {
        const juce::ScopedLock lock (programLock);
        lastCompile = formula::compileExpression (getDefaultFormulaText());

        if (lastCompile.ok)
            publishProgramLocked (lastCompile.program);
    }
}

FormulaCompressorAudioProcessor::~FormulaCompressorAudioProcessor()
{
    const juce::ScopedLock lock (programLock);
    publishedProgram.store (nullptr, std::memory_order_release);
    programHistory.clear();
    liveProgram.reset();
}

void FormulaCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engineResetRequested.store (false, std::memory_order_relaxed);
    setLatencySamples (0);

    inputGainSmoother.reset (sampleRate, 0.02);
    inputGainSmoother.setCurrentAndTargetValue (1.0f);

    scopeInputBuffer.assign (static_cast<std::size_t> (juce::jmax (1, samplesPerBlock)), 0.0f);
    scopeSamplesPerPoint = juce::jmax (1, juce::roundToInt (sampleRate / 120.0));
    resetScopeState();
    scopeResetRequested.store (true, std::memory_order_relaxed);
}

void FormulaCompressorAudioProcessor::resetScopeState() noexcept
{
    scopeWriteIndex.store (0, std::memory_order_relaxed);
    scopeCount.store (0, std::memory_order_relaxed);
    scopeSampleCounter = 0;
    scopeGainSamples = 0;
    scopeGainDbSum = 0.0;
    scopePointPrimed = false;
    scopeInputMin = scopeInputMax = 0.0f;
    scopeOutputMin = scopeOutputMax = 0.0f;
}

void FormulaCompressorAudioProcessor::publishScopePoint (float gainDb) noexcept
{
    const int write = scopeWriteIndex.load (std::memory_order_relaxed);
    auto& point = scopeData[static_cast<std::size_t> (write)];

    point.inputMin.store (scopeInputMin, std::memory_order_relaxed);
    point.inputMax.store (scopeInputMax, std::memory_order_relaxed);
    point.outputMin.store (scopeOutputMin, std::memory_order_relaxed);
    point.outputMax.store (scopeOutputMax, std::memory_order_relaxed);
    point.gainDb.store (gainDb, std::memory_order_relaxed);

    scopeWriteIndex.store ((write + 1) % scopePointCapacity, std::memory_order_release);

    const int count = scopeCount.load (std::memory_order_relaxed);

    if (count < scopePointCapacity)
        scopeCount.store (count + 1, std::memory_order_release);
}

void FormulaCompressorAudioProcessor::captureScope (int numSamples) noexcept
{
    const float* gains = engine.getLastGainCapture();
    const int gainCount = engine.getGainCaptureSize();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float input = scopeInputBuffer[static_cast<std::size_t> (sample)];
        float gain = 1.0f;

        if (gains != nullptr && sample < gainCount)
            gain = gains[sample];

        const float output = input * gain;

        if (! scopePointPrimed)
        {
            scopeInputMin = scopeInputMax = input;
            scopeOutputMin = scopeOutputMax = output;
            scopePointPrimed = true;
        }
        else
        {
            scopeInputMin = std::min (scopeInputMin, input);
            scopeInputMax = std::max (scopeInputMax, input);
            scopeOutputMin = std::min (scopeOutputMin, output);
            scopeOutputMax = std::max (scopeOutputMax, output);
        }

        scopeGainDbSum += 20.0 * std::log10 (
            std::max (1.0e-6f, std::abs (gain)));
        ++scopeGainSamples;

        if (++scopeSampleCounter >= scopeSamplesPerPoint)
        {
            publishScopePoint (static_cast<float> (
                scopeGainDbSum / std::max (1, scopeGainSamples)));

            scopeSampleCounter = 0;
            scopeGainSamples = 0;
            scopeGainDbSum = 0.0;
            scopePointPrimed = false;
        }
    }
}

void FormulaCompressorAudioProcessor::releaseResources()
{
}

bool FormulaCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

    return output == layouts.getMainInputChannelSet();
}

juce::AudioProcessorParameter* FormulaCompressorAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

void FormulaCompressorAudioProcessor::loadEngineParameters (formula::EngineParameters& parameters) const
{
    const auto value = [] (const std::atomic<float>* parameter, float fallback)
    {
        return parameter != nullptr ? parameter->load (std::memory_order_relaxed)
                                    : fallback;
    };

    parameters.attackMs = value (attackParam, 10.0f);
    parameters.releaseMs = value (releaseParam, 150.0f);
    parameters.holdMs = value (holdParam, 0.0f);
    parameters.mix = value (mixParam, 1.0f);
    parameters.makeupDb = value (makeupParam, 0.0f);
    parameters.detectorMode = juce::roundToInt (value (detectorParam, 0.0f));
    parameters.xDomain = formulaDomain.load (std::memory_order_relaxed);
    parameters.p1 = value (pParams[0], 0.25f);
    parameters.p2 = value (pParams[1], 0.25f);
    parameters.p3 = value (pParams[2], 0.0f);
    parameters.p4 = value (pParams[3], 0.0f);
}

void FormulaCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalChannels = buffer.getNumChannels();
    const int processedChannels = juce::jmin (totalChannels, 2);

    for (int channel = processedChannels; channel < totalChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples <= 0 || processedChannels <= 0)
        return;

    // State restores only publish a request; the audio thread owns the reset.
    if (engineResetRequested.exchange (false, std::memory_order_acq_rel))
        engine.reset();

    formula::EngineParameters parameters;
    loadEngineParameters (parameters);
    parameters.bypassed = bypassParam != nullptr && bypassParam->load() >= 0.5f;
    parameters.meterActive = isEditorActive();

    // Input trim is part of the compressor path; bypass ramps it out too.
    const float inputTarget = parameters.bypassed
                                  ? 1.0f
                                  : juce::Decibels::decibelsToGain (
                                        inputGainParam != nullptr
                                            ? inputGainParam->load()
                                            : 0.0f);
    inputGainSmoother.setTargetValue (inputTarget);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float gain = inputGainSmoother.getNextValue();

        for (int channel = 0; channel < processedChannels; ++channel)
            buffer.getWritePointer (channel)[sample] *= gain;
    }

    const bool scopeActive = isEditorActive()
                             && numSamples <= static_cast<int> (scopeInputBuffer.size());

    if (scopeActive)
    {
        const float* left = buffer.getReadPointer (0);
        const float* right = processedChannels > 1 ? buffer.getReadPointer (1) : nullptr;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float mono = right != nullptr
                                   ? 0.5f * (left[sample] + right[sample])
                                   : left[sample];
            scopeInputBuffer[static_cast<std::size_t> (sample)] =
                std::isfinite (mono) ? mono : 0.0f;
        }
    }

    engine.setProgram (publishedProgram.load (std::memory_order_acquire));
    engine.process (buffer.getArrayOfWritePointers(), processedChannels, numSamples, parameters);

    if (scopeActive)
    {
        if (scopeResetRequested.exchange (false))
            resetScopeState();

        captureScope (numSamples);
    }
}

juce::AudioProcessorEditor* FormulaCompressorAudioProcessor::createEditor()
{
    return new FormulaCompressorAudioProcessorEditor (*this);
}

juce::String FormulaCompressorAudioProcessor::getFormula() const
{
    const juce::ScopedLock lock (programLock);
    return apvts.state.getProperty ("formula", getDefaultFormulaText()).toString();
}

formula::CompileResult FormulaCompressorAudioProcessor::setFormula (const juce::String& text,
                                                                    bool updateState)
{
    auto result = formula::compileExpression (text);

    {
        const juce::ScopedLock lock (programLock);

        lastCompile = result;

        if (result.ok)
            publishProgramLocked (result.program);

        if (updateState)
            apvts.state.setProperty ("formula", text, nullptr);
    }

    formulaBroadcaster.sendChangeMessage();
    return result;
}

bool FormulaCompressorAudioProcessor::isFormulaValid() const
{
    const juce::ScopedLock lock (programLock);
    return lastCompile.ok;
}

juce::String FormulaCompressorAudioProcessor::getFormulaError() const
{
    const juce::ScopedLock lock (programLock);
    return lastCompile.error;
}

int FormulaCompressorAudioProcessor::getFormulaErrorColumn() const
{
    const juce::ScopedLock lock (programLock);
    return lastCompile.errorColumn;
}

std::shared_ptr<const formula::Program> FormulaCompressorAudioProcessor::getActiveProgram() const
{
    const juce::ScopedLock lock (programLock);
    return liveProgram;
}

float FormulaCompressorAudioProcessor::getPValue (int index) const
{
    if (index < 1 || index > 4)
        return 0.0f;

    const auto* parameter = pParams[index - 1];
    return parameter != nullptr ? parameter->load (std::memory_order_relaxed) : 0.0f;
}

void FormulaCompressorAudioProcessor::setEditorActive (bool active) noexcept
{
    if (active)
    {
        editorRefCount.fetch_add (1, std::memory_order_relaxed);
        scopeResetRequested.store (true, std::memory_order_relaxed);
        uiFrozen.store (false, std::memory_order_relaxed);
    }
    else
    {
        editorRefCount.fetch_sub (1, std::memory_order_relaxed);
    }

    if (editorRefCount.load (std::memory_order_relaxed) < 0)
        editorRefCount.store (0, std::memory_order_relaxed);
}

void FormulaCompressorAudioProcessor::setFormulaDomain (int domain)
{
    domain = juce::jlimit (0, 1, domain);
    formulaDomain.store (domain, std::memory_order_relaxed);

    {
        const juce::ScopedLock lock (programLock);
        apvts.state.setProperty ("xDomain", domain, nullptr);
    }

    formulaBroadcaster.sendChangeMessage();
}

void FormulaCompressorAudioProcessor::setParameterValue (const juce::String& id, float value)
{
    auto* parameter = apvts.getParameter (id);

    if (parameter == nullptr)
        return;

    const auto& range = parameter->getNormalisableRange();
    const auto clamped = juce::jlimit (range.start, range.end, value);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (clamped));
}

void FormulaCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    state.setProperty ("stateVersion", getStateVersion(), nullptr);

    if (! state.hasProperty ("formula"))
        state.setProperty ("formula", getDefaultFormulaText(), nullptr);

    if (! state.hasProperty ("xDomain"))
        state.setProperty ("xDomain", getFormulaDomain(), nullptr);

    const std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void FormulaCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    juce::ValueTree newState = juce::ValueTree::fromXml (*xml);

    if (! newState.isValid())
        return;

    const int loadedVersion = static_cast<int> (newState.getProperty ("stateVersion", 1));

    if (loadedVersion < 3 && ! newState.hasProperty ("xDomain"))
        newState.setProperty ("xDomain", 0, nullptr);

    if (! newState.hasProperty ("formula"))
        newState.setProperty ("formula", getDefaultFormulaText(), nullptr);

    apvts.replaceState (newState);
    apvts.state.setProperty ("stateVersion", getStateVersion(), nullptr);

    formulaDomain.store (juce::jlimit (0, 1,
                           static_cast<int> (apvts.state.getProperty ("xDomain", 0))),
                         std::memory_order_relaxed);

    applyFormulaFromState();
    engineResetRequested.store (true, std::memory_order_release);
}

void FormulaCompressorAudioProcessor::applyFormulaFromState()
{
    juce::String text;
    int domain = 0;

    {
        const juce::ScopedLock lock (programLock);
        text = apvts.state.getProperty ("formula", getDefaultFormulaText()).toString();

        domain = juce::jlimit (0, 1,
                               static_cast<int> (apvts.state.getProperty ("xDomain", 0)));
        formulaDomain.store (domain, std::memory_order_relaxed);

        lastCompile = formula::compileExpression (text);

        if (lastCompile.ok)
            publishProgramLocked (lastCompile.program);
    }

    formulaBroadcaster.sendChangeMessage();
}

void FormulaCompressorAudioProcessor::publishProgramLocked (const formula::Program& program)
{
    auto newProgram = std::make_shared<const formula::Program> (program);

    if (liveProgram != nullptr)
        programHistory.push_back (liveProgram);

    liveProgram = newProgram;
    publishedProgram.store (newProgram.get(), std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FormulaCompressorAudioProcessor();
}
