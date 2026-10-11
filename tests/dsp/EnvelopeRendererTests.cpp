// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/BreakpointEnvelope.h"
#include "dsp/envelope/EnvelopePresets.h"
#include "dsp/envelope/EnvelopeRenderer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <juce_core/juce_core.h>
#include <vector>

using Catch::Matchers::WithinAbs;
using fcm::dsp::BreakpointEnvelope;
using fcm::dsp::curveProgress;
using fcm::dsp::EnvelopeKind;
using fcm::dsp::EnvelopeRenderer;
using fcm::dsp::EnvelopeShape;

namespace
{
struct Rendered
{
    explicit Rendered(const BreakpointEnvelope& envelope, double newSampleRate = 48000.0,
                      float timeScale = 1.0f)
        : shape(envelope.toShape()), sampleRate(newSampleRate)
    {
        renderer.prepare(sampleRate);
        renderer.setShape(shape.get());
        renderer.start(timeScale);
    }

    std::vector<float> take(int samples)
    {
        std::vector<float> out(static_cast<size_t>(samples));
        for (auto& s : out)
            s = renderer.next();
        return out;
    }

    int msToSamples(double ms) const { return static_cast<int>(std::lround(ms * 0.001 * sampleRate)); }

    std::unique_ptr<const EnvelopeShape> shape;
    double sampleRate;
    EnvelopeRenderer renderer;
};
} // namespace

TEST_CASE("curveProgress is linear at 0 and bends in the right direction", "[dsp][envelope][shape]")
{
    for (float x : {0.0f, 0.25f, 0.5f, 1.0f})
        REQUIRE_THAT(curveProgress(x, 0.0f), WithinAbs(x, 1.0e-6));

    for (float curve : {-1.0f, -0.4f, 0.4f, 1.0f})
    {
        REQUIRE_THAT(curveProgress(0.0f, curve), WithinAbs(0.0, 1.0e-6));
        REQUIRE_THAT(curveProgress(1.0f, curve), WithinAbs(1.0, 1.0e-5));
        float previous = 0.0f;
        for (float x = 0.01f; x <= 1.0f; x += 0.01f)
        {
            const float y = curveProgress(x, curve);
            REQUIRE(y >= previous); // monotonic
            previous = y;
        }
    }

    REQUIRE(curveProgress(0.5f, -1.0f) > 0.5f); // fast first
    REQUIRE(curveProgress(0.5f, 1.0f) < 0.5f);  // slow first
}

TEST_CASE("The default pitch shape is the Phase 2 pitch sweep", "[dsp][envelope][shape]")
{
    const auto shape = BreakpointEnvelope(EnvelopeKind::pitch).toShape();

    // (e^(-t/tau) - e^(-T/tau)) / (1 - e^(-T/tau)) with T = 90 ms and T / tau = 5.
    const double floorValue = std::exp(-5.0);
    for (float t = 0.0f; t <= 90.0f; t += 3.0f)
    {
        const double expected = (std::exp(-5.0 * t / 90.0) - floorValue) / (1.0 - floorValue);
        REQUIRE_THAT(shape->valueAt(t), WithinAbs(expected, 1.0e-4));
    }
    REQUIRE(juce::exactlyEqual(shape->valueAt(90.0f), 0.0f));
    REQUIRE(juce::exactlyEqual(shape->valueAt(1000.0f), 0.0f));
}

TEST_CASE("valueAt hits the points, interpolates and handles jumps", "[dsp][envelope][shape]")
{
    const auto amp = BreakpointEnvelope(EnvelopeKind::amp).toShape();

    REQUIRE_THAT(amp->valueAt(0.0f), WithinAbs(0.0, 1.0e-6));
    REQUIRE_THAT(amp->valueAt(0.5f), WithinAbs(0.5, 1.0e-6)); // halfway up the 1 ms attack
    REQUIRE_THAT(amp->valueAt(1.0f), WithinAbs(1.0, 1.0e-6));
    REQUIRE_THAT(amp->valueAt(25.0f), WithinAbs(1.0, 1.0e-6));
    REQUIRE_THAT(amp->valueAt(52.5f), WithinAbs(0.7, 1.0e-5)); // halfway down to the dip (80 ms, 40 %)
    REQUIRE_THAT(amp->valueAt(80.0f), WithinAbs(0.4, 1.0e-6));
    REQUIRE_THAT(amp->valueAt(110.0f), WithinAbs(0.55, 1.0e-5));
    REQUIRE_THAT(amp->valueAt(140.0f), WithinAbs(0.7, 1.0e-6));
    REQUIRE_THAT(amp->valueAt(5000.0f), WithinAbs(0.7, 1.0e-6)); // held after the last point

    // Two points at the same time are a vertical jump: the later one wins from that time on.
    const auto pitch = BreakpointEnvelope(EnvelopeKind::pitch).toShape();
    REQUIRE_THAT(pitch->valueAt(0.0f), WithinAbs(1.0, 1.0e-6));
}

