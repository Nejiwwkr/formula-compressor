#include "CompressorEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <limits>

namespace formula
{

void CompressorEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    gainCapture.assign (static_cast<std::size_t> (std::max (1, maximumBlockSize)), 0.0f);

    mixSmoother.prepare (sampleRate, 10.0);
    makeupSmoother.prepare (sampleRate, 20.0);
    bypassSmoother.prepare (sampleRate, 10.0);

    for (auto& smoother : pSmoothers)
        smoother.prepare (sampleRate, 10.0);

    reset();
}

void CompressorEngine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        envelope[c] = 0.0f;
        localRmsSquare[c] = 0.0f;
        holdCounter[c] = 0;
        hpfState[c] = {};
    }

    linkedRmsSquare = 0.0f;
    sidechainEnvelope = 0.0f;
    sidechainRmsSquare = 0.0f;
    sidechainHoldCounter = 0;
    hpfCoeffs = {};
    primed = false;

    lastEnvelope.store (0.0f, std::memory_order_relaxed);
    lastSidechainEnvelope.store (0.0f, std::memory_order_relaxed);
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    inputLevelDb.store (-100.0f, std::memory_order_relaxed);
    outputLevelDb.store (-100.0f, std::memory_order_relaxed);
    lastInverted.store (false, std::memory_order_relaxed);
}

CompressorEngine::BiquadCoeffs CompressorEngine::makeHighPass (float hz,
                                                               double fs) noexcept
{
    BiquadCoeffs coeffs;

    if (hz <= 0.5f || fs <= 0.0)
        return coeffs;

    const float nyquistLimit = static_cast<float> (0.45 * fs);
    const float f = std::clamp (hz, 10.0f, std::min (500.0f, nyquistLimit));
    const float w0 = 2.0f * 3.14159265358979f * f / static_cast<float> (fs);
    const float c = std::cos (w0);
    const float s = std::sin (w0);
    const float alpha = s / (2.0f * 0.70710678f);
    const float a0 = 1.0f + alpha;

    coeffs.b0 = (1.0f + c) * 0.5f / a0;
    coeffs.b1 = -(1.0f + c) / a0;
    coeffs.b2 = coeffs.b0;
    coeffs.a1 = -2.0f * c / a0;
    coeffs.a2 = (1.0f - alpha) / a0;
    coeffs.active = true;

    return coeffs;
}

float CompressorEngine::processBiquad (const BiquadCoeffs& coeffs,
                                       BiquadState& state,
                                       float x) noexcept
{
    if (! coeffs.active)
        return x;

    const float y = coeffs.b0 * x + state.z1;
    state.z1 = coeffs.b1 * x - coeffs.a1 * y + state.z2;
    state.z2 = coeffs.b2 * x - coeffs.a2 * y;
    return y;
}

float CompressorEngine::evaluateGain (float envelopeLinear, float sidechainValue,
                                      int xDomain, float p1, float p2, float p3,
                                      float p4) const noexcept
{
    if (activeProgram == nullptr || ! activeProgram->valid)
        return 1.0f;

    if (xDomain == 1)
    {
        const float inputDb = 20.0f * std::log10 (std::max (envelopeLinear, 1.0e-6f));
        const float outputDb = activeProgram->eval (inputDb, sidechainValue,
                                                    p1, p2, p3, p4);

        if (outputDb == -std::numeric_limits<float>::infinity())
            return 0.0f;

        if (std::isnan (outputDb))
            return 1.0f;

        const float effectiveDb = std::isinf (outputDb) ? 24.0f : outputDb;
        const float gainDb = std::clamp (effectiveDb - inputDb, -240.0f, 24.0f);
        const float gain = dbToGain (gainDb);
        return std::isfinite (gain) ? gain : 1.0f;
    }

    const float output = activeProgram->eval (envelopeLinear, sidechainValue,
                                              p1, p2, p3, p4);

    if (! std::isfinite (output))
        return 1.0f;

    const float gain = output / std::max (envelopeLinear, 1.0e-6f);
    return std::isfinite (gain) ? gain : 1.0f;
}

void CompressorEngine::process (float* const* channels, int numChannels, int numSamples,
                                const EngineParameters& parameters) noexcept
{
    process (channels, numChannels, numSamples, nullptr, 0, parameters);
}

