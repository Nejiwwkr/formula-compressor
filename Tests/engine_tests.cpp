#include "TestFramework.h"

#include "dsp/CompressorEngine.h"
#include "dsp/ExpressionParser.h"

#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
constexpr double testSampleRate = 48000.0;
const juce::String ampClassic = "min(x, P1 + (x - P1) * P2)";

struct StereoResult
{
    std::vector<float> left;
    std::vector<float> right;
};

StereoResult runEngine (formula::CompressorEngine& engine,
                        const formula::EngineParameters& parameters,
                        float leftValue, float rightValue, int numSamples)
{
    StereoResult result;
    result.left.assign (static_cast<std::size_t> (numSamples), leftValue);
    result.right.assign (static_cast<std::size_t> (numSamples), rightValue);

    float* pointers[2] = { result.left.data(), result.right.data() };
    engine.process (pointers, 2, numSamples, parameters);
    return result;
}

StereoResult runWithSidechain (formula::CompressorEngine& engine,
                               const formula::EngineParameters& parameters,
                               float mainValue, float sidechainValue, int numSamples)
{
    StereoResult result;
    result.left.assign (static_cast<std::size_t> (numSamples), mainValue);
    result.right.assign (static_cast<std::size_t> (numSamples), mainValue);

    std::vector<float> sidechain (static_cast<std::size_t> (numSamples), sidechainValue);
    float* audio[2] = { result.left.data(), result.right.data() };
    const float* detector[2] = { sidechain.data(), sidechain.data() };

    engine.process (audio, 2, numSamples, detector, 2, parameters);
    return result;
}

/** Runs four independent sidechain buses; absent buses stay nullptr. */
StereoResult runWithSidechains (formula::CompressorEngine& engine,
                                const formula::EngineParameters& parameters,
                                float mainValue,
                                const std::array<float, 4>& sidechainValues,
                                const std::array<bool, 4>& present,
                                int numSamples)
{
    StereoResult result;
    result.left.assign (static_cast<std::size_t> (numSamples), mainValue);
    result.right.assign (static_cast<std::size_t> (numSamples), mainValue);

    std::array<std::vector<float>, 4> buffers;
    std::array<std::array<const float*, 2>, 4> rows {};
    std::array<formula::SidechainInput, 4> inputs {};

    for (int bus = 0; bus < 4; ++bus)
    {
        if (! present[static_cast<std::size_t> (bus)])
            continue;

        buffers[static_cast<std::size_t> (bus)].assign (
            static_cast<std::size_t> (numSamples),
            sidechainValues[static_cast<std::size_t> (bus)]);

        const float* data = buffers[static_cast<std::size_t> (bus)].data();
        rows[static_cast<std::size_t> (bus)] = { data, data };
        inputs[static_cast<std::size_t> (bus)].channels =
            rows[static_cast<std::size_t> (bus)].data();
        inputs[static_cast<std::size_t> (bus)].numChannels = 2;
    }

    float* audio[2] = { result.left.data(), result.right.data() };
    engine.process (audio, 2, numSamples, inputs.data(), 4, parameters);
    return result;
}

struct Rig
{
    formula::CompressorEngine engine;
    formula::CompileResult program;
    formula::EngineParameters parameters;

    explicit Rig (const juce::String& text, double sampleRate = testSampleRate)
    {
        engine.prepare (sampleRate, 512);
        program = formula::compileExpression (text);
        CHECK_MESSAGE (program.ok, text + " -> " + program.error);
        engine.setProgram (&program.program);

        parameters.attackMs = 1.0f;
        parameters.releaseMs = 150.0f;
        parameters.holdMs = 0.0f;
        parameters.mix = 1.0f;
        parameters.makeupDb = 0.0f;
        parameters.detectorMode = 0; // peak
        parameters.p1 = 0.25f;
        parameters.p2 = 0.25f;
        parameters.p3 = 0.0f;
        parameters.p4 = 0.0f;
        parameters.meterActive = true;
    }

