// SPDX-License-Identifier: AGPL-3.0-only
#include "core/WavetableLoader.h"

namespace fcm::core
{

WavetableLoader::WavetableLoader() = default;

WavetableLoader::~WavetableLoader()
{
    alive->store(false);
    pool.removeAllJobs(true, 5000);
}

void WavetableLoader::load(const juce::File& file, Callback done)
{
    const auto flag = alive;
    pool.addJob(
        [flag, file, done = std::move(done)]
        {
            WavetableImportResult result;
            const juce::String name = sanitiseWavetableName(file.getFileNameWithoutExtension());

            if (!file.existsAsFile())
            {
                result.error = "The file does not exist";
            }
            else if (static_cast<juce::uint64>(file.getSize()) > kMaxWavetableFileBytes)
            {
                result.error = "The file is too big (at most 32 MB)";
            }
            else
            {
                juce::MemoryBlock bytes;
                if (file.loadFileAsData(bytes))
                    result = importWavetable(bytes.getData(), bytes.getSize());
                else
                    result.error = "The file could not be read";
            }

            juce::MessageManager::callAsync(
                [flag, done, result = std::move(result), name]() mutable
                {
                    if (flag->load())
                        done(std::move(result), name);
                });
        });
}

void WavetableLoader::loadTock(const juce::File& file, TockCallback done)
{
    const auto flag = alive;
    pool.addJob(
        [flag, file, done = std::move(done)]
        {
            TockImportResult result;
            const juce::String name = sanitiseWavetableName(file.getFileNameWithoutExtension());

            if (!file.existsAsFile())
            {
                result.error = "The file does not exist";
            }
            else if (static_cast<juce::uint64>(file.getSize()) > kMaxWavetableFileBytes)
            {
                result.error = "The file is too big (at most 32 MB)";
            }
            else
            {
                juce::MemoryBlock bytes;
                if (file.loadFileAsData(bytes))
                    result = importTockSample(bytes.getData(), bytes.getSize());
                else
                    result.error = "The file could not be read";
            }

            juce::MessageManager::callAsync(
                [flag, done, result = std::move(result), name]() mutable
                {
                    if (flag->load())
                        done(std::move(result), name);
                });
        });
}

} // namespace fcm::core
