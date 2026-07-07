#include <gtest/gtest.h>
#include "order_book.hpp"

TEST(OrderBookSanityCheck, PingReturnsExpectedValue) {
    engine::OrderBook book;
    EXPECT_EQ(book.ping(), 42);
}