TEST_CASE("valueAt with the segment cache matches the plain lookup", "[dsp][envelope][shape]")
{
    for (auto kind : {EnvelopeKind::amp, EnvelopeKind::pitch})
        for (const auto& preset : fcm::dsp::EnvelopePresets::all(kind))
        {
            const auto shape = preset.envelope.toShape();
            int segment = 0;
            for (float t = 0.0f; t < 1200.0f; t += 0.37f)
                REQUIRE(juce::exactlyEqual(shape->valueAt(t, segment), shape->valueAt(t)));
        }
}

TEST_CASE("Amp renderer follows Kick + Bass at every sample rate", "[dsp][envelope][renderer]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        const BreakpointEnvelope amp(EnvelopeKind::amp);
        Rendered run(amp, sampleRate);
        const auto out = run.take(run.msToSamples(400));

        INFO("sampleRate " << sampleRate);
        const auto at = [&](double ms) { return out[static_cast<size_t>(run.msToSamples(ms))]; };

        REQUIRE_THAT(out.front(), WithinAbs(0.0, 1.0e-6));
        REQUIRE_THAT(at(1.0), WithinAbs(1.0, 0.03));
        REQUIRE_THAT(at(25.0), WithinAbs(1.0, 0.02));
        REQUIRE_THAT(at(80.0), WithinAbs(0.4, 0.02));
        REQUIRE_THAT(at(140.0), WithinAbs(0.7, 0.02));
        // The body is held for as long as the key is down.
        REQUIRE_THAT(at(399.0), WithinAbs(0.7, 1.0e-6));
        REQUIRE(run.renderer.isActive());
    }
}

TEST_CASE("Amp renderer release fades linearly from the current level", "[dsp][envelope][renderer]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        // Released early in the decay, in the attack, and in the held body.
        for (double releaseAtMs : {0.4, 40.0, 300.0})
        {
            const BreakpointEnvelope amp(EnvelopeKind::amp);
            Rendered run(amp, sampleRate);

            float level = 0.0f;
            for (int i = 0; i < run.msToSamples(releaseAtMs); ++i)
                level = run.renderer.next();
            REQUIRE(level > 0.0f);

            run.renderer.release();
            REQUIRE(run.renderer.isActive());

            const int releaseSamples = run.msToSamples(30.0);
            float previous = level;
            float firstStep = 0.0f;
            int samples = 0;
            while (run.renderer.isActive())
            {
                const float value = run.renderer.next();
                INFO("sampleRate " << sampleRate << " released at " << releaseAtMs << " ms");
                REQUIRE(value <= previous);
                if (samples == 0)
                    firstStep = previous - value;
                previous = value;
                REQUIRE(++samples <= releaseSamples + 2);
            }

            REQUIRE_THAT(firstStep, WithinAbs(level / releaseSamples, level * 0.01f));
            REQUIRE(samples >= releaseSamples - 2);
            REQUIRE(juce::exactlyEqual(previous, 0.0f));
        }
    }
}

TEST_CASE("Releasing twice does not restart the release, and releasing when idle does nothing",
          "[dsp][envelope][renderer]")
{
    EnvelopeRenderer idle;
    idle.prepare(48000.0);
    idle.release();
    REQUIRE_FALSE(idle.isActive());
    REQUIRE(juce::exactlyEqual(idle.next(), 0.0f));

    Rendered run{BreakpointEnvelope(EnvelopeKind::amp)};
    run.take(2000);
    run.renderer.release();
    const float first = run.renderer.next();
    run.renderer.release(); // must not recompute the step from the lower level
    const float second = run.renderer.next();
    const float third = run.renderer.next();
    REQUIRE_THAT(second - third, WithinAbs(first - second, 1.0e-6));
}

