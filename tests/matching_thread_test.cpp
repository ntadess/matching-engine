// tests/matching_thread_test.cpp
#include "matching_thread.hpp"
#include "spsc_queue.hpp"
#include "messages.hpp"
#include "order_book.hpp"

#include <gtest/gtest.h>
#include <chrono>      

using namespace engine;

TEST(MatchingThreadTest, ConsumesAddMessageAndAppliesToBook) {
    SpscQueue<FeedMessage, 1024> queue;
    OrderBook book(10000, 500);
    MatchingThread matcher(queue, book);

    matcher.start();

    AddMessage add{};
    add.sequence = 1;
    add.id = 1;
    add.side = Side::Bid;
    add.price_ticks = 10050;
    add.quantity = 10;
    ASSERT_TRUE(queue.push(FeedMessage{add}));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto best_bid = book.best_bid_price();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_EQ(*best_bid, 10050);

    matcher.stop();
}

TEST(MatchingThreadTest, ConsumesCancelMessageAndRemovesOrder) {
    SpscQueue<FeedMessage, 1024> queue;
    OrderBook book(10000, 500);
    MatchingThread matcher(queue, book);

    matcher.start();

    AddMessage add{};
    add.sequence = 1;
    add.id = 2;
    add.side = Side::Bid;
    add.price_ticks = 10075;
    add.quantity = 5;
    ASSERT_TRUE(queue.push(FeedMessage{add}));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // confirm the add actually landed before testing cancel
    auto best_bid_after_add = book.best_bid_price();
    ASSERT_TRUE(best_bid_after_add.has_value());
    EXPECT_EQ(*best_bid_after_add, 10075);

    CancelMessage cancel{};
    cancel.sequence = 2;
    cancel.id = 2;
    ASSERT_TRUE(queue.push(FeedMessage{cancel}));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto best_bid_after_cancel = book.best_bid_price();
    EXPECT_FALSE(best_bid_after_cancel.has_value());

    matcher.stop();
}

TEST(MatchingThreadTest, StopIsIdempotentAndSafeToCallTwice) {
    SpscQueue<FeedMessage, 1024> queue;
    OrderBook book(10000, 500);
    MatchingThread matcher(queue, book);

    matcher.start();
    matcher.stop();
    matcher.stop();  
}