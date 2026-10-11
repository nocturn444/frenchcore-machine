// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace fcm::dsp
{

// Hands an immutable object from the message thread to the audio thread without locks, and frees
// the old one safely (ADR 0006). The audio thread calls acquire() once per block; it never
// allocates, frees or waits. The publishing side may be any non-audio thread (a mutex serialises
// them; the audio thread never takes it).
//
// A hazard pointer tells the publisher which object the audio thread may still be reading: a
// retired object is freed only when it is not that one, so at most the object in use and the
// current one stay alive. Exactly one thread may call acquire().
template <typename T> class PublishedPointer
{
  public:
    PublishedPointer() = default;
    PublishedPointer(const PublishedPointer&) = delete;
    PublishedPointer& operator=(const PublishedPointer&) = delete;

    ~PublishedPointer()
    {
        delete current.load();
        for (const T* object : retired)
            delete object;
    }

    // Publisher side. Takes ownership of `next`.
    void publish(std::unique_ptr<const T> next)
    {
        const std::lock_guard<std::mutex> lock(publishMutex);

        const T* old = current.exchange(next.release());
        if (old != nullptr)
            retired.push_back(old);

        retired.erase(std::remove_if(retired.begin(), retired.end(),
                                     [this](const T* object)
                                     {
                                         if (object == hazard.load())
                                             return false;
                                         delete object;
                                         return true;
                                     }),
                      retired.end());
    }

    // Audio side. Returns nullptr until something has been published. The object stays valid until
    // the next call to acquire().
    const T* acquire() noexcept
    {
        const T* pointer;
        do
        {
            pointer = current.load();
            hazard.store(pointer);
        } while (current.load() != pointer);
        return pointer;
    }

    // For tests: how many objects are alive (the current one plus retired ones still in use).
    size_t aliveCount() const
    {
        const std::lock_guard<std::mutex> lock(publishMutex);
        return retired.size() + (current.load() != nullptr ? 1u : 0u);
    }

  private:
    std::atomic<const T*> current{nullptr};
    std::atomic<const T*> hazard{nullptr};
    mutable std::mutex publishMutex;
    std::vector<const T*> retired;
};

} // namespace fcm::dsp
