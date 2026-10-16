// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/engine/Engine.h"

#include "dsp/engine/SafetyLimiter.h"

#include <algorithm>
#include <cmath>

namespace fcm::dsp
{

namespace
{
constexpr double kOutputGainSmoothSeconds = 0.020;

// MIDI channel voice message status nibbles and the two controllers that silence the instrument.
constexpr juce::uint8 kStatusNoteOff = 0x80;
constexpr juce::uint8 kStatusNoteOn = 0x90;
constexpr juce::uint8 kStatusController = 0xB0;
constexpr juce::uint8 kControllerAllSoundOff = 120;
constexpr juce::uint8 kControllerAllNotesOff = 123;
} // namespace

void Engine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxBlockSize = spec.maximumBlockSize;
    numChannels = spec.numChannels;

    if (sineTable == nullptr)
        sineTable = WavetableData::makeSine();

    voice.prepare(sampleRate, sineTable.get());
    const EffectSpec fxSpec{sampleRate, static_cast<int>(maxBlockSize), static_cast<int>(numChannels)};
    pool.prepare(fxSpec);
    for (auto& rack : racks)
        rack.prepare(pool, fxSpec);

    // The mixer, the buses and their delays (docs/decisions/0010).
    const int chunk = std::max(1, static_cast<int>(maxBlockSize));
    const int longestLatency = pool.largestLatencySamples();
    mixer.prepare(sampleRate);
    for (auto& bus : buses)
        bus.prepare(sampleRate, chunk, longestLatency);
    mainDirectRing.prepare(longestLatency + 1);
    for (auto& sum : sums)
        sum.assign(static_cast<size_t>(chunk), 0.0f);
    punchWeights.assign(static_cast<size_t>(chunk), 0.0f);
    for (auto& busBuffer : busBuffers)
        busBuffer.setSize(2, chunk);
    glide.prepare(sampleRate);
    outputGain.reset(sampleRate, kOutputGainSmoothSeconds);

    reset();
    snapParameters = true;
    setParameters(parameters);
}

void Engine::reset() noexcept
{
    pool.reset();
    for (auto& rack : racks)
        rack.reset();
    mixer.reset();
    for (auto& bus : buses)
        bus.reset();
    mainDirectRing.reset();
    notes.allNotesOff();
    glide.reset();
    voice.reset();
    lastNote = -1;
    outputGain.setCurrentAndTargetValue(outputGain.getTargetValue());
    updatePitchTarget();
}

void Engine::setParameters(const EngineParameters& newParameters) noexcept
{
    parameters = newParameters;
    voice.setParameters(parameters, snapParameters);
    mixer.setParameters(parameters.mixer, snapParameters);
    for (size_t i = 0; i < buses.size(); ++i)
        buses[i].setParameters(parameters.buses[i], snapParameters);

    if (snapParameters)
        outputGain.setCurrentAndTargetValue(parameters.outputGain);
    else
        outputGain.setTargetValue(parameters.outputGain);

    snapParameters = false;
    updatePitchTarget();
}

void Engine::updatePitchTarget() noexcept
{
    int note = -1;
    if (!parameters.keyTrack)
        note = parameters.rootNote;
    else if (lastNote >= 0)
        note = lastNote;

    if (note >= 0)
        glide.setTarget(static_cast<float>(note), parameters.glideOn, parameters.glideMs);
}

void Engine::noteOn(int note, int velocity) noexcept
{
    notes.noteOn(note);
    lastNote = note;
    updatePitchTarget();

    const float velocity01 = static_cast<float>(velocity) / 127.0f;
    voice.trigger(1.0f - parameters.velocitySensitivity * (1.0f - velocity01));
}

void Engine::noteOff(int note) noexcept
{
    const int heldBefore = notes.heldCount();

    // Releasing the sounding note while an older one is held sends the tail pitch back to it
    // without retriggering.
    if (const auto previous = notes.noteOff(note))
    {
        lastNote = *previous;
        updatePitchTarget();
    }

    // Letting go of the last held key ends the kick with a short release, so the length of the
    // note sets the length of the tail. A stray note-off for a note that was not held does nothing.
    if (heldBefore > 0 && !notes.hasHeldNotes())
        voice.release();
}

void Engine::handleMidi(const juce::MidiMessageMetadata& event) noexcept
{
    // Read the raw bytes: building a juce::MidiMessage can allocate for long (sysex) messages.
    if (event.numBytes < 2 || event.data == nullptr)
        return;

    const juce::uint8 status = event.data[0] & 0xF0;
    const int data1 = event.data[1] & 0x7F;
    const int data2 = event.numBytes >= 3 ? (event.data[2] & 0x7F) : 0;

    if (status == kStatusNoteOn && data2 > 0)
        noteOn(data1, data2);
    else if (status == kStatusNoteOff || status == kStatusNoteOn)
        noteOff(data1);
    else if (status == kStatusController &&
             (data1 == kControllerAllNotesOff || data1 == kControllerAllSoundOff))
    {
        notes.allNotesOff();
        if (data1 == kControllerAllSoundOff)
            voice.reset();
        else
            voice.release();
    }
}

