#include "CompressorEngine.h"

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
    envelope = 0.0f;
    rmsSquare = 0.0f;
    holdCounter = 0;
    primed = false;

    lastEnvelope.store (0.0f, std::memory_order_relaxed);
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    inputLevelDb.store (-100.0f, std::memory_order_relaxed);
    outputLevelDb.store (-100.0f, std::memory_order_relaxed);
    lastInverted.store (false, std::memory_order_relaxed);
}

void CompressorEngine::process (float* const* channels, int numChannels, int numSamples,
                                const EngineParameters& parameters) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    attackCoefficient = coefficientForMs (parameters.attackMs, sampleRate);
    releaseCoefficient = coefficientForMs (parameters.releaseMs, sampleRate);
    rmsCoefficient = coefficientForMs (10.0, sampleRate);

    holdSamples = static_cast<int> (
        std::lround (std::max (0.0f, parameters.holdMs) * 0.001f
                     * static_cast<float> (sampleRate)));

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
    const int captureSize = static_cast<int> (gainCapture.size());

    float blockPeakIn = 0.0f;
    float blockPeakOut = 0.0f;
    float lastGain = 1.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float peak = 0.0f;
        float meanSquare = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float value = channels[channel][sample];
            peak = std::max (peak, std::abs (value));
            meanSquare += value * value;
        }

        meanSquare /= static_cast<float> (numChannels);
        blockPeakIn = std::max (blockPeakIn, peak);

        float detector;

        if (parameters.detectorMode == 1)
        {
            rmsSquare += (meanSquare - rmsSquare) * rmsCoefficient;
            detector = std::sqrt (std::max (rmsSquare, 0.0f));
        }
        else
        {
            detector = peak;
        }

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

        const float p1 = pSmoothers[0].next();
        const float p2 = pSmoothers[1].next();
        const float p3 = pSmoothers[2].next();
        const float p4 = pSmoothers[3].next();

        float gain = 1.0f;

        if (activeProgram != nullptr && activeProgram->valid)
        {
            if (parameters.xDomain == 1)
            {
                // dB domain: f(x) is in dBFS, gain dB = f(x) - x.
                const float inputDb = 20.0f * std::log10 (std::max (envelope, 1.0e-6f));
                const float outputDb = activeProgram->eval (inputDb, p1, p2, p3, p4);

                if (outputDb == -std::numeric_limits<float>::infinity())
                {
                    gain = 0.0f;
                }
                else if (! std::isnan (outputDb))
                {
                    const float effectiveDb = std::isinf (outputDb) ? 24.0f : outputDb;
                    const float gainDb = std::clamp (effectiveDb - inputDb,
                                                     -240.0f, 24.0f);
                    gain = dbToGain (gainDb);

                    if (! std::isfinite (gain))
                        gain = 1.0f;
                }
            }
            else
            {
                const float output = activeProgram->eval (envelope, p1, p2, p3, p4);

                if (std::isfinite (output))
                {
                    gain = output / std::max (envelope, 1.0e-6f);

                    if (! std::isfinite (gain))
                        gain = 1.0f;
                }
            }
        }

        // The sign carries polarity: f(x) < 0 inverts both channels, while
        // f(x) == 0 is true silence. The magnitude is clamped to +24 dB.
        gain = std::clamp (gain, -maxGain, maxGain);

        if (std::abs (gain) < 1.0e-9f)
            gain = 0.0f;

        if (sample < captureSize)
            gainCapture[static_cast<std::size_t> (sample)] = gain;

        const float mix = mixSmoother.next();
        const float makeup = makeupSmoother.next();
        const float bypass = bypassSmoother.next();
        const float wetGain = gain * makeup;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float* data = channels[channel];
            const float dry = data[sample];
            const float wet = dry * wetGain;

            float output = dry + (wet - dry) * mix;
            output = output * (1.0f - bypass) + dry * bypass;

            data[sample] = output;
            blockPeakOut = std::max (blockPeakOut, std::abs (output));
        }

        lastGain = gain;
    }

    lastInverted.store (lastGain < 0.0f, std::memory_order_relaxed);

    if (parameters.meterActive)
    {
        lastEnvelope.store (envelope, std::memory_order_relaxed);
        gainReductionDb.store (gainToDb (std::abs (lastGain)), std::memory_order_relaxed);
        inputLevelDb.store (blockPeakIn > 0.0f ? gainToDb (blockPeakIn) : -100.0f,
                            std::memory_order_relaxed);
        outputLevelDb.store (blockPeakOut > 0.0f ? gainToDb (blockPeakOut) : -100.0f,
                             std::memory_order_relaxed);
    }
}

} // namespace formula
