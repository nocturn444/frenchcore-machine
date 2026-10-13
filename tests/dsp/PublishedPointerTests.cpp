// SPDX-License-Identifier: AGPL-3.0-only
#include "dsp/envelope/PublishedPointer.h"

#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using fcm::dsp::PublishedPointer;

namespace
{
std::atomic<int> liveObjects{0};

// A payload that tells when it is torn or freed: every slot holds the same number, and the
// destructor poisons it.
struct Payload
{
    explicit Payload(int number)
    {
        values.fill(number);
        ++liveObjects;
    }
    ~Payload()
    {
        values.fill(-1);
        --liveObjects;
    }

    bool isConsistent() const
    {
        for (int v : values)
            if (v != values[0] || v < 0)
                return false;
        return true;
    }

    std::array<int, 64> values{};
};
} // namespace

TEST_CASE("PublishedPointer is empty until something is published", "[dsp][publish]")
{
    PublishedPointer<Payload> publisher;
    REQUIRE(publisher.acquire() == nullptr);
    REQUIRE(publisher.aliveCount() == 0);
}

TEST_CASE("PublishedPointer returns the latest object", "[dsp][publish]")
{
    REQUIRE(liveObjects == 0);
    {
        PublishedPointer<Payload> publisher;
        publisher.publish(std::make_unique<Payload>(1));
        REQUIRE(publisher.acquire()->values[0] == 1);

        publisher.publish(std::make_unique<Payload>(2));
        REQUIRE(publisher.acquire()->values[0] == 2);
    }
    REQUIRE(liveObjects == 0); // destruction frees everything
}

TEST_CASE("PublishedPointer keeps the object in use alive and frees old ones later", "[dsp][publish]")
{
    REQUIRE(liveObjects == 0);
    PublishedPointer<Payload> publisher;

    publisher.publish(std::make_unique<Payload>(1));
    const Payload* inUse = publisher.acquire(); // the audio thread is reading object 1

    publisher.publish(std::make_unique<Payload>(2));
    publisher.publish(std::make_unique<Payload>(3));

    // Object 1 must survive while it is the hazard; object 2 was never in use and is gone.
    REQUIRE(inUse->isConsistent());
    REQUIRE(inUse->values[0] == 1);
    REQUIRE(liveObjects == 2); // 1 (in use) and 3 (current)
    REQUIRE(publisher.aliveCount() == 2);

    // Once the audio thread moves on, the next publish frees object 1.
    REQUIRE(publisher.acquire()->values[0] == 3);
    publisher.publish(std::make_unique<Payload>(4));
    REQUIRE(liveObjects == 2); // 3 (still the hazard) and 4
    REQUIRE(publisher.acquire()->values[0] == 4);
    publisher.publish(std::make_unique<Payload>(5));
    REQUIRE(liveObjects == 2); // 4 and 5
}

TEST_CASE("PublishedPointer never shows a torn or freed object across threads", "[dsp][publish]")
{
    REQUIRE(liveObjects == 0);
    constexpr int kPublishes = 20000;

    {
        PublishedPointer<Payload> publisher;
        publisher.publish(std::make_unique<Payload>(0));

        std::atomic<bool> done{false};
        std::atomic<bool> allGood{true};
        std::atomic<int> maxSeen{0};
        std::atomic<long long> acquires{0};
        std::atomic<bool> readerRunning{false};

        // The "audio thread": acquires like once per block and reads the whole object.
        std::thread reader(
            [&]
            {
                int lastSeen = 0;
                while (!done.load())
                {
                    const Payload* object = publisher.acquire();
                    if (object == nullptr || !object->isConsistent() || object->values[0] < lastSeen)
                        allGood = false;
                    else
                        lastSeen = object->values[0]; // never goes back to an older object
                    ++acquires;
                    readerRunning = true;
                }
                maxSeen = lastSeen;
            });

        // On a busy machine the reader thread may not start until the writer is done, which would
        // test nothing. Start publishing only once the reader is acquiring, and do not stop before
        // it has acquired again after the last publish.
        while (!readerRunning.load())
            std::this_thread::yield();

        for (int i = 1; i <= kPublishes; ++i)
            publisher.publish(std::make_unique<Payload>(i));

        const long long acquiresAfterLastPublish = acquires.load();
        while (acquires.load() <= acquiresAfterLastPublish)
            std::this_thread::yield();

        done = true;
        reader.join();

        REQUIRE(allGood.load());
        REQUIRE(acquires > 0);
        REQUIRE(publisher.acquire()->values[0] == kPublishes);
        REQUIRE(publisher.aliveCount() <= 2);
        REQUIRE(liveObjects <= 2);
    }
    REQUIRE(liveObjects == 0);
}