void Engine::renderStems(int offset, int count) noexcept
{
    Voice::Stems stems;
    std::array<float, kNumDestinations> routed{};
    for (int i = offset; i < offset + count; ++i)
    {
        float weight = 0.0f;
        voice.processStems(glide.next(), stems, weight);
        mixer.process(stems, routed);
        for (size_t d = 0; d < sums.size(); ++d)
            sums[d][static_cast<size_t>(i)] = routed[d];
        punchWeights[static_cast<size_t>(i)] = weight;
    }
}

void Engine::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (numSamples == 0 || channels == 0)
        return;

    // The envelope shapes are fixed for the whole block.
    voice.setShapes(ampSource != nullptr ? ampSource->acquire() : nullptr,
                    pitchSource != nullptr ? pitchSource->acquire() : nullptr);

    // Likewise the wavetables.
    std::array<const WavetableData*, kNumOscillators> tables{};
    for (size_t i = 0; i < kNumOscillators; ++i)
        tables[i] = wavetableSources[i] != nullptr ? wavetableSources[i]->acquire() : nullptr;
    voice.setWavetables(tables);
    voice.setTockSample(tockSource != nullptr ? tockSource->acquire() : nullptr);

    // What effects may want to know about the note: the pitch the tail is settling on, now.
    EffectContext context;
    context.noteSemitones =
        (glide.hasPitch() ? glide.value() : static_cast<float>(parameters.rootNote)) - 60.0f;
    context.tempoBpm = parameters.bpm;

    // The racks are read once for the whole block. An instance can be in one rack only: a rack that
    // claims one that an earlier rack has is treated as empty (it cannot happen through the plugin's
    // own state, only through a bug).
    std::array<std::uint64_t, kNumRacks> packed{};
    std::uint64_t inRack = 0;
    for (size_t r = 0; r < racks.size(); ++r)
    {
        packed[r] = racks[r].value().load();
        const auto mask = EffectRack::memberMask(packed[r], pool.instanceCount());
        if ((mask & inRack) != 0)
            packed[r] = 0;
        else
            inRack |= mask;
    }
    pool.setParameters(parameters.effects, parameters.oversamplingFactor, context, inRack);

    const int factor = parameters.oversamplingFactor;
    const int latency = pool.latencySamples(factor);
    const int chunkSize = std::max(1, static_cast<int>(maxBlockSize));
    const auto busIndex = [](RackId id) { return static_cast<size_t>(id); };

    // A host may hand over more samples than it announced: they are processed in chunks.
    int position = 0;
    auto event = midi.begin();
    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int end = std::min(start + chunkSize, numSamples);
        const int count = end - start;

        // The voice, with the MIDI events that fall in this chunk, into one mono sum per destination.
        int cursor = start;
        while (event != midi.end())
        {
            const auto metadata = *event;
            const int eventPosition = std::clamp(metadata.samplePosition, position, numSamples);
            if (eventPosition >= end && end < numSamples)
                break;
            const int inChunk = std::clamp(eventPosition, cursor, end);
            renderStems(cursor - start, inChunk - cursor);
            cursor = inChunk;
            position = eventPosition;
            handleMidi(metadata);
            ++event;
        }
        renderStems(cursor - start, end - cursor);

        // The buses: each takes its mono sum through its rack of effects.
        for (size_t b = 0; b < buses.size(); ++b)
        {
            auto& busBuffer = busBuffers[b];
            auto& rack = racks[busIndex(b == 0 ? RackId::busA : RackId::busB)];
            buses[b].begin(sums[b + 1].data(), punchWeights.data(), count, latency, busBuffer);
            juce::AudioBuffer<float> view(busBuffer.getArrayOfWritePointers(), 2, count);
            rack.process(pool, packed[b == 0 ? busIndex(RackId::busA) : busIndex(RackId::busB)], factor,
                         view);
        }

        // Main: what was sent straight to it, delayed to line up with the buses, plus the buses.
        for (int i = 0; i < count; ++i)
        {
            const float direct = mainDirectRing.process(sums[0][static_cast<size_t>(i)], latency);
            for (int c = 0; c < channels; ++c)
                buffer.getWritePointer(c)[start + i] = direct;
        }
        {
            std::array<float*, 2> pointers{};
            const int used = std::min(channels, 2);
            for (int c = 0; c < used; ++c)
                pointers[static_cast<size_t>(c)] = buffer.getWritePointer(c) + start;
            juce::AudioBuffer<float> mainView(pointers.data(), used, count);
            for (size_t b = 0; b < buses.size(); ++b)
            {
                juce::AudioBuffer<float> busView(busBuffers[b].getArrayOfWritePointers(), 2, count);
                buses[b].finish(busView, count, mainView);
            }

            // The Main rack; the first channels only, as before the racks were three.
            racks[busIndex(RackId::main)].process(pool, packed[busIndex(RackId::main)], factor, mainView);
        }
    }

    // The racks handle two channels; any further channel (not offered by the plugin's layouts, but
    // harmless to cover) repeats the first.
    for (int channel = 2; channel < channels; ++channel)
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);

    // Last of all, on every channel: the output gain and the safety limiter, so the limiter always
    // protects what leaves the plugin.
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = outputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            const float sample = data[i] * gain;
            data[i] = parameters.limiterOn ? SafetyLimiter::process(sample)
                                           : (std::isfinite(sample) ? sample : 0.0f);
        }
    }
}

} // namespace fcm::dsp
