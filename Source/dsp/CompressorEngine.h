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
    float stereoLink = 1.0f; // 0 = dual mono, 1 = fully linked
    float scHpfHz = 0.0f;    // 0 = off, detector-only high-pass
    int monitorMode = 0;     // 0 = off, 1 = delta, 2 = sidechain listen
    float p1 = 0.25f;
    float p2 = 0.25f;
    float p3 = 0.0f;
    float p4 = 0.0f;
    bool bypassed = false;
    bool meterActive = true;
};

/**
    Stereo-linked feed-forward compressor driven by a user formula.

    x is the main-input detector envelope. s is the selected external
    sidechain detector envelope. The formula maps both smoothed levels to an
    output level; the applied gain is clamped to +/-24 dB.
    Attack / release / hold shape the envelopes, not the gain directly.
*/
class CompressorEngine
{
public:
    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    void setProgram (const Program* program) noexcept { activeProgram = program; }

    /** Process without an external sidechain (s stays at its silent value). */
    void process (float* const* channels, int numChannels, int numSamples,
                  const EngineParameters& parameters) noexcept;

    /**
        Process with an optional read-only sidechain input.

        sidechainChannels may be nullptr / numSidechainChannels may be 0; in
        that case the sidechain envelope decays to silence and s becomes 0 in
        AMP mode or -120 in dBFS mode.
    */
    void process (float* const* mainChannels, int numMainChannels, int numSamples,
                  const float* const* sidechainChannels, int numSidechainChannels,
                  const EngineParameters& parameters) noexcept;

    /** Linear detector envelope of the main input, for tests and the live curve marker. */
    float getEnvelopeLevel() const noexcept
    {
        return lastEnvelope.load (std::memory_order_relaxed);
    }

    /** Linear detector envelope of the selected sidechain, for the s readout. */
    float getSidechainEnvelope() const noexcept
    {
        return lastSidechainEnvelope.load (std::memory_order_relaxed);
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

    /** True when any channel inverted in the last block. */
    bool isGainInverted() const noexcept
    {
        return lastInverted.load (std::memory_order_relaxed);
    }

    /** Signed formula gain of the most-reduced channel for every sample. */
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

    struct BiquadState
    {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    struct BiquadCoeffs
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        bool active = false;
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

    static void updateEnvelope (float& envelope, int& holdCounter,
                                float detector, float attackCoefficient,
                                float releaseCoefficient, int holdSamples) noexcept
    {
        if (detector > envelope)
        {
            envelope += (detector - envelope) * attackCoefficient;

            if (holdSamples > 0)
                holdCounter = holdSamples;
        }
        else if (holdCounter > 0)
        {
            --holdCounter;
        }
        else
        {
            envelope += (detector - envelope) * releaseCoefficient;
        }

        if (envelope < 1.0e-9f)
            envelope = 0.0f;
    }

    static BiquadCoeffs makeHighPass (float hz, double sampleRate) noexcept;
    static float processBiquad (const BiquadCoeffs&, BiquadState&, float x) noexcept;

    float evaluateGain (float envelopeLinear, float sidechainValue,
                        int xDomain, float p1, float p2, float p3, float p4) const noexcept;

    double sampleRate = 48000.0;
    const Program* activeProgram = nullptr;

    float envelope[2] {};
    float localRmsSquare[2] {};
    int holdCounter[2] {};
    float linkedRmsSquare = 0.0f;

    float sidechainEnvelope = 0.0f;
    float sidechainRmsSquare = 0.0f;
    int sidechainHoldCounter = 0;

    BiquadState hpfState[2];
    BiquadCoeffs hpfCoeffs;

    float attackCoefficient = 0.01f;
    float releaseCoefficient = 0.001f;
    float rmsCoefficient = 0.01f;
    int holdSamples = 0;
    bool primed = false;

    ScalarSmoother mixSmoother;
    ScalarSmoother makeupSmoother;
    ScalarSmoother bypassSmoother;
    ScalarSmoother pSmoothers[4];

    std::atomic<float> lastEnvelope { 0.0f };
    std::atomic<float> lastSidechainEnvelope { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<bool> lastInverted { false };

    std::vector<float> gainCapture;
};

} // namespace formula
