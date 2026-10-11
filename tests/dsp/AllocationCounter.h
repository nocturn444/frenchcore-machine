// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

namespace fcm::test
{

// Counts heap allocations made through operator new while it is alive, to show that real-time code
// makes none. Only available in ordinary builds: under the address sanitizer replacing operator new
// clashes with the sanitizer's own, so `available()` is false there and the count stays 0 (the
// sanitizer run checks memory safety instead). JUCE's own containers allocate with malloc, which this
// does not see.
class AllocationCounter
{
  public:
    AllocationCounter();
    ~AllocationCounter();

    int count() const noexcept;
    static bool available() noexcept;
};

} // namespace fcm::test
