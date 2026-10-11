// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/EnvelopePresets.h"

namespace fcm::dsp
{

namespace
{
// timeMs, value, curve, isMain
constexpr EnvelopePoint p(float timeMs, float value, float curve, bool isMain)
{
    return {timeMs, value, curve, isMain};
}

std::vector<EnvelopePresets::Preset> makeAmpPresets()
{
    std::vector<EnvelopePresets::Preset> presets;

    // Kick + Bass (default, confirmed by the product owner): instant attack, 25 ms punch, quick
    // drop to 40 % by 80 ms, rise to a 70 % bass body by 140 ms held while the key is down.
    presets.push_back(
        {"Kick + Bass", EnvelopePresets::makeUnchecked(EnvelopeKind::amp,
                                                       {p(0, 0, 0, true), p(1, 1, 0, true), p(25, 1, 0, true),
                                                        p(80, 0.4f, 0, false), p(140, 0.7f, 0, true)},
                                                       30.0f)});

    // One-shot: a short punch that ends by itself.
    presets.push_back(
        {"Short Punch",
         EnvelopePresets::makeUnchecked(
             EnvelopeKind::amp,
             {p(0, 0, 0, true), p(1, 1, 0, true), p(15, 1, -0.8f, true), p(120, 0, 0, true)}, 20.0f)});

    // A long bass tail that ends by itself.
    presets.push_back({"Long Tail", EnvelopePresets::makeUnchecked(
                                        EnvelopeKind::amp,
                                        {p(0, 0, 0, true), p(1, 1, 0, true), p(30, 1, -0.5f, true),
                                         p(300, 0.45f, -0.6f, false), p(900, 0, 0, true)},
                                        40.0f)});
    return presets;
}

std::vector<EnvelopePresets::Preset> makePitchPresets()
{
    std::vector<EnvelopePresets::Preset> presets;

    // Classic Drop (default): starts at the full amount and falls onto the note in 90 ms with a
    // fast-then-slow curve; the same sweep as the Phase 2 placeholder.
    presets.push_back({"Classic Drop", EnvelopePresets::makeUnchecked(
                                           EnvelopeKind::pitch,
                                           {p(0, 0, 0, true), p(0, 1, -1, true), p(90, 0, 0, true)}, 30.0f)});

    presets.push_back({"Short Punch", EnvelopePresets::makeUnchecked(
                                          EnvelopeKind::pitch,
                                          {p(0, 0, 0, true), p(0, 1, -1, true), p(40, 0, 0, true)}, 30.0f)});

    presets.push_back(
        {"Long Tail",
         EnvelopePresets::makeUnchecked(
             EnvelopeKind::pitch, {p(0, 0, 0, true), p(0, 1, -0.6f, true), p(250, 0, 0, true)}, 30.0f)});
    return presets;
}
} // namespace

const std::vector<EnvelopePresets::Preset>& EnvelopePresets::all(EnvelopeKind kind)
{
    static const std::vector<Preset> amp = makeAmpPresets();
    static const std::vector<Preset> pitch = makePitchPresets();
    return kind == EnvelopeKind::amp ? amp : pitch;
}

} // namespace fcm::dsp
