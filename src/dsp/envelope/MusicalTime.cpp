// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/MusicalTime.h"

#include <cmath>
#include <numeric>

namespace fcm::dsp
{

std::vector<TimeChoice> timeChoices(float minMs, float maxMs, double bpm)
{
    std::vector<TimeChoice> choices;
    choices.push_back({minMs <= 0.0f ? "0" : "Min", minMs});

    for (const auto& figure : kMusicalFigures)
    {
        const auto ms = static_cast<float>(beatsToMs(figure.beats, bpm));
        if (ms > minMs && ms <= maxMs)
            choices.push_back({figure.name, ms});
    }
    return choices;
}

int nearestChoice(const std::vector<TimeChoice>& choices, float ms)
{
    int best = 0;
    float bestDistance = std::abs(choices.front().ms - ms);
    for (size_t i = 1; i < choices.size(); ++i)
    {
        const float distance = std::abs(choices[i].ms - ms);
        if (distance < bestDistance)
        {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

std::string formatBarFraction(double beats)
{
    // Sixty-fourths of a bar are the finest step of the figures, so every position is a whole number of them.
    const auto sixtyFourths = static_cast<long long>(std::llround(beats / 4.0 * 64.0));
    if (sixtyFourths <= 0)
        return "0";

    const long long divisor = std::gcd(sixtyFourths, 64LL);
    const long long numerator = sixtyFourths / divisor;
    const long long denominator = 64 / divisor;
    if (denominator == 1)
        return std::to_string(numerator);
    return std::to_string(numerator) + "/" + std::to_string(denominator);
}

} // namespace fcm::dsp
