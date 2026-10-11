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
    glide.prepare(sampleRate);
    outputGain.reset(sampleRate, kOutputGainSmoothSeconds);

    reset();
    snapParameters = true;
    setParameters(parameters);
}

void Engine::reset() noexcept
{
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

void Engine::renderSegment(float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float settleNote = glide.next();
        const float gain = outputGain.getNextValue();
        const float sample = voice.process(settleNote) * gain;

        if (parameters.limiterOn)
            output[i] = SafetyLimiter::process(sample);
        else
            output[i] = std::isfinite(sample) ? sample : 0.0f;
    }
}

void Engine::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();
    if (numSamples == 0 || channels == 0)
        return;

    // The voice is mono: render into the first channel and copy it to the others.
    float* output = buffer.getWritePointer(0);

    int position = 0;
    for (const auto event : midi)
    {
        const int eventPosition = std::clamp(event.samplePosition, position, numSamples);
        renderSegment(output + position, eventPosition - position);
        position = eventPosition;
        handleMidi(event);
    }
    renderSegment(output + position, numSamples - position);

    for (int channel = 1; channel < channels; ++channel)
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
}

} // namespace fcm::dsp
