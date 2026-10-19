// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/fx/EffectRegistry.h"
#include "dsp/fx/compressor/CompressorEffect.h"
#include "dsp/fx/distortion/DistortionEffect.h"
#include "dsp/fx/eq/EqEffect.h"
#include "dsp/fx/filter/FilterEffect.h"
#include "dsp/fx/reverb/ReverbEffect.h"
#include "dsp/fx/stereo/StereoEffect.h"
#include "dsp/fx/transient/TransientEffect.h"

namespace fcm::dsp
{

// The only place that names effects: one line per effect, appended at the end. The order here is the
// default chain order and the index of the effect; never reshuffle it (docs/ADDING_AN_EFFECT.md).
void registerEffects(EffectRegistry& registry)
{
    registry.add<DistortionEffect>();
    registry.add<EqEffect>();
    registry.add<StereoEffect>();
    registry.add<FilterEffect>();
    registry.add<TransientEffect>();
    registry.add<CompressorEffect>();
    registry.add<ReverbEffect>();
}

} // namespace fcm::dsp
