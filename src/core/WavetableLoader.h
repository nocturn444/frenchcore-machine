// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/NoiseImport.h"
#include "core/TockImport.h"
#include "core/WavetableImport.h"

#include <atomic>
#include <functional>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <memory>

namespace fcm::core
{

// Reads and decodes a wavetable (or Tock sample) file on a background thread and hands the result to the
// message thread (docs/specs/wavetable-user.md §4). A file bigger than the limit is never read. If the loader
// is destroyed while a job is pending, the result is dropped instead of delivered.
class WavetableLoader
{
  public:
    using Callback = std::function<void(WavetableImportResult result, juce::String fileName)>;

    WavetableLoader();
    ~WavetableLoader();

    // Message thread. `done` is called later on the message thread, once per call.
    void load(const juce::File& file, Callback done);

    // The same for a Tock sample (docs/specs/tock.md §4.1), on the same background thread.
    using TockCallback = std::function<void(TockImportResult result, juce::String fileName)>;
    void loadTock(const juce::File& file, TockCallback done);

    // The same for a noise sample (docs/specs/noise-oscillator.md §4).
    using NoiseCallback = std::function<void(NoiseImportResult result, juce::String fileName)>;
    void loadNoise(const juce::File& file, NoiseCallback done);

  private:
    juce::ThreadPool pool{1};
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace fcm::core