void CompressorEngine::process (float* const* mainChannels, int numMainChannels,
                                int numSamples, const float* const* sidechainChannels,
                                int numSidechainChannels,
                                const EngineParameters& parameters) noexcept
{
    if (mainChannels == nullptr || numMainChannels <= 0 || numSamples <= 0)
        return;

    const juce::ScopedNoDenormals noDenormals;

    numMainChannels = std::clamp (numMainChannels, 1, 2);
    numSidechainChannels = sidechainChannels != nullptr
                               ? std::clamp (numSidechainChannels, 0, 2)
                               : 0;

    attackCoefficient = coefficientForMs (parameters.attackMs, sampleRate);
    releaseCoefficient = coefficientForMs (parameters.releaseMs, sampleRate);
    rmsCoefficient = coefficientForMs (10.0, sampleRate);

    holdSamples = static_cast<int> (
        std::lround (std::max (0.0f, parameters.holdMs) * 0.001f
                     * static_cast<float> (sampleRate)));

    hpfCoeffs = makeHighPass (parameters.scHpfHz, sampleRate);

    if (! hpfCoeffs.active || numSidechainChannels == 0)
    {
        hpfState[0] = {};
        hpfState[1] = {};
    }

    const float mixTarget = std::clamp (parameters.mix, 0.0f, 1.0f);
    const float makeupTarget = dbToGain (parameters.makeupDb);
    const float bypassTarget = parameters.bypassed ? 1.0f : 0.0f;
    const float pTargets[4] = {
        parameters.p1, parameters.p2, parameters.p3, parameters.p4
    };

    if (! primed)
    {
        mixSmoother.snapTo (mixTarget);
        makeupSmoother.snapTo (makeupTarget);
        bypassSmoother.snapTo (bypassTarget);

        for (int i = 0; i < 4; ++i)
            pSmoothers[i].snapTo (pTargets[i]);

        primed = true;
    }
    else
    {
        mixSmoother.setTarget (mixTarget);
        makeupSmoother.setTarget (makeupTarget);
        bypassSmoother.setTarget (bypassTarget);

        for (int i = 0; i < 4; ++i)
            pSmoothers[i].setTarget (pTargets[i]);
    }


    const float maxGain = dbToGain (24.0f);
    const float link = std::clamp (parameters.stereoLink, 0.0f, 1.0f);
    const bool fullyLinked = link >= 1.0f;
    const int monitorMode = std::clamp (parameters.monitorMode, 0, 2);
    const int captureSize = static_cast<int> (gainCapture.size());

    float blockPeakIn = 0.0f;
    float blockPeakOut = 0.0f;
    float blockMinAbsGain = std::numeric_limits<float>::infinity();
    float worstGain = 1.0f;
    bool anyInverted = false;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mainDetectorPeak[2] {};
        float mainDetectorMs[2] {};

        for (int channel = 0; channel < numMainChannels; ++channel)
        {
            const float raw = mainChannels[channel][sample];
            const float value = std::isfinite (raw)
                                    ? std::clamp (raw, -1.0e6f, 1.0e6f)
                                    : 0.0f;
            mainDetectorPeak[channel] = std::abs (value);
            mainDetectorMs[channel] = value * value;
            blockPeakIn = std::max (blockPeakIn, mainDetectorPeak[channel]);
        }

        float scPeak = 0.0f;
        float scMeanSquare = 0.0f;
        float scFiltered[2] {};

        if (numSidechainChannels > 0)
        {
            for (int channel = 0; channel < numSidechainChannels; ++channel)
            {
                const float raw = sidechainChannels[channel][sample];
                float value = std::isfinite (raw)
                                  ? std::clamp (raw, -1.0e6f, 1.0e6f)
                                  : 0.0f;

                value = processBiquad (hpfCoeffs, hpfState[channel], value);
                scFiltered[channel] = value;
                scPeak = std::max (scPeak, std::abs (value));
                scMeanSquare += value * value;
            }

            scMeanSquare /= static_cast<float> (numSidechainChannels);
        }

        float sidechainDetector;

        if (parameters.detectorMode == 1)
        {
            sidechainRmsSquare += (scMeanSquare - sidechainRmsSquare) * rmsCoefficient;
            sidechainDetector = std::sqrt (std::max (sidechainRmsSquare, 0.0f));
        }
        else
        {
            sidechainDetector = scPeak;
        }

        updateEnvelope (sidechainEnvelope, sidechainHoldCounter, sidechainDetector,
                        attackCoefficient, releaseCoefficient, holdSamples);

        float linkedPeak = 0.0f;
        float linkedMeanSquare = 0.0f;

        for (int channel = 0; channel < numMainChannels; ++channel)
        {
            linkedPeak = std::max (linkedPeak, mainDetectorPeak[channel]);
            linkedMeanSquare += mainDetectorMs[channel];
        }

        linkedMeanSquare /= static_cast<float> (numMainChannels);

        float linkedDetector;

        if (parameters.detectorMode == 1)
        {
            linkedRmsSquare += (linkedMeanSquare - linkedRmsSquare) * rmsCoefficient;
            linkedDetector = std::sqrt (std::max (linkedRmsSquare, 0.0f));
        }
        else
        {
            linkedDetector = linkedPeak;
        }

        for (int channel = 0; channel < numMainChannels; ++channel)
        {
            float localDetector;

            if (parameters.detectorMode == 1)
            {
                localRmsSquare[channel] += (mainDetectorMs[channel]
                                            - localRmsSquare[channel])
                                           * rmsCoefficient;
                localDetector = std::sqrt (std::max (localRmsSquare[channel], 0.0f));
            }
            else
            {
                localDetector = mainDetectorPeak[channel];
            }

            const float detector = link * linkedDetector
                                   + (1.0f - link) * localDetector;
            updateEnvelope (envelope[channel], holdCounter[channel], detector,
                            attackCoefficient, releaseCoefficient, holdSamples);
        }

        const float p1 = pSmoothers[0].next();
        const float p2 = pSmoothers[1].next();
        const float p3 = pSmoothers[2].next();
        const float p4 = pSmoothers[3].next();

        const float sidechainValue = parameters.xDomain == 1
                                         ? 20.0f * std::log10 (
                                               std::max (sidechainEnvelope, 1.0e-6f))
                                         : sidechainEnvelope;

        float gains[2] { 1.0f, 1.0f };

        if (fullyLinked)
        {
            const float gain = evaluateGain (envelope[0], sidechainValue,
                                             parameters.xDomain, p1, p2, p3, p4);
            gains[0] = gains[1] = gain;
        }
        else
        {
            for (int channel = 0; channel < numMainChannels; ++channel)
                gains[channel] = evaluateGain (envelope[channel], sidechainValue,
                                               parameters.xDomain, p1, p2, p3, p4);
        }

        for (int channel = 0; channel < numMainChannels; ++channel)
        {
            float gain = std::clamp (gains[channel], -maxGain, maxGain);

            if (std::abs (gain) < 1.0e-9f)
                gain = 0.0f;

            gains[channel] = gain;

            if (std::abs (gain) < blockMinAbsGain)
            {
                blockMinAbsGain = std::abs (gain);
                worstGain = gain;
            }

            anyInverted = anyInverted || gain < 0.0f;
        }

        if (sample < captureSize)
            gainCapture[static_cast<std::size_t> (sample)] = worstGain;

        const float mix = mixSmoother.next();
        const float makeup = makeupSmoother.next();
        const float bypass = bypassSmoother.next();

        for (int channel = 0; channel < numMainChannels; ++channel)
        {
            float* data = mainChannels[channel];
            const float rawDry = data[sample];
            const float dry = std::isfinite (rawDry) ? rawDry : 0.0f;
            const float gain = gains[channel];
            const float wet = dry * gain * makeup;

            float output = dry + (wet - dry) * mix;

            if (monitorMode == 1)
            {
                output = mix * (dry - dry * gain);
            }
            else if (monitorMode == 2)
            {
                const int detectorChannel = numSidechainChannels > 0
                                                ? std::min (channel,
                                                            numSidechainChannels - 1)
                                                : 0;
                output = numSidechainChannels > 0
                             ? scFiltered[detectorChannel]
                             : 0.0f;
            }

            output = output * (1.0f - bypass) + dry * bypass;

            if (! std::isfinite (output))
                output = 0.0f;

            data[sample] = output;
            blockPeakOut = std::max (blockPeakOut, std::abs (output));
        }
    }


    lastInverted.store (anyInverted, std::memory_order_relaxed);

    if (parameters.meterActive)
    {
        lastEnvelope.store (fullyLinked ? envelope[0]
                                        : std::max (envelope[0], envelope[1]),
                            std::memory_order_relaxed);
        lastSidechainEnvelope.store (sidechainEnvelope, std::memory_order_relaxed);
        gainReductionDb.store (std::isfinite (blockMinAbsGain)
                                   ? gainToDb (blockMinAbsGain)
                                   : 0.0f,
                               std::memory_order_relaxed);
        inputLevelDb.store (blockPeakIn > 0.0f ? gainToDb (blockPeakIn) : -100.0f,
                            std::memory_order_relaxed);
        outputLevelDb.store (blockPeakOut > 0.0f ? gainToDb (blockPeakOut) : -100.0f,
                             std::memory_order_relaxed);
    }
}

} // namespace formula
