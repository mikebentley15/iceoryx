// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_utils/cxx/variant_queue.hpp"
#include "test.hpp"
#include "testutils/watch_dog.hpp"

using namespace ::testing;
using namespace iox;
using namespace iox::cxx;


class VariantQueue_test : public Test
{
  public:
    void SetUp()
    {
        m_watchdog.watchAndActOnFailure([] { std::terminate(); });
    }
    void TearDown()
    {
    }

    void PerformTestForQueueTypes(const std::function<void(uint64_t typeID)>& testCase) noexcept
    {
        for (uint64_t i = 0; i < numberOfQueueTypes; ++i)
            testCase(i);
    }

    // if a new fifo type is added this variable has to be adjusted
    uint64_t numberOfQueueTypes = 6U;
    Watchdog m_watchdog{units::Duration::fromSeconds(2U)};
};

TEST_F(VariantQueue_test, isEmptyWhenCreated)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        EXPECT_THAT(sut.empty(), Eq(true));
    });
}

TEST_F(VariantQueue_test, isNotEmptyWhenOneElementIsInside)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        sut.push(123);
        EXPECT_THAT(sut.empty(), Eq(false));
    });
}

TEST_F(VariantQueue_test, popsSingleElementWhichWasPushed)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        sut.push(4123);
        auto element = sut.pop();
        ASSERT_THAT(element.has_value(), Eq(true));
        EXPECT_THAT(element.value(), Eq(4123));
    });
}

TEST_F(VariantQueue_test, popsMultiElementsWhichWerePushed)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        sut.push(14123);
        sut.push(24123);
        sut.push(34123);

        auto element = sut.pop();
        ASSERT_THAT(element.has_value(), Eq(true));
        EXPECT_THAT(element.value(), Eq(14123));

        element = sut.pop();
        ASSERT_THAT(element.has_value(), Eq(true));
        EXPECT_THAT(element.value(), Eq(24123));

        element = sut.pop();
        ASSERT_THAT(element.has_value(), Eq(true));
        EXPECT_THAT(element.value(), Eq(34123));
    });
}

TEST_F(VariantQueue_test, pushTwoElementsAfterSecondPopIsInvalid)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        sut.push(14123);
        sut.push(24123);

        sut.pop();
        sut.pop();

        EXPECT_THAT(sut.pop().has_value(), Eq(false));
    });
}

TEST_F(VariantQueue_test, handlesOverflow)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueueTypes type = static_cast<VariantQueueTypes>(typeID);
        if (type == VariantQueueTypes::BlockingFiFo_MultiProducerSingleConsumer
            || type == VariantQueueTypes::BlockingFiFo_SingleProducerSingleConsumer)
        {
            return;
        }

        VariantQueue<int, 2> sut(type);
        // current SOFI can hold capacity +1 values, so push some more to ensure overflow
        sut.push(14123);
        sut.push(24123);
        sut.push(22222);
        sut.push(33333);
        auto maybeOverflowValue = sut.push(667);
        EXPECT_THAT(maybeOverflowValue.has_value(), Eq(true));
    });
}

TEST_F(VariantQueue_test, blocksOnOverflow)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueueTypes type = static_cast<VariantQueueTypes>(typeID);
        if (!(type == VariantQueueTypes::BlockingFiFo_MultiProducerSingleConsumer
              || type == VariantQueueTypes::BlockingFiFo_SingleProducerSingleConsumer))
        {
            return;
        }

        VariantQueue<int, 2> sut(type);

        for (uint64_t i = 0U; i < sut.capacity(); ++i)
        {
            sut.push(24123 + i);
        }

        std::atomic<uint64_t> counter{0U};
        std::thread t([&] {
            sut.push(1231);
            ++counter;
        });

        constexpr int64_t TIMEOUT_IN_MS = 100;
        std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_IN_MS));
        EXPECT_THAT(counter.load(), Eq(0U));
        auto result = sut.pop();
        ASSERT_THAT(result.has_value(), Eq(true));
        EXPECT_THAT(*result, Eq(24123));
        std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_IN_MS));
        EXPECT_THAT(counter.load(), Eq(1U));
        t.join();
    });
}

TEST_F(VariantQueue_test, noPopWhenEmpty)
{
    PerformTestForQueueTypes([](uint64_t typeID) {
        VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(typeID));
        EXPECT_THAT(sut.pop().has_value(), Eq(false));
    });
}

TEST_F(VariantQueue_test, underlyingTypeIsEmptyWhenCreated)
{
    VariantQueue<int, 5> sut(static_cast<VariantQueueTypes>(0));
    EXPECT_THAT(sut.getUnderlyingFiFo().template get_at_index<0>()->empty(), Eq(true));
}
