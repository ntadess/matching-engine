#include <gtest/gtest.h>

#include "order_book.hpp"

using engine::OrderBook;
using engine::Side;

namespace {
OrderBook make_book() { return OrderBook(/*reference=*/10000, /*window=*/500); }
}  // namespace

TEST(OrderBookBasics, AddSingleBidBecomesBest) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);

    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 9995);
    EXPECT_FALSE(book.best_ask_price().has_value());
}

TEST(OrderBookBasics, HigherBidBecomesNewBest) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9998, 50);

    EXPECT_EQ(*book.best_bid_price(), 9998);
}

TEST(OrderBookBasics, LowerAskBecomesNewBest) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10005, 100);
    book.add_order(2, Side::Ask, 10002, 50);

    EXPECT_EQ(*book.best_ask_price(), 10002);
}

TEST(OrderBookBasics, SamePriceOrdersPreserveFIFO) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9995, 50);

    const auto& level = book.bid_level_at_price(9995);
    ASSERT_NE(level.head, nullptr);
    EXPECT_EQ(level.head->id, 1u);
    EXPECT_EQ(level.head->next->id, 2u);
    EXPECT_EQ(level.tail->id, 2u);
    EXPECT_EQ(level.total_quantity, 150u);
}

TEST(OrderBookCancel, CancelOnlyOrderEmptiesLevelAndClearsBest) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);

    ASSERT_TRUE(book.cancel_order(1));
    EXPECT_FALSE(book.best_bid_price().has_value());
    EXPECT_TRUE(book.bid_level_at_price(9995).empty());
}

TEST(OrderBookCancel, CancelHeadPatchesNewHead) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9995, 50);

    ASSERT_TRUE(book.cancel_order(1));

    const auto& level = book.bid_level_at_price(9995);
    EXPECT_EQ(level.head->id, 2u);
    EXPECT_EQ(level.head->prev, nullptr);
    EXPECT_EQ(level.total_quantity, 50u);
}

TEST(OrderBookCancel, CancelTailPatchesNewTail) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9995, 50);

    ASSERT_TRUE(book.cancel_order(2));

    const auto& level = book.bid_level_at_price(9995);
    EXPECT_EQ(level.tail->id, 1u);
    EXPECT_EQ(level.tail->next, nullptr);
    EXPECT_EQ(level.total_quantity, 100u);
}

TEST(OrderBookCancel, CancelMiddleOrderPatchesBothNeighbors) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9995, 50);
    book.add_order(3, Side::Bid, 9995, 25);

    ASSERT_TRUE(book.cancel_order(2));

    const auto& level = book.bid_level_at_price(9995);
    EXPECT_EQ(level.head->id, 1u);
    EXPECT_EQ(level.head->next->id, 3u);
    EXPECT_EQ(level.tail->prev->id, 1u);
    EXPECT_EQ(level.total_quantity, 125u);
}

TEST(OrderBookCancel, CancelUnknownIdReturnsFalse) {
    auto book = make_book();
    EXPECT_FALSE(book.cancel_order(999));
}

TEST(OrderBookCancel, CancelBestLevelScansOutwardToNextBest) {
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9990, 100);

    ASSERT_TRUE(book.cancel_order(1));

    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 9990);
}

TEST(OrderBookBounds, PriceOutsideWindowThrows) {
    auto book = make_book();
    EXPECT_THROW(book.add_order(1, Side::Bid, 9000, 100), std::out_of_range);
}

TEST(OrderBookResets, LevelResetOrderHeadTail){
    auto book = make_book();
    book.add_order(1, Side::Bid, 9995, 100);
    book.add_order(2, Side::Bid, 9995, 50);

    ASSERT_TRUE(book.cancel_order(1));
    ASSERT_TRUE(book.cancel_order(2));

    auto& level = book.bid_level_at_price(9995);
    EXPECT_EQ(level.head, nullptr);
    EXPECT_EQ(level.tail, nullptr);
    EXPECT_EQ(level.total_quantity, 0u);

    book.add_order(3, Side::Bid, 9995, 25);
    EXPECT_EQ(level.head->id, 3u);
}

TEST(OrderBookMatching, FullyFillsAgainstRestingOrderOfEqualQuantity) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10005, 100);

    book.add_order(2, Side::Bid, 10005, 100);

    EXPECT_FALSE(book.best_ask_price().has_value());
    EXPECT_FALSE(book.best_bid_price().has_value());
}

TEST(OrderBookMatching, PartialFillOfRestingOrderLeavesRemainder) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10005, 100);

    book.add_order(2, Side::Bid, 10005, 40);

    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(*book.best_ask_price(), 10005);
    EXPECT_EQ(book.ask_level_at_price(10005).total_quantity, 60u);
    EXPECT_FALSE(book.best_bid_price().has_value());
}

TEST(OrderBookMatching, IncomingOrderRestsWithLeftoverAfterPartialMatch) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10005, 50);

    book.add_order(2, Side::Bid, 10005, 80);

    EXPECT_FALSE(book.best_ask_price().has_value());
    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 10005);
    EXPECT_EQ(book.bid_level_at_price(10005).total_quantity, 30u);
}

TEST(OrderBookMatching, WalksMultipleLevelsUntilFilledOrNoLongerCrossing) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10001, 50);
    book.add_order(2, Side::Ask, 10002, 50);
    book.add_order(3, Side::Ask, 10003, 50);

    book.add_order(4, Side::Bid, 10002, 100);

    EXPECT_FALSE(book.bid_level_at_price(10002).total_quantity);
    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(*book.best_ask_price(), 10003);
    EXPECT_EQ(book.ask_level_at_price(10003).total_quantity, 50u);
}

TEST(OrderBookMatching, NonCrossingOrderJustRests) {
    auto book = make_book();
    book.add_order(1, Side::Ask, 10005, 100);

    book.add_order(2, Side::Bid, 9995, 50);

    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 9995);
    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(*book.best_ask_price(), 10005);
}

TEST(OrderBookRecenter, TriggersWhenBestIndexNearEdge){
    auto book = make_book();
    book.add_order(1, Side::Bid, 9500, 100);
    book.add_order(2, Side::Bid, 9000, 100); // not best bid wont change anything
    book.add_order(3, Side::Bid, 10000, 100); // best bid at edge triggers recenter

    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 10000);
    EXPECT_EQ(book.bid_level_at_price(10000).total_quantity, 100u);


}