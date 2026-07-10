// tests/spsc_queue_test.cpp
#include "spsc_queue.hpp"
#include "messages.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace engine;

// ---------- Single-threaded correctness ----------

TEST(SpscQueueTest, PushThenPopReturnsSameValue) {
    SpscQueue<int, 8> queue;

    ASSERT_TRUE(queue.push(42));
    auto item = queue.pop();

    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 42);
}

TEST(SpscQueueTest, PopFromEmptyReturnsNullopt) {
    SpscQueue<int, 8> queue;

    auto item = queue.pop();

    EXPECT_FALSE(item.has_value());
}

TEST(SpscQueueTest, PushUntilFullThenFails) {
    SpscQueue<int, 4> queue;

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_TRUE(queue.push(4));

    // capacity is 4, queue should now be full
    EXPECT_FALSE(queue.push(5));
}

TEST(SpscQueueTest, FifoOrderingHolds) {
    SpscQueue<int, 8> queue;

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(queue.push(i));
    }

    for (int i = 0; i < 5; ++i) {
        auto item = queue.pop();
        ASSERT_TRUE(item.has_value());
        EXPECT_EQ(*item, i);
    }
}

TEST(SpscQueueTest, WraparoundWorksCorrectly) {
    SpscQueue<int, 4> queue;

    // fill, drain some, push more so write_index wraps past capacity
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));

    auto a = queue.pop();
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(*a, 1);

    ASSERT_TRUE(queue.push(3));
    ASSERT_TRUE(queue.push(4));
    ASSERT_TRUE(queue.push(5));  // write_index now past Capacity, exercises the mask

    EXPECT_EQ(*queue.pop(), 2);
    EXPECT_EQ(*queue.pop(), 3);
    EXPECT_EQ(*queue.pop(), 4);
    EXPECT_EQ(*queue.pop(), 5);
}

// ---------- FeedMessage-specific single-threaded check ----------

TEST(SpscQueueTest, PushAndPopFeedMessage) {
    SpscQueue<FeedMessage, 8> queue;

    AddMessage add{};
    add.sequence = 1;
    add.id = 100;
    add.side = Side::Bid;   // confirm this matches your Side enum's actual value name
    add.price_ticks = 10000;
    add.quantity = 50;

    FeedMessage msg = add;
    ASSERT_TRUE(queue.push(msg));

    auto popped = queue.pop();
    ASSERT_TRUE(popped.has_value());
    ASSERT_TRUE(std::holds_alternative<AddMessage>(*popped));

    const auto& popped_add = std::get<AddMessage>(*popped);
    EXPECT_EQ(popped_add.sequence, 1u);
    EXPECT_EQ(popped_add.id, 100u);
    EXPECT_EQ(popped_add.price_ticks, 10000);
    EXPECT_EQ(popped_add.quantity, 50u);
}

// ---------- Concurrency ----------

TEST(SpscQueueTest, ConcurrentProducerConsumerPreservesOrder) {
    SpscQueue<int, 1024> queue;
    constexpr int N = 100000;

    std::vector<int> consumed;
    consumed.reserve(N);

    std::thread producer([&queue]() {
        for (int i = 0; i < N; ++i) {
            while (!queue.push(i)) {
                // spin: queue full, wait for consumer
            }
        }
    });

    std::thread consumer([&queue, &consumed]() {
        int received = 0;
        while (received < N) {
            auto item = queue.pop();
            if (item.has_value()) {
                consumed.push_back(*item);
                ++received;
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(consumed[i], i);
    }
}