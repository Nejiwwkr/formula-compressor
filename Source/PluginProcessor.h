#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/CompressorEngine.h"
#include "dsp/ExpressionParser.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class FormulaCompressorAudioProcessor final : public juce::AudioProcessor,
                                              private juce::Timer
{
public:
    FormulaCompressorAudioProcessor();
    ~FormulaCompressorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

    static constexpr int getStateVersion() noexcept { return 3; }

    static juce::String getDefaultFormulaText();

    /** 0 = x is linear amplitude, 1 = x is dBFS (whole formula in dB). */
    int getFormulaDomain() const noexcept
    {
        return formulaDomain.load (std::memory_order_relaxed);
    }

    void setFormulaDomain (int domain);

    static constexpr int getScopePointCount() noexcept { return scopePointCapacity; }

    int getScopeWriteIndex() const noexcept
    {
        return scopeWriteIndex.load (std::memory_order_relaxed);
    }

    int getScopeCount() const noexcept
    {
        return scopeCount.load (std::memory_order_relaxed);
    }

    float getScopeInputMin (int index) const noexcept
    {
        return scopeData[static_cast<std::size_t> (scopeIndex (index))]
            .inputMin.load (std::memory_order_relaxed);
    }

    float getScopeInputMax (int index) const noexcept
    {
        return scopeData[static_cast<std::size_t> (scopeIndex (index))]
            .inputMax.load (std::memory_order_relaxed);
    }

    float getScopeOutputMin (int index) const noexcept
    {
        return scopeData[static_cast<std::size_t> (scopeIndex (index))]
            .outputMin.load (std::memory_order_relaxed);
    }

    float getScopeOutputMax (int index) const noexcept
    {
        return scopeData[static_cast<std::size_t> (scopeIndex (index))]
            .outputMax.load (std::memory_order_relaxed);
    }

    float getScopeGainDb (int index) const noexcept
    {
        return scopeData[static_cast<std::size_t> (scopeIndex (index))]
            .gainDb.load (std::memory_order_relaxed);
    }

    /** Current text in the state tree (may be invalid while the user edits). */
    juce::String getFormula() const;

    /**
        Parses and publishes a formula on the message thread.

        On failure the previously valid program keeps running, the text is still
        stored in the state tree, and the compile error is exposed for the UI.
    */
    formula::CompileResult setFormula (const juce::String& text, bool updateState = true);

    bool isFormulaValid() const;
    juce::String getFormulaError() const;
    int getFormulaErrorColumn() const;

    /** Program currently used by the audio thread, for drawing the curve. */
    std::shared_ptr<const formula::Program> getActiveProgram() const;

    juce::ChangeBroadcaster& getFormulaBroadcaster() noexcept { return formulaBroadcaster; }

    float getPValue (int index) const; // index 1..4

    /** Called by editors; metering and analysis only run while one is open. */
    void setEditorActive (bool active) noexcept;
    bool isUiFrozen() const noexcept
    {
        return uiFrozen.load (std::memory_order_relaxed);
    }

    void setUiFrozen (bool frozen) noexcept
    {
        uiFrozen.store (frozen, std::memory_order_relaxed);
    }

    bool isEditorActive() const noexcept
    {
        return editorRefCount.load (std::memory_order_relaxed) > 0;
    }

    float getDetectorLevel() const noexcept { return engine.getEnvelopeLevel(); }
    float getGainReductionDb() const noexcept { return engine.getGainReductionDb(); }
    float getInputLevelDb() const noexcept { return engine.getInputLevelDb(); }
    float getOutputLevelDb() const noexcept { return engine.getOutputLevelDb(); }
    bool isGainInverted() const noexcept { return engine.isGainInverted(); }

    /** Sets a parameter from a normalised-UI value (message thread, presets). */
    void setParameterValue (const juce::String& id, float value);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void timerCallback() override;
    void publishProgramLocked (const formula::Program& program);
    void retireOldProgramsLocked();
    void loadEngineParameters (formula::EngineParameters&) const;
    void applyFormulaFromState();

    static int scopeIndex (int index) noexcept
    {
        return juce::jlimit (0, scopePointCapacity - 1, index);
    }

    void resetScopeState() noexcept;
    void captureScope (int numSamples) noexcept;
    void publishScopePoint (float gainDb) noexcept;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* makeupParam = nullptr;
    std::atomic<float>* detectorParam = nullptr;
    std::atomic<float>* pParams[4] { nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* bypassParam = nullptr;

    formula::CompressorEngine engine;
    juce::SmoothedValue<float> inputGainSmoother;

    mutable juce::CriticalSection programLock;
    std::shared_ptr<const formula::Program> liveProgram;
    std::atomic<const formula::Program*> publishedProgram { nullptr };

    struct RetiredProgram
    {
        std::shared_ptr<const formula::Program> program;
        std::uint32_t stampMs = 0;
    };

    std::vector<RetiredProgram> retiredPrograms;

    formula::CompileResult lastCompile;
    juce::ChangeBroadcaster formulaBroadcaster;

    std::atomic<int> editorRefCount { 0 };
    std::atomic<bool> uiFrozen { false };

    std::atomic<int> formulaDomain { 0 };

    static constexpr int scopePointCapacity = 360;

    struct ScopePoint
    {
        std::atomic<float> inputMin { 0.0f };
        std::atomic<float> inputMax { 0.0f };
        std::atomic<float> outputMin { 0.0f };
        std::atomic<float> outputMax { 0.0f };
        std::atomic<float> gainDb { 0.0f };
    };

    std::array<ScopePoint, scopePointCapacity> scopeData;
    std::atomic<int> scopeWriteIndex { 0 };
    std::atomic<int> scopeCount { 0 };
    std::atomic<bool> scopeResetRequested { true };
    std::vector<float> scopeInputBuffer;
    int scopeSamplesPerPoint = 1;
    int scopeSampleCounter = 0;
    float scopeInputMin = 0.0f;
    float scopeInputMax = 0.0f;
    float scopeOutputMin = 0.0f;
    float scopeOutputMax = 0.0f;
    double scopeGainDbSum = 0.0;
    int scopeGainSamples = 0;
    bool scopePointPrimed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FormulaCompressorAudioProcessor)
};
