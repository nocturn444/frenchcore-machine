// SPDX-License-Identifier: AGPL-3.0-only
#include "core/PresetLibrary.h"

#include <algorithm>

namespace fcm::core
{

juce::File PresetLibrary::defaultFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Frenchcore Machine")
        .getChildFile("Presets");
}

std::vector<juce::File> PresetLibrary::list() const
{
    std::vector<juce::File> files;
    if (!folder.isDirectory())
        return files;

    for (const auto& entry : folder.findChildFiles(juce::File::findFiles, false, "*"))
        if (entry.hasFileExtension(kPresetExtension) && !entry.isSymbolicLink())
            files.push_back(entry);

    std::sort(files.begin(), files.end(),
              [](const juce::File& a, const juce::File& b)
              {
                  const auto order = a.getFileName().compareIgnoreCase(b.getFileName());
                  return order != 0 ? order < 0 : a.getFileName() < b.getFileName();
              });
    if (files.size() > static_cast<size_t>(kMaxListed))
        files.resize(static_cast<size_t>(kMaxListed));
    return files;
}

bool PresetLibrary::exists(const juce::String& name) const
{
    const auto clean = sanitizePresetName(name);
    return clean.isNotEmpty() && folder.getChildFile(clean + kPresetExtension).existsAsFile();
}

PresetLibrary::SaveResult PresetLibrary::save(const juce::String& name, const juce::ValueTree& state) const
{
    SaveResult result;
    const auto clean = sanitizePresetName(name);
    if (clean.isEmpty())
    {
        result.message = "Give the preset a name with letters or numbers.";
        return result;
    }
    if (!state.isValid())
    {
        result.message = "There is nothing to save.";
        return result;
    }
    if (!folder.createDirectory().wasOk())
    {
        result.message = "Could not create the presets folder.";
        return result;
    }

    const auto target = folder.getChildFile(clean + kPresetExtension);
    // The name was sanitised, so this holds; it is checked anyway because a file name is the one
    // place user text reaches the file system.
    if (target.getParentDirectory() != folder || target.isSymbolicLink())
    {
        result.message = "That name cannot be used.";
        return result;
    }

    // Written to a temporary file next to the target and then moved over it, so a failed save never
    // leaves half a preset.
    juce::TemporaryFile temporary(target);
    if (!temporary.getFile().replaceWithText(encodePreset(clean, state), false, false, "\n") ||
        !temporary.overwriteTargetFileWithTemporary())
    {
        result.message = "Could not write the preset file.";
        return result;
    }

    result.ok = true;
    result.file = target;
    return result;
}

PresetParseResult PresetLibrary::load(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        PresetParseResult result;
        result.error = "That preset file does not exist.";
        return result;
    }
    if (file.getSize() > kMaxPresetBytes)
    {
        PresetParseResult result;
        result.error = "That preset file is too big.";
        return result;
    }
    return parsePreset(file.loadFileAsString());
}

} // namespace fcm::core
