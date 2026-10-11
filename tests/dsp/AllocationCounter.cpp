// SPDX-License-Identifier: AGPL-3.0-only
#include "AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(__SANITIZE_ADDRESS__)
#define FCM_COUNT_ALLOCATIONS 0
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define FCM_COUNT_ALLOCATIONS 0
#endif
#endif
#ifndef FCM_COUNT_ALLOCATIONS
#define FCM_COUNT_ALLOCATIONS 1
#endif

namespace
{
std::atomic<bool> gCounting{false};
std::atomic<int> gAllocations{0};
} // namespace

#if FCM_COUNT_ALLOCATIONS
void* operator new(std::size_t size)
{
    if (gCounting)
        ++gAllocations;
    if (void* pointer = std::malloc(size > 0 ? size : 1))
        return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size)
{
    return operator new(size);
}
void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}
#endif

namespace fcm::test
{

AllocationCounter::AllocationCounter()
{
    gAllocations = 0;
    gCounting = true;
}

AllocationCounter::~AllocationCounter()
{
    gCounting = false;
}

int AllocationCounter::count() const noexcept
{
    return gAllocations.load();
}

bool AllocationCounter::available() noexcept
{
    return FCM_COUNT_ALLOCATIONS != 0;
}

} // namespace fcm::test