TEST_CASE("A sustain level of 0 ends the envelope by itself", "[dsp][envelope][renderer]")
{
    const auto& presets = fcm::dsp::EnvelopePresets::all(EnvelopeKind::amp);
    const BreakpointEnvelope shortPunch = presets[1].envelope;
    REQUIRE(shortPunch.knob(fcm::dsp::AmpKnob::sustain) < 1.0e-6f);

    Rendered run(shortPunch);
    const auto out = run.take(run.msToSamples(300));

    REQUIRE_FALSE(run.renderer.isActive());
    REQUIRE(juce::exactlyEqual(out.back(), 0.0f));
    // It ends at the last point (120 ms), not before.
    REQUIRE(out[static_cast<size_t>(run.msToSamples(100.0))] > 0.0f);
}

TEST_CASE("Time scale stretches every time, the release included", "[dsp][envelope][renderer]")
{
    const BreakpointEnvelope amp(EnvelopeKind::amp);
    Rendered normal(amp, 48000.0, 1.0f);
    Rendered doubled(amp, 48000.0, 2.0f);
    Rendered halved(amp, 48000.0, 0.5f);

    const auto a = normal.take(normal.msToSamples(200));
    const auto b = doubled.take(normal.msToSamples(400));
    const auto c = halved.take(normal.msToSamples(100));

    for (double ms : {5.0, 30.0, 60.0, 100.0})
    {
        const auto index = static_cast<size_t>(normal.msToSamples(ms));
        REQUIRE_THAT(b[2 * index], WithinAbs(a[index], 2.0e-3));
        REQUIRE_THAT(c[index / 2], WithinAbs(a[index], 2.0e-3));
    }

    doubled.renderer.release();
    int releaseSamples = 0;
    while (doubled.renderer.isActive())
    {
        doubled.renderer.next();
        ++releaseSamples;
    }
    REQUIRE(std::abs(releaseSamples - doubled.msToSamples(60.0)) <= 2);
}

TEST_CASE("Pitch renderer falls to the end level and holds it", "[dsp][envelope][renderer]")
{
    BreakpointEnvelope pitch(EnvelopeKind::pitch);
    pitch.setKnob(fcm::dsp::PitchKnob::end, 0.25f);

    Rendered run(pitch);
    const auto out = run.take(run.msToSamples(500));

    REQUIRE_THAT(out.front(), WithinAbs(1.0, 1.0e-5));
    REQUIRE_THAT(out.back(), WithinAbs(0.25, 1.0e-6));
    REQUIRE(run.renderer.isActive()); // a held level above 0 never ends
    for (size_t i = 1; i < out.size(); ++i)
        REQUIRE(out[i] <= out[i - 1] + 1.0e-6f); // falls, then holds
}

TEST_CASE("Pitch renderer with the default drop ends at exactly 0", "[dsp][envelope][renderer]")
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
    {
        Rendered run{BreakpointEnvelope(EnvelopeKind::pitch), sampleRate};
        const auto out = run.take(run.msToSamples(200));

        const auto dropEnd = static_cast<size_t>(run.msToSamples(90.0)) + 2;
        for (size_t i = dropEnd; i < out.size(); ++i)
            REQUIRE(juce::exactlyEqual(out[i], 0.0f));
    }
}

TEST_CASE("Renderer reset is deterministic and a missing shape is silent", "[dsp][envelope][renderer]")
{
    Rendered run{BreakpointEnvelope(EnvelopeKind::amp)};
    const auto first = run.take(5000);
    run.renderer.reset();
    REQUIRE_FALSE(run.renderer.isActive());
    run.renderer.start(1.0f);
    REQUIRE(run.take(5000) == first);

    EnvelopeRenderer empty;
    empty.prepare(48000.0);
    empty.start(1.0f);
    REQUIRE_FALSE(empty.isActive());
    REQUIRE(juce::exactlyEqual(empty.next(), 0.0f));
}

TEST_CASE("Swapping the shape while sounding continues from the same time", "[dsp][envelope][renderer]")
{
    BreakpointEnvelope amp(EnvelopeKind::amp);
    auto first = amp.toShape();
    amp.setKnob(fcm::dsp::AmpKnob::sustain, 0.3f);
    auto second = amp.toShape();

    EnvelopeRenderer renderer;
    renderer.prepare(48000.0);
    renderer.setShape(first.get());
    renderer.start(1.0f);

    float level = 0.0f;
    for (int i = 0; i < 12000; ++i) // 250 ms: in the held body
        level = renderer.next();
    REQUIRE_THAT(level, WithinAbs(0.7, 1.0e-6));

    renderer.setShape(second.get());
    REQUIRE_THAT(renderer.next(), WithinAbs(0.3, 1.0e-6)); // the new body level, no restart
    REQUIRE(renderer.isActive());
}