    StereoResult run (float leftValue, int numSamples, float rightValue = -999.0f)
    {
        return runEngine (engine, parameters,
                          leftValue,
                          rightValue <= -998.0f ? leftValue : rightValue,
                          numSamples);
    }
};

int samplesForMs (double milliseconds, double sampleRate = testSampleRate)
{
    return juce::roundToInt (milliseconds * 0.001 * sampleRate);
}

bool allFinite (const std::vector<float>& values)
{
    for (const float value : values)
        if (! std::isfinite (value))
            return false;

    return true;
}
} // namespace

void tests::testEngine()
{
    std::printf ("Engine tests\n");

    // Static transfer curve, fully settled.
    {
        Rig rig (ampClassic);
        rig.parameters.attackMs = 0.1f;

        const auto result = rig.run (0.5f, samplesForMs (500.0));
        CHECK_CLOSE (result.left.back(), 0.3125, 1.0e-3);
        CHECK_CLOSE (rig.engine.getEnvelopeLevel(), 0.5, 1.0e-3);
        CHECK (rig.engine.getGainReductionDb() < -3.0f);
    }

    // Attack time constant: 63% of a step after 10 ms.
    {
        Rig rig ("x");
        rig.parameters.attackMs = 10.0f;

        rig.run (0.5f, samplesForMs (10.0));
        CHECK_CLOSE (rig.engine.getEnvelopeLevel(), 0.5 * 0.63212, 0.02);
    }

    // Release time constant: 37% left after one release time.
    {
        Rig rig ("x");
        rig.parameters.attackMs = 0.1f;
        rig.parameters.releaseMs = 150.0f;

        rig.run (0.5f, samplesForMs (1000.0));
        rig.run (0.0f, samplesForMs (150.0));
        CHECK_CLOSE (rig.engine.getEnvelopeLevel(), 0.5 * 0.36788, 0.015);
    }

    // Hold freezes the reduction, then release resumes.
    {
        Rig rig (ampClassic);
        rig.parameters.attackMs = 1.0f;
        rig.parameters.releaseMs = 50.0f;
        rig.parameters.holdMs = 100.0f;

        rig.run (0.5f, samplesForMs (500.0));
        rig.run (0.0f, samplesForMs (50.0));
        CHECK_MESSAGE (rig.engine.getEnvelopeLevel() > 0.49f,
                       "hold should freeze the envelope");

        rig.run (0.0f, samplesForMs (100.0));
        CHECK_MESSAGE (rig.engine.getEnvelopeLevel() < 0.30f,
                       "release should resume after hold");
    }

    // A new attack event restarts the hold timer.
    {
        Rig rig ("x");
        rig.parameters.holdMs = 50.0f;

        rig.run (0.0f, samplesForMs (100.0));
        rig.run (0.5f, samplesForMs (50.0));
        CHECK_MESSAGE (rig.engine.getEnvelopeLevel() > 0.4f,
                       "hold should not block a new attack");
    }

    // Stereo linking: the loud channel compresses both.
    {
        Rig rig ("min(x, 0.25)");
        rig.parameters.attackMs = 1.0f;

        const auto result = rig.run (0.5f, samplesForMs (200.0), 0.05f);
        CHECK_CLOSE (result.left.back(), 0.25, 2.0e-3);
        CHECK_CLOSE (result.right.back(), 0.025, 2.0e-4);
    }

    // LINK=0 is dual mono: the quiet channel is not compressed by the loud one.
    {
        Rig rig ("min(x, 0.25)");
        rig.parameters.attackMs = 1.0f;
        rig.parameters.stereoLink = 0.0f;

        const auto result = rig.run (0.5f, samplesForMs (200.0), 0.05f);
        CHECK_CLOSE (result.left.back(), 0.25, 2.0e-3);
        CHECK_CLOSE (result.right.back(), 0.05, 2.0e-3);
    }

    // Partial link sits between the two.
    {
        Rig rig ("min(x, 0.25)");
        rig.parameters.attackMs = 1.0f;
        rig.parameters.stereoLink = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0), 0.05f);
        CHECK (result.right.back() > 0.025f);
        CHECK (result.right.back() < 0.05f);
    }

    // Sidechain s uses the same domain as x, and f(x,s) can duck the main path.
    {
        Rig rig ("x - max(0, s + 60) / 4");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto loud = runWithSidechain (rig.engine, rig.parameters, 0.5f, 0.5f,
                                            samplesForMs (300.0));
        CHECK (loud.left.back() < 0.2f);

        Rig quietRig ("x - max(0, s + 60) / 4");
        quietRig.parameters.xDomain = 1;
        quietRig.parameters.attackMs = 0.5f;

        const auto quiet = runWithSidechain (quietRig.engine, quietRig.parameters,
                                             0.5f, 0.0f, samplesForMs (300.0));
        CHECK_CLOSE (quiet.left.back(), 0.5f, 0.02f);
    }

    // With no sidechain s is silence; an AMP formula using s stays neutral.
    {
        Rig rig ("x - s / 10");
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.5f, 2.0e-3);
        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 0.0f, 1.0e-6);
    }

    // Sidechain HPF removes DC from s but never touches the main x path.
    {
        Rig rig ("x - max(0, s + 60) / 4");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;
        rig.parameters.scHpfHz = 200.0f;

        const auto filtered = runWithSidechain (rig.engine, rig.parameters, 0.5f, 0.5f,
                                                samplesForMs (1000.0));
        CHECK_MESSAGE (filtered.left.back() > 0.45f,
                       "HPF output " + juce::String (filtered.left.back(), 5));

        rig.parameters.scHpfHz = 0.0f;
        const auto unfiltered = runWithSidechain (rig.engine, rig.parameters, 0.5f, 0.5f,
                                                  samplesForMs (500.0));
        CHECK (unfiltered.left.back() < 0.3f);
    }

    // Monitor Delta removes makeup and plays the removed part.
    {
        Rig rig ("min(x, 0.25)");
        rig.parameters.attackMs = 0.5f;
        rig.parameters.monitorMode = 1;
        rig.parameters.makeupDb = 12.0f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.25f, 2.0e-3);

        rig.parameters.mix = 0.5f;
        const auto half = rig.run (0.5f, samplesForMs (50.0));
        CHECK_CLOSE (half.left.back(), 0.125f, 5.0e-3);

        rig.parameters.bypassed = true;
        const auto bypassed = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (bypassed.left.back(), 0.5f, 2.0e-3);
    }

    // Monitor Sidechain plays the sum of all sidechains (or silence without any).
    {
        Rig rig ("x");
        rig.parameters.monitorMode = 2;
        rig.parameters.attackMs = 0.5f;

        const auto withSidechain = runWithSidechain (rig.engine, rig.parameters, 0.5f,
                                                     0.3f, samplesForMs (50.0));
        CHECK_CLOSE (withSidechain.left.back(), 0.3f, 2.0e-3);

        const auto withoutSidechain = rig.run (0.5f, samplesForMs (50.0));
        CHECK_CLOSE (withoutSidechain.left.back(), 0.0f, 2.0e-3);
    }

    // Four distinct sidechain envelopes; the total s is their linear sum.
    {
        Rig rig ("x");
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values { 0.1f, 0.2f, 0.3f, 0.4f };
        const std::array<bool, 4> allPresent { true, true, true, true };

        rig.engine.setProgram (nullptr);
        runWithSidechains (rig.engine, rig.parameters, 0.5f, values, allPresent,
                           samplesForMs (500.0));

        CHECK_CLOSE (rig.engine.getSidechainEnvelope (0), 0.1f, 2.0e-3);
        CHECK_CLOSE (rig.engine.getSidechainEnvelope (1), 0.2f, 2.0e-3);
        CHECK_CLOSE (rig.engine.getSidechainEnvelope (2), 0.3f, 2.0e-3);
        CHECK_CLOSE (rig.engine.getSidechainEnvelope (3), 0.4f, 2.0e-3);
        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 1.0f, 5.0e-3);
    }

    // Each formula variable reads its own bus: s2 ignores a loud s1.
    {
        Rig rig ("x - s2 / 10");
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values { 0.8f, 0.2f, 0.0f, 0.0f };
        const std::array<bool, 4> present { true, true, false, false };

        const auto result = runWithSidechains (rig.engine, rig.parameters, 0.5f,
                                               values, present, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.48f, 2.0e-3);
        CHECK_CLOSE (result.right.back(), 0.48f, 2.0e-3);
    }

    // dBFS total: two -6 dB sidechains sum to 0 dB, not -12 dB.
    {
        Rig rig ("x - max(0, s + 60) / 4");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values { 0.5f, 0.5f, 0.0f, 0.0f };
        const std::array<bool, 4> present { true, true, false, false };

        const auto result = runWithSidechains (rig.engine, rig.parameters, 0.5f,
                                               values, present, samplesForMs (300.0));
        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 1.0f, 5.0e-3);
        // s = 0 dBFS, so the formula subtracts 15 dB from a -6 dB input.
        CHECK_CLOSE (result.left.back(), 0.5f * 0.177828f, 2.0e-3);
    }

    // One shared HPF runs on every bus: four DC inputs all lose their energy.
    {
        Rig rig ("x - s / 10");
        rig.parameters.attackMs = 0.5f;
        rig.parameters.scHpfHz = 200.0f;

        const std::array<float, 4> values { 0.5f, 0.5f, 0.5f, 0.5f };
        const std::array<bool, 4> allPresent { true, true, true, true };

        const auto result = runWithSidechains (rig.engine, rig.parameters, 0.5f,
                                               values, allPresent, samplesForMs (2000.0));
        CHECK_CLOSE (result.left.back(), 0.5f, 2.0e-3);
        CHECK_MESSAGE (rig.engine.getSidechainTotalEnvelope() < 1.0e-3f,
                       "HPF should remove DC from every sidechain bus, total "
                           + juce::String (rig.engine.getSidechainTotalEnvelope(), 6));
    }

    // Hold is tracked per bus: a held bus stays up while a silent one stays zero.
    {
        Rig rig ("x");
        rig.parameters.attackMs = 0.5f;
        rig.parameters.releaseMs = 50.0f;
        rig.parameters.holdMs = 100.0f;

        const std::array<float, 4> loudFirst { 0.5f, 0.0f, 0.0f, 0.0f };
        const std::array<bool, 4> firstPair { true, true, false, false };
        const std::array<float, 4> silent { 0.0f, 0.0f, 0.0f, 0.0f };

        runWithSidechains (rig.engine, rig.parameters, 0.5f, loudFirst, firstPair,
                           samplesForMs (300.0));
        runWithSidechains (rig.engine, rig.parameters, 0.5f, silent, firstPair,
                           samplesForMs (50.0));

        CHECK_MESSAGE (rig.engine.getSidechainEnvelope (0) > 0.45f,
                       "held bus should freeze its envelope");
        CHECK_CLOSE (rig.engine.getSidechainEnvelope (1), 0.0f, 1.0e-6);

        runWithSidechains (rig.engine, rig.parameters, 0.5f, silent, firstPair,
                           samplesForMs (100.0));
        CHECK_MESSAGE (rig.engine.getSidechainEnvelope (0) < 0.30f,
                       "held bus should release afterwards");
    }

    // Absent buses (OFF, or a bus the host disabled) are cleared immediately.
    {
        Rig rig ("x - s / 10");
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values { 0.5f, 0.5f, 0.5f, 0.5f };
        const std::array<bool, 4> allPresent { true, true, true, true };
        const std::array<float, 4> zeros {};

        runWithSidechains (rig.engine, rig.parameters, 0.5f, values, allPresent,
                           samplesForMs (300.0));
        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 2.0f, 1.0e-2);

        const std::array<bool, 4> nonePresent {};
        runWithSidechains (rig.engine, rig.parameters, 0.5f, zeros, nonePresent,
                           samplesForMs (1.0));

        for (int bus = 0; bus < 4; ++bus)
            CHECK_CLOSE (rig.engine.getSidechainEnvelope (bus), 0.0f, 1.0e-6);

        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 0.0f, 1.0e-6);

        // Re-enabling starts from silence rather than the stale level.
        const std::array<bool, 4> onlyFirst { true, false, false, false };
        runWithSidechains (rig.engine, rig.parameters, 0.5f, values, onlyFirst,
                           samplesForMs (1.0));
        CHECK (rig.engine.getSidechainEnvelope (0) < 0.6f);
        CHECK (rig.engine.getSidechainEnvelope (1) == 0.0f);
    }

    // Monitor Sidechain sums every post-HPF bus sample.
    {
        Rig rig ("x");
        rig.parameters.monitorMode = 2;
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values { 0.1f, 0.2f, 0.3f, 0.4f };
        const std::array<bool, 4> allPresent { true, true, true, true };

        const auto result = runWithSidechains (rig.engine, rig.parameters, 0.5f,
                                               values, allPresent, samplesForMs (50.0));
        CHECK_CLOSE (result.left.back(), 1.0f, 2.0e-3);
        CHECK_CLOSE (result.right.back(), 1.0f, 2.0e-3);
    }

    // Non-finite sidechain samples never reach the formula or the monitor.
    {
        Rig rig ("x - s / 10");
        rig.parameters.attackMs = 0.5f;

        const std::array<float, 4> values {
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),
            0.0f, 0.0f
        };
        const std::array<bool, 4> present { true, true, false, false };

        const auto result = runWithSidechains (rig.engine, rig.parameters, 0.5f,
                                               values, present, samplesForMs (100.0));
        CHECK (allFinite (result.left));
        CHECK (allFinite (result.right));
        CHECK_CLOSE (rig.engine.getSidechainTotalEnvelope(), 0.0f, 1.0e-6);
    }

    // Mix at zero is bit-transparent.
    {
        Rig rig ("0.1 * x");
        rig.parameters.mix = 0.0f;

        const auto result = rig.run (0.5f, 1024);
        CHECK_CLOSE (result.left.back(), 0.5, 1.0e-6);
    }

    // Makeup gain.
    {
        Rig rig ("0.5 * x");
        rig.parameters.makeupDb = 6.0206f; // x2

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.5, 2.0e-3);
    }

    // Bypass is bit-transparent.
    {
        Rig rig ("0.1 * x");
        rig.parameters.bypassed = true;

        const auto result = rig.run (0.7f, 1024);
        CHECK_CLOSE (result.left.back(), 0.7, 1.0e-6);
    }

    // Peak vs RMS detection.
    {
        const int count = samplesForMs (1000.0);
        std::vector<float> left (static_cast<std::size_t> (count));
        std::vector<float> right (static_cast<std::size_t> (count));

        for (int i = 0; i < count; ++i)
        {
            const float value = 0.5f * std::sin (
                2.0f * 3.14159265358979f * 1000.0f
                * static_cast<float> (i) / static_cast<float> (testSampleRate));

            left[static_cast<std::size_t> (i)] = value;
            right[static_cast<std::size_t> (i)] = value;
        }

        const auto processStereo = [&] (int detectorMode)
        {
            auto program = formula::compileExpression ("x");
            CHECK (program.ok);

            formula::CompressorEngine engine;
            engine.prepare (testSampleRate, 512);
            engine.setProgram (&program.program);

            formula::EngineParameters parameters;
            parameters.attackMs = 1.0f;
            parameters.detectorMode = detectorMode;
            parameters.meterActive = true;

            std::vector<float> l (left);
            std::vector<float> r (right);
            float* pointers[2] = { l.data(), r.data() };
            engine.process (pointers, 2, count, parameters);
            return engine.getEnvelopeLevel();
        };

        CHECK_CLOSE (processStereo (0), 0.488, 0.03);
        CHECK_CLOSE (processStereo (1), 0.35355, 0.02);
    }

    // RMS uses a 10 ms one-pole time constant: after one time constant the
    // mean square has reached 1 - e^-1 of a DC step.
    {
        Rig rig ("x");
        rig.parameters.detectorMode = 1;
        rig.parameters.attackMs = 0.1f;

        rig.run (0.5f, samplesForMs (10.0));
        CHECK_CLOSE (rig.engine.getEnvelopeLevel(),
                     0.5 * std::sqrt (1.0 - std::exp (-1.0)),
                     0.012);
    }

    // The GR meter reports the strongest reduction in the block, not the
    // last sample (which may already have recovered).
    {
        Rig rig ("min(x, 0.25)");
        rig.parameters.attackMs = 0.1f;
        rig.parameters.releaseMs = 5.0f;

        constexpr int count = 512;
        std::vector<float> left (count, 0.05f);
        std::vector<float> right (count, 0.05f);

        for (int i = 0; i < 64; ++i)
            left[static_cast<std::size_t> (i)]
                = right[static_cast<std::size_t> (i)] = 0.8f;

        float* pointers[2] = { left.data(), right.data() };
        rig.engine.process (pointers, 2, count, rig.parameters);

        const float* capture = rig.engine.getLastGainCapture();
        CHECK (capture != nullptr);
        CHECK (rig.engine.getGainCaptureSize() >= count);

        float minDb = std::numeric_limits<float>::infinity();

        for (int i = 0; i < count; ++i)
            minDb = std::min (minDb, 20.0f * std::log10 (
                std::max (1.0e-9f, std::abs (capture[i]))));

        CHECK (minDb < -6.0f);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), minDb, 0.01);
    }

    // Non-finite and extreme finite input must not propagate NaN/Inf.
    {
        Rig rig ("x");
        rig.parameters.attackMs = 0.5f;

        constexpr int count = 512;
        std::vector<float> left (count, 0.25f);
        std::vector<float> right (count, 0.25f);

        left[0] = std::numeric_limits<float>::quiet_NaN();
        left[1] = std::numeric_limits<float>::infinity();
        left[2] = -std::numeric_limits<float>::infinity();
        left[3] = 1.0e30f;
        right[0] = 1.0e30f;
        right[1] = std::numeric_limits<float>::quiet_NaN();
        right[2] = std::numeric_limits<float>::infinity();
        right[3] = -std::numeric_limits<float>::infinity();

        float* pointers[2] = { left.data(), right.data() };
        rig.engine.process (pointers, 2, count, rig.parameters);

        CHECK (allFinite (left));
        CHECK (allFinite (right));
        CHECK (std::isfinite (rig.engine.getEnvelopeLevel()));
        CHECK (std::isfinite (rig.engine.getGainReductionDb()));
        CHECK (std::isfinite (rig.engine.getInputLevelDb()));
        CHECK (std::isfinite (rig.engine.getOutputLevelDb()));
    }

    // Negative gain is polarity inversion, not silence.
    {
        Rig rig ("-0.5 * x");
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), -0.25, 2.0e-3);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), -6.0206, 0.1);
        CHECK (rig.engine.isGainInverted());
    }

    // A full inversion cancels the dry signal at 50% mix.
    {
        Rig rig ("-x");
        rig.parameters.attackMs = 0.5f;
        rig.parameters.mix = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.0, 1.0e-4);
    }

    // Zero output is true silence, not a -60 dB floor.
    {
        Rig rig ("0 * x");
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.0, 1.0e-6);
        CHECK (! rig.engine.isGainInverted());
    }

    // Positive gain above unity is reported as boost.
    {
        Rig rig ("2 * x");
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 1.0, 2.0e-3);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), 6.0206, 0.1);
        CHECK (! rig.engine.isGainInverted());
    }

    // dB domain: x and f(x) are both dBFS.
    {
        Rig rig ("x");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.5, 2.0e-3);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), 0.0, 0.1);
        CHECK (! rig.engine.isGainInverted());
    }

    {
        Rig rig ("x - 6");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.25, 2.0e-3);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), -6.0206, 0.1);
    }

    {
        Rig rig ("min(x, -12 + (x + 12) / 4)");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK_CLOSE (result.left.back(), 0.2983, 3.0e-3);
        CHECK_CLOSE (rig.engine.getGainReductionDb(), -4.485, 0.1);
    }

    {
        Rig rig ("x");
        rig.parameters.xDomain = 1;

        const auto result = rig.run (0.0f, 2048);
        CHECK (allFinite (result.left));
        CHECK_CLOSE (result.left.back(), 0.0, 1.0e-7);
    }

    {
        Rig rig ("log(x)");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (200.0));
        CHECK (allFinite (result.left));
        CHECK_CLOSE (result.left.back(), 0.5, 2.0e-3);
    }

    // -inf dB in dBFS mode means silence, not unity.
    {
        Rig rig ("-1 / 0");
        rig.parameters.xDomain = 1;
        rig.parameters.attackMs = 0.5f;

        const auto result = rig.run (0.5f, samplesForMs (50.0));
        CHECK (allFinite (result.left));
        CHECK_CLOSE (result.left.back(), 0.0, 1.0e-7);
    }

    // Extreme formulas stay finite and clamped.
    {
        Rig rig ("1 / x");
        auto silence = rig.run (0.0f, 1024);
        CHECK (allFinite (silence.left));

        auto loud = rig.run (0.5f, 4800);
        CHECK (allFinite (loud.left));
        CHECK (std::abs (loud.left.back()) < 16.0f);

        Rig heat ("exp(500)");
        auto hot = heat.run (0.5f, 4800);
        CHECK (allFinite (hot.left));
        CHECK (std::abs (hot.left.back()) <= 0.5f * 15.9f);

        Rig logarithm ("log(x)");
        auto logResult = logarithm.run (0.0f, 2048);
        CHECK (allFinite (logResult.left));
    }

    // Engine without a program is identity.
    {
        formula::CompressorEngine engine;
        engine.prepare (testSampleRate, 512);

        formula::EngineParameters parameters;
        parameters.meterActive = true;

        std::vector<float> left (512, 0.25f);
        std::vector<float> right (512, -0.25f);
        float* pointers[2] = { left.data(), right.data() };
        engine.process (pointers, 2, 512, parameters);

        CHECK_CLOSE (left.back(), 0.25, 1.0e-6);
        CHECK_CLOSE (right.back(), -0.25, 1.0e-6);
    }

    // Sample rates and block sizes stay stable.
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        const int blocks[] = { 32, 64, 512 };

        for (const double rate : rates)
        {
            for (const int blockSize : blocks)
            {
                Rig rig (ampClassic, rate);
                rig.parameters.attackMs = 2.0f;
                rig.parameters.releaseMs = 100.0f;

                const int total = juce::roundToInt (rate * 0.2);

                for (int position = 0; position < total; position += blockSize)
                {
                    const int size = juce::jmin (blockSize, total - position);

                    std::vector<float> left (static_cast<std::size_t> (size));
                    std::vector<float> right (static_cast<std::size_t> (size));

                    for (int i = 0; i < size; ++i)
                    {
                        const float value = 0.5f * std::sin (
                            2.0f * 3.14159265358979f * 220.0f
                            * static_cast<float> (position + i) / static_cast<float> (rate));
                        left[static_cast<std::size_t> (i)] = value;
                        right[static_cast<std::size_t> (i)] = value;
                    }

                    float* pointers[2] = { left.data(), right.data() };
                    rig.engine.process (pointers, 2, size, rig.parameters);

                    CHECK (allFinite (left));
                    CHECK (allFinite (right));
                }
            }
        }
    }
}
