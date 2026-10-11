// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace fcm::core
{

// Version of the saved plugin state. Bump it and add a migration in StateMigrations when the
// state format changes (docs/ARCHITECTURE.md §5).
inline constexpr int kStateVersion = 1;

// Parameter IDs are permanent once released: never rename, remove or reuse one.
namespace ParameterIds
{
inline constexpr const char* kOutputGain = "global.outputGain";
} // namespace ParameterIds

} // namespace fcm::core
