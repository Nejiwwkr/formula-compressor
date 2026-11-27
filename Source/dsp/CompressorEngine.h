#pragma once

#include "Expression.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace formula
{

struct EngineParameters
{
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    float holdMs = 0.0f;
    float mix = 1.0f;
    float makeupDb = 0.0f;
    int detectorMode = 0; // 0 = peak, 1 = RMS
    int xDomain = 0;      // 0 = linear amplitude, 1 = dBFS
    float p1 = 0.25f;
    float p2 = 0.25f;
    float p3 = 0.0f;
    float p4 = 0.0f;
    bool bypassed = false;
    bool meterActive = true;
};

/**
    Stereo-linked feed-forward compressor driven by a user formula.

    The formula maps the smoothed detector envelope e (linear amplitude) to an
    output amplitude y; the applied gain is y / e, clamped to -60..+24 dB.
    Attack / release / hold shape the envelope, not the gain directly.
*/
class CompressorEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    void setProgram (const Program* program) noexcept { activeProgram = program; }

    void process (float* const* channels, int numChannels, int numSamples,
                  const EngineParameters& parameters) noexcept;

    /** Linear detector envelope, for tests and the live curve marker. */
    float getEnvelopeLevel() const noexcept
    {
        return lastEnvelope.load (std::memory_order_relaxed);
    }

    float getGainReductionDb() const noexcept
    {
        return gainReductionDb.load (std::memory_order_relaxed);
    }

    float getInputLevelDb() const noexcept
    {
        return inputLevelDb.load (std::memory_order_relaxed);
    }

    float getOutputLevelDb() const noexcept
    {
        return outputLevelDb.load (std::memory_order_relaxed);
    }

    /** True when the last processed gain was negative (polarity inverted). */
    bool isGainInverted() const noexcept
    {
        return lastInverted.load (std::memory_order_relaxed);
    }

    /** Signed formula gain for every sample of the last process() call. */
    const float* getLastGainCapture() const noexcept
    {
        return gainCapture.empty() ? nullptr : gainCapture.data();
    }

    int getGainCaptureSize() const noexcept
    {
        return static_cast<int> (gainCapture.size());
    }

private:
    struct ScalarSmoother
    {
        float current = 0.0f;
        float target = 0.0f;
        float coefficient = 0.01f;

        void prepare (double sampleRate, double timeMs) noexcept
        {
            coefficient = static_cast<float> (
                1.0 - std::exp (-1.0 / (timeMs * 0.001 * sampleRate)));
        }

        void snapTo (float value) noexcept
        {
            current = target = value;
        }

        void setTarget (float value) noexcept { target = value; }

        float next() noexcept
        {
            current += (target - current) * coefficient;
            return current;
        }
    };

    static float coefficientForMs (double timeMs, double sampleRate) noexcept
    {
        return static_cast<float> (
            1.0 - std::exp (-1.0 / (std::max (0.01, timeMs) * 0.001 * sampleRate)));
    }

    static float gainToDb (float gain) noexcept
    {
        return 20.0f * std::log10 (std::max (1.0e-9f, gain));
    }

    static float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db / 20.0f);
    }

    double sampleRate = 48000.0;
    const Program* activeProgram = nullptr;

    float envelope = 0.0f;
    float rmsSquare = 0.0f;
    int holdCounter = 0;
    bool primed = false;

    float attackCoefficient = 0.01f;
    float releaseCoefficient = 0.001f;
    float rmsCoefficient = 0.01f;
    int holdSamples = 0;

    ScalarSmoother mixSmoother;
    ScalarSmoother makeupSmoother;
    ScalarSmoother bypassSmoother;
    ScalarSmoother pSmoothers[4];

    std::atomic<float> lastEnvelope { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<bool> lastInverted { false };

    std::vector<float> gainCapture;
};

} // namespace formula
