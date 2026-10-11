// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PresetManager.h"

#include "core/FactoryPresets.h"

namespace fcm::core
{

PresetManager::PresetManager(ProjectState& projectState, PresetLibrary presetLibrary, juce::Identifier root)
    : project(projectState), library(std::move(presetLibrary)), rootType(std::move(root))
{
    // A new instance sounds like the first factory preset, Init (every control at its default).
    refresh();
    currentEntry = 0;
}

int PresetManager::numFactory() const noexcept
{
    return FactoryPresets::count();
}

void PresetManager::refresh()
{
    // Keep pointing at the same preset if it is still there, whatever the list did.
    const auto previouslyCurrent = currentEntry >= 0 && currentEntry < static_cast<int>(list.size())
                                       ? list[static_cast<size_t>(currentEntry)]
                                       : Entry{};
    list.clear();
    for (int i = 0; i < FactoryPresets::count(); ++i)
        list.push_back({FactoryPresets::name(i), true, {}});
    for (const auto& file : library.list())
        list.push_back({file.getFileNameWithoutExtension(), false, file});

    currentEntry = -1;
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].factory == previouslyCurrent.factory && list[i].name == previouslyCurrent.name &&
            previouslyCurrent.name.isNotEmpty())
            currentEntry = static_cast<int>(i);
}

bool PresetManager::load(int index)
{
    if (index < 0 || index >= static_cast<int>(list.size()))
    {
        message = "There is no such preset.";
        sendChangeMessage();
        return false;
    }

    const auto& entry = list[static_cast<size_t>(index)];
    juce::ValueTree state;
    if (entry.factory)
    {
        state = FactoryPresets::makeState(index, rootType);
    }
    else
    {
        const auto parsed = PresetLibrary::load(entry.file);
        if (!parsed.ok)
        {
            message = parsed.error;
            sendChangeMessage();
            return false;
        }
        state = parsed.state;
    }

    if (project.restore(state) == StateLoadResult::rejected)
    {
        message = "That file is not a Frenchcore Machine preset.";
        sendChangeMessage();
        return false;
    }

    current = entry.name;
    currentEntry = index;
    message.clear();
    sendChangeMessage();
    return true;
}

bool PresetManager::step(int direction)
{
    const int total = static_cast<int>(list.size());
    if (total == 0)
        return false;
    const int from = currentEntry >= 0 ? currentEntry : (direction > 0 ? -1 : 0);
    return load(((from + direction) % total + total) % total);
}

bool PresetManager::next()
{
    return step(1);
}

bool PresetManager::previous()
{
    return step(-1);
}

bool PresetManager::save(const juce::String& name)
{
    const auto result = library.save(name, project.capture());
    if (!result.ok)
    {
        message = result.message;
        sendChangeMessage();
        return false;
    }

    refresh();
    current = result.file.getFileNameWithoutExtension();
    for (size_t i = 0; i < list.size(); ++i)
        if (!list[i].factory && list[i].file == result.file)
            currentEntry = static_cast<int>(i);
    message.clear();
    sendChangeMessage();
    return true;
}

} // namespace fcm::core
