// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "dsp/fx/EffectCurve.h"

namespace fcm::core
{

// The operations of the curve editor on a drawn curve (docs/specs/fx-volumelfo.md section 2), as plain
// functions so they are tested without a window. Every one leaves a valid curve (EffectCurve::isValid)
// whatever it is given: a position that is not a number, outside the curve or past a neighbour is
// brought back into range. `snap` is a step of the position in the cycle (0.25 for a quarter);
// 0 or less means no snapping.
namespace curve_edit
{

// The position (0..1) a drag lands on after snapping.
float snapped(float x, float snap) noexcept;

// Moves a point. The first and the last stay at the start and at the end of the cycle and only move
// up and down; any other point cannot pass its neighbours (it may reach their position: two points at
// one position make a vertical edge). Returns false for an index that is not a point.
bool movePoint(dsp::EffectCurve& curve, int index, float x, float y, float snap) noexcept;

// Adds a point at the position, between the points on either side of it. Returns the index of the new
// point, or -1 when the curve already has the most points.
int addPoint(dsp::EffectCurve& curve, float x, float y, float snap) noexcept;

// Deletes a point that is neither the first nor the last. Returns false when it cannot.
bool removePoint(dsp::EffectCurve& curve, int index) noexcept;

// Sets how the segment that leaves the point bends (-1 fast first to +1 slow first). Returns false for
// an index with no segment after it.
bool setBend(dsp::EffectCurve& curve, int index, float bend) noexcept;

} // namespace curve_edit

} // namespace fcm::core
