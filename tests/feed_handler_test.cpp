#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "feed_handler.hpp"
#include "messages.hpp"
#include "order_book.hpp"

using engine::AddMessage;
using engine::CancelMessage;
using engine::FeedHandler;
using engine::OrderBook;
using engine::Side;

namespace {

void send_udp(uint16_t port, const uint8_t* data, size_t len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(fd, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    ssize_t sent = sendto(fd, data, len, 0, reinterpret_cast<sockaddr*>(&addr),
                           sizeof(addr));
    ASSERT_EQ(sent, static_cast<ssize_t>(len));
    close(fd);
}

}  // namespace

TEST(FeedHandlerEndToEnd, ReceivesAddMessageAndUpdatesOrderBook) {
    constexpr uint16_t kPort = 45001;
    OrderBook book(10000, 500);
    FeedHandler handler(book, kPort);

    AddMessage msg;
    msg.sequence = 0;
    msg.id = 42;
    msg.side = Side::Bid;
    msg.price_ticks = 9995;
    msg.quantity = 100;

    uint8_t buffer[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(msg, buffer);
    send_udp(kPort, buffer, sizeof(buffer));

    handler.run_for(1);

    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), 9995);
}

TEST(FeedHandlerEndToEnd, ReceivesCancelMessageAndRemovesOrder) {
    constexpr uint16_t kPort = 45002;
    OrderBook book(10000, 500);
    book.add_order(7, Side::Bid, 9995, 100);

    FeedHandler handler(book, kPort);

    CancelMessage msg;
    msg.sequence = 0;
    msg.id = 7;

    uint8_t buffer[engine::CANCEL_MESSAGE_SIZE];
    engine::encode_cancel_message(msg, buffer);
    send_udp(kPort, buffer, sizeof(buffer));

    handler.run_for(1);

    EXPECT_FALSE(book.best_bid_price().has_value());
}

TEST(FeedHandlerEndToEnd, ProcessesMultipleMessagesInSequence) {
    constexpr uint16_t kPort = 45003;
    OrderBook book(10000, 500);
    FeedHandler handler(book, kPort);

    AddMessage add1;
    add1.sequence = 0;
    add1.id = 1;
    add1.side = Side::Ask;
    add1.price_ticks = 10005;
    add1.quantity = 50;

    AddMessage add2;
    add2.sequence = 1;
    add2.id = 2;
    add2.side = Side::Ask;
    add2.price_ticks = 10002;
    add2.quantity = 30;

    uint8_t buf1[engine::ADD_MESSAGE_SIZE];
    uint8_t buf2[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(add1, buf1);
    engine::encode_add_message(add2, buf2);

    send_udp(kPort, buf1, sizeof(buf1));
    handler.run_for(1);
    send_udp(kPort, buf2, sizeof(buf2));
    handler.run_for(1);

    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(*book.best_ask_price(), 10002);
}

TEST(FeedHandlerSequencing, DetectsGapWhenSequenceSkips) {
    constexpr uint16_t kPort = 45004;
    OrderBook book(10000, 500);
    FeedHandler handler(book, kPort);

    AddMessage msg;
    msg.sequence = 0;
    msg.id = 1;
    msg.side = Side::Bid;
    msg.price_ticks = 9995;
    msg.quantity = 100;

    uint8_t buf[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(msg, buf);
    send_udp(kPort, buf, sizeof(buf));
    handler.run_for(1);

    // Skip sequence 1 entirely -- jump straight to 2.
    msg.sequence = 2;
    msg.id = 2;
    engine::encode_add_message(msg, buf);
    send_udp(kPort, buf, sizeof(buf));
    handler.run_for(1);

    EXPECT_EQ(handler.gaps_detected(), 1u);
    EXPECT_EQ(handler.duplicates_dropped(), 0u);
    ASSERT_TRUE(book.best_bid_price().has_value());
}

TEST(FeedHandlerSequencing, DropsDuplicateMessage) {
    constexpr uint16_t kPort = 45005;
    OrderBook book(10000, 500);
    FeedHandler handler(book, kPort);

    AddMessage msg;
    msg.sequence = 0;
    msg.id = 5;
    msg.side = Side::Bid;
    msg.price_ticks = 9995;
    msg.quantity = 100;

    uint8_t buf[engine::ADD_MESSAGE_SIZE];
    engine::encode_add_message(msg, buf);

    send_udp(kPort, buf, sizeof(buf));
    handler.run_for(1);

    // Resend the exact same sequence number (simulates a retransmit/dup).
    send_udp(kPort, buf, sizeof(buf));
    handler.run_for(1);

    EXPECT_EQ(handler.duplicates_dropped(), 1u);
    EXPECT_EQ(handler.gaps_detected(), 0u);
